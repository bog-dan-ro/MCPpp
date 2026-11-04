// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#pragma once

#include <mcp++/server.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/beast/http.hpp>
#include <algorithm>
#include <charconv>
#include <chrono>
#include <functional>
#include <memory>

namespace mcp {
using beast_request = boost::beast::http::request<boost::beast::http::string_body>;
using beast_response = boost::beast::http::response<boost::beast::http::string_body>;
using beast_handler = std::function<beast_response(const beast_request&)>;

// A pre-shared bearer credential policy, not an OAuth authorization server.
// Configure the expected token from a secret source; never put it in a URL.
class bearer_token_policy {
    std::string token_;
public:
    explicit bearer_token_policy(std::string token) : token_(std::move(token)) {
        if (token_.empty()) throw std::invalid_argument("Bearer token must not be empty");
        bool padding = false;
        for (unsigned char c : token_) {
            if (c == '=') { padding = true; continue; }
            if (padding || !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~' || c == '+' || c == '/'))
                throw std::invalid_argument("Invalid bearer token syntax");
        }
        if (token_.front() == '=') throw std::invalid_argument("Invalid bearer token syntax");
    }
    bool operator()(const beast_request& request) const {
        namespace http = boost::beast::http;
        if (request.count(http::field::authorization) != 1) return false;
        std::string_view authorization = request[http::field::authorization];
        auto split = authorization.find(' ');
        if (split == std::string_view::npos || !boost::beast::iequals(authorization.substr(0, split), "Bearer")) return false;
        auto supplied = authorization.substr(split + 1);
        std::size_t difference = supplied.size() ^ token_.size();
        // Avoid an early exit depending on the position of a matching prefix.
        for (std::size_t i = 0; i < token_.size(); ++i)
            difference |= static_cast<unsigned char>(token_[i]) ^
                static_cast<unsigned char>(i < supplied.size() ? supplied[i] : 0);
        return difference == 0;
    }
};

struct beast_options {
    std::string address = "127.0.0.1";
    std::uint16_t port = 8080; // Zero asks the OS for an available port.
    std::string path = "/mcp";
    std::vector<std::string> allowed_origins; // Absent Origin is allowed; present requires an exact match.
    std::uint64_t body_limit = 1024 * 1024;
    std::uint32_t header_limit = 16 * 1024;
    std::chrono::milliseconds io_timeout{15000};
    std::size_t connection_limit = 64;
    std::function<bool(const beast_request&)> authorize; // Optional application policy; no OAuth implementation.
    std::string authentication_challenge; // E.g. Bearer realm="mcp" on authorization denial.
};

struct beast_tls {
    std::string certificate_chain_file;
    std::string private_key_file;
};

namespace detail {
inline std::string_view trim_http(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.remove_suffix(1);
    return text;
}

inline bool accepts_media(const beast_request& request, std::string_view wanted) {
    for (const auto& header : request) {
        if (header.name() != boost::beast::http::field::accept) continue;
        std::string_view rest = header.value();
        while (!rest.empty()) {
            const auto comma = rest.find(',');
            auto entry = trim_http(rest.substr(0, comma));
            const auto semi = entry.find(';');
            const bool matches = boost::beast::iequals(trim_http(entry.substr(0, semi)), wanted);
            double quality = 1;
            while (entry.find(';') != std::string_view::npos) {
                entry.remove_prefix(entry.find(';') + 1);
                auto parameter = trim_http(entry.substr(0, entry.find(';')));
                auto equals = parameter.find('=');
                if (equals != std::string_view::npos && boost::beast::iequals(trim_http(parameter.substr(0, equals)), "q")) {
                    auto weight = trim_http(parameter.substr(equals + 1));
                    auto [end, ec] = std::from_chars(weight.data(), weight.data() + weight.size(), quality);
                    if (ec != std::errc{} || end != weight.data() + weight.size() || !(quality >= 0 && quality <= 1)) quality = 0;
                }
            }
            if (matches && quality > 0) return true;
            if (comma == std::string_view::npos) break;
            rest.remove_prefix(comma + 1);
        }
    }
    return false;
}

// Decode the standard MCP sentinel with strict, canonical Base64 padding.
inline std::optional<std::string> decode_header(std::string_view text) {
    for (unsigned char c : text) if (c < 0x20 || c > 0x7e) return std::nullopt;
    if (text != trim_http(text)) return std::nullopt;
    if (!text.starts_with("=?base64?") || !text.ends_with("?=")) return std::string(text);
    text = text.substr(9, text.size() - 11);
    if (text.size() % 4) return std::nullopt;
    auto digit = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    std::string result;
    for (std::size_t i = 0; i < text.size(); i += 4) {
        int a = digit(text[i]), b = digit(text[i + 1]);
        int c = text[i + 2] == '=' ? 0 : digit(text[i + 2]);
        int d = text[i + 3] == '=' ? 0 : digit(text[i + 3]);
        bool pad2 = text[i + 2] == '=', pad1 = text[i + 3] == '=';
        if (a < 0 || b < 0 || c < 0 || d < 0 || (pad2 && !pad1) ||
            ((pad1 || pad2) && i + 4 != text.size()) || (pad2 && (b & 15)) || (pad1 && !pad2 && (c & 3)))
            return std::nullopt;
        result += static_cast<char>((a << 2) | (b >> 4));
        if (!pad2) result += static_cast<char>((b << 4) | (c >> 2));
        if (!pad1) result += static_cast<char>((c << 6) | d);
    }
    return result;
}
} // namespace detail

// Independent of sockets and TLS, and of the selected JSON library.
template<json_backend B>
beast_handler make_beast_handler(const server<B>& dispatcher, beast_options options = {}) {
    using J = detail::json<B>;
    using V = typename B::value;
    namespace http = boost::beast::http;
    return [&dispatcher, options = std::move(options)](const beast_request& request) -> beast_response {
        auto reply = [&](http::status status, std::optional<V> body = std::nullopt) {
            beast_response response{status, request.version()};
            response.set(http::field::server, "MCP++/0.1.0");
            response.set(http::field::cache_control, "no-store");
            response.keep_alive(request.keep_alive());
            if (body) {
                response.set(http::field::content_type, "application/json");
                response.body() = B::dump(*body);
            }
            response.prepare_payload();
            return response;
        };
        auto rpc_error = [&](int code, std::string_view message, const V* id = nullptr) {
            auto body = J::obj({{"jsonrpc", J::string("2.0")}, {"error", J::obj({
                {"code", J::integer(code)}, {"message", J::string(message)}})}});
            if (id && (J::is(*id, json_kind::string) || J::integer_kind(*id))) J::set(body, "id", *id);
            return body;
        };
        auto dispatched = [&](const V& message) {
            auto result = dispatcher.dispatch(message);
            if (!result) return reply(http::status::accepted);
            auto status = http::status::ok;
            if (const auto* error = J::find(*result, "error")) {
                auto code = J::as_i64(*J::find(*error, "code"));
                status = code == -32601 ? http::status::not_found :
                    code == -32603 ? http::status::internal_server_error : http::status::bad_request;
            }
            return reply(status, std::move(result));
        };
        try {
            if (request.count(http::field::origin)) {
                std::string origin(request[http::field::origin]);
                if (request.count(http::field::origin) != 1 ||
                    std::find(options.allowed_origins.begin(), options.allowed_origins.end(), origin) == options.allowed_origins.end())
                    return reply(http::status::forbidden);
            }
            if (request.target() != options.path) return reply(http::status::not_found);
            if (request.method() != http::verb::post) {
                auto response = reply(http::status::method_not_allowed);
                response.set(http::field::allow, "POST");
                return response;
            }
            if (request.version() != 11) return reply(http::status::http_version_not_supported);
            if (request.count(http::field::host) != 1 || request[http::field::host].empty())
                return reply(http::status::bad_request);
            if (options.authorize && !options.authorize(request)) {
                auto response = reply(http::status::unauthorized);
                if (!options.authentication_challenge.empty())
                    response.set(http::field::www_authenticate, options.authentication_challenge);
                return response;
            }
            if (request.body().size() > options.body_limit) return reply(http::status::payload_too_large);
            std::string_view content_type = request[http::field::content_type];
            if (request.count(http::field::content_type) != 1 ||
                !boost::beast::iequals(detail::trim_http(content_type.substr(0, content_type.find(';'))), "application/json"))
                return reply(http::status::unsupported_media_type);
            if (request.count(http::field::content_encoding) && request[http::field::content_encoding] != "identity")
                return reply(http::status::unsupported_media_type);
            if (!detail::accepts_media(request, "application/json") || !detail::accepts_media(request, "text/event-stream"))
                return reply(http::status::not_acceptable);

            V body;
            try { body = B::parse(request.body()); }
            catch (const parse_error&) { return reply(http::status::bad_request, rpc_error(-32700, "Parse error")); }
            auto id = J::find(body, "id");
            // Malformed envelopes are sent to the dispatcher only when they
            // cannot execute a method; valid requests first pass all mirrors.
            if (!J::is(body, json_kind::object) || !J::string_field(body, "jsonrpc") ||
                J::as_string(*J::find(body, "jsonrpc")) != "2.0" || !J::string_field(body, "method") ||
                (id && !J::is(*id, json_kind::string) && !J::integer_kind(*id))) return dispatched(body);
            if (!id) return reply(http::status::accepted); // No notification handlers.
            auto params = J::find(body, "params");
            auto meta = params ? J::find(*params, "_meta") : nullptr;
            constexpr auto version_key = "io.modelcontextprotocol/protocolVersion";
            if (!meta || !J::string_field(*meta, version_key)) return dispatched(body);
            auto matches = [&](std::string_view header, std::string_view expected, bool encoded = false) {
                if (request.count(header) != 1) return false;
                auto value = std::string_view(request[header]);
                if (encoded) { auto decoded = detail::decode_header(value); return decoded && *decoded == expected; }
                for (unsigned char c : value) if (c < 0x21 || c > 0x7e) return false;
                return value == expected;
            };
            auto method = J::as_string(*J::find(body, "method"));
            if (!matches("MCP-Protocol-Version", J::as_string(*J::find(*meta, version_key))) || !matches("Mcp-Method", method))
                return reply(http::status::bad_request, rpc_error(-32020, "Missing or mismatched MCP headers", id));
            std::string_view name_key;
            if (method == "tools/call" || method == "prompts/get") name_key = "name";
            if (method == "resources/read") name_key = "uri";
            if (!name_key.empty()) {
                if (!J::string_field(*params, name_key)) return dispatched(body);
                if (!matches("Mcp-Name", J::as_string(*J::find(*params, name_key)), true))
                    return reply(http::status::bad_request, rpc_error(-32020, "Missing or mismatched Mcp-Name header", id));
            }
            return dispatched(body);
        } catch (...) {
            return reply(http::status::internal_server_error, rpc_error(-32603, "Internal error"));
        }
    };
}

// Owns the acceptor and active connections. Dispatcher must outlive the listener
// and all callbacks. Run its io_context on one thread. Each listener executes
// tool calls on one worker; do not access its dispatcher concurrently elsewhere.
// stop() closes idle connections, rejects queued work and drains the active call.
class beast_listener {
    struct impl;
    std::shared_ptr<impl> impl_;
    beast_listener(boost::asio::io_context&, beast_handler, beast_options, std::optional<beast_tls>);
public:
    template<json_backend B>
    beast_listener(boost::asio::io_context& io, const server<B>& dispatcher,
                   beast_options options = {}, std::optional<beast_tls> tls = std::nullopt)
        : beast_listener(io, make_beast_handler(dispatcher, options), options, std::move(tls)) {}
    ~beast_listener();
    beast_listener(const beast_listener&) = delete;
    beast_listener& operator=(const beast_listener&) = delete;
    std::uint16_t port() const;
    void stop();
};
} // namespace mcp

#include <mcp++/detail/beast_transport.hpp>
