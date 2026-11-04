// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#pragma once
#include <mcp++/bind.hpp>
#include <mcp++/stdio.hpp>
#include <functional>
#include <map>
#include <optional>

namespace mcp {
struct protocol_error : std::runtime_error {
    int code;
    protocol_error(int c, std::string message) : std::runtime_error(std::move(message)), code(c) {}
};

template<json_backend B>
class server {
public:
    using value = typename B::value;
private:
    using J = detail::json<B>;
    struct entry {
        value descriptor;
        std::function<value(const value&)> invoke;
    };
    std::string name_, version_;
    std::map<std::string, entry, std::less<>> tools_;
    static constexpr std::string_view protocol = "2026-07-28";
    static constexpr std::string_view version_key = "io.modelcontextprotocol/protocolVersion";
    static constexpr std::string_view caps_key = "io.modelcontextprotocol/clientCapabilities";
    static constexpr std::string_view client_key = "io.modelcontextprotocol/clientInfo";

    static void require(bool condition, std::string_view message) {
        if (!condition) throw protocol_error(-32602, std::string(message));
    }
    value info() const {
        return J::obj({{"io.modelcontextprotocol/serverInfo", J::obj({
            {"name", J::string(name_)}, {"version", J::string(version_)}})}});
    }
    static value error(const value* id, int code, std::string_view message) {
        auto result = J::obj({{"jsonrpc", J::string("2.0")},
            {"error", J::obj({{"code", J::integer(code)}, {"message", J::string(message)}})}});
        // MCP 2026-07-28 forbids null IDs; omit an unreadable/invalid ID.
        if (id) J::set(result, "id", *id);
        return result;
    }
    static void validate_capabilities(const value& caps) {
        require(J::is(caps, json_kind::object), "clientCapabilities must be an object");
        for (auto key : {"elicitation", "sampling", "roots", "extensions"}) {
            if (auto cap = J::find(caps, key))
                require(J::is(*cap, json_kind::object), "Client capability settings must be objects");
        }
        if (auto roots = J::find(caps, "roots")) {
            if (auto changed = J::find(*roots, "listChanged"))
                require(J::is(*changed, json_kind::boolean), "roots.listChanged must be boolean");
        }
        if (auto extensions = J::find(caps, "extensions")) {
            for (const auto& key : J::keys(*extensions))
                require(J::is(*J::find(*extensions, key), json_kind::object), "Extension settings must be objects");
        }
    }
    static const value& metadata(const value& params) {
        auto meta = J::find(params, "_meta");
        require(meta && J::is(*meta, json_kind::object), "params._meta is required and must be an object");
        require(J::string_field(*meta, version_key), "Protocol version metadata is required and must be a string");
        auto caps = J::find(*meta, caps_key);
        require(caps != nullptr, "Client capabilities metadata is required");
        validate_capabilities(*caps);
        if (auto client = J::find(*meta, client_key)) {
            require(J::is(*client, json_kind::object) && J::string_field(*client, "name") &&
                    J::string_field(*client, "version"), "clientInfo must contain name and version strings");
        }
        if (auto token = J::find(*meta, "progressToken"))
            require(J::is(*token, json_kind::string) || J::integer_kind(*token), "Invalid progressToken");
        if (auto level = J::find(*meta, "io.modelcontextprotocol/logLevel")) {
            require(J::is(*level, json_kind::string), "Invalid logLevel");
            bool valid = false;
            for (auto known : {"debug", "info", "notice", "warning", "error", "critical", "alert", "emergency"})
                if (J::as_string(*level) == known) valid = true;
            require(valid, "Invalid logLevel");
        }
        return *meta;
    }
    value route(std::string_view method, const value& params) const {
        if (method == "server/discover") {
            return J::obj({{"resultType", J::string("complete")},
                {"supportedVersions", J::arr({J::string(protocol)})},
                {"capabilities", J::obj({{"tools", J::object()}})},
                {"ttlMs", J::integer(0)}, {"cacheScope", J::string("private")}});
        }
        if (method == "tools/list") {
            require(!J::find(params, "cursor"), "Invalid cursor: this server returns all tools in one page");
            auto result = J::obj({{"resultType", J::string("complete")}, {"tools", list_tools()},
                {"ttlMs", J::integer(0)}, {"cacheScope", J::string("private")}});
            return result;
        }
        if (method == "tools/call") {
            require(J::string_field(params, "name"), "Tool name is required and must be a string");
            auto args = J::find(params, "arguments");
            require(!args || J::is(*args, json_kind::object), "Tool arguments must be an object");
            // This server does not issue MRTR continuations.
            require(!J::find(params, "inputResponses") && !J::find(params, "requestState"),
                    "Tool continuations are unsupported");
            return call_tool(J::as_string(*J::find(params, "name")), args ? *args : J::object());
        }
        throw protocol_error(-32601, "Method not found");
    }

public:
    explicit server(std::string name, std::string version = "0.1.0")
        : name_(std::move(name)), version_(std::move(version)) {}

    // Registered objects must outlive the server and every call.
    template<class T>
    void expose(T& object) {
        static_assert(!std::is_const_v<T>, "MCP++: expose a non-const object (const methods are supported)");
        std::map<std::string, entry, std::less<>> pending;
        template for (constexpr auto m : describe<T>()) {
            const std::string name(std::meta::identifier_of(m));
            if (tools_.contains(name)) throw std::invalid_argument("Duplicate MCP tool name: " + name);
            auto descriptor = J::obj({{"name", J::string(name)}, {"inputSchema", input_schema<m, B>()}});
            constexpr auto annotations = std::define_static_array(std::meta::annotations_of_with_type(m, ^^tool_t));
            constexpr auto annotation = std::meta::extract<tool_t>(annotations[0]);
            constexpr auto description_size = [](const auto& text) {
                std::size_t length = 0;
                while (length < sizeof(text) && text[length] != '\0') ++length;
                return length;
            }(annotation.description);
            if constexpr (description_size != 0)
                J::set(descriptor, "description", J::string(std::string_view(annotation.description, description_size)));
            pending.emplace(name, entry{
                std::move(descriptor),
                [&object](const value& args) { return detail::call<m, B>(object, args); }});
        }
        tools_.merge(pending);
    }
    value list_tools() const {
        auto result = J::array();
        for (const auto& [name, tool] : tools_) J::append(result, tool.descriptor);
        return result;
    }
    value call_tool(std::string_view name, const value& args) const {
        auto it = tools_.find(name);
        if (it == tools_.end()) throw protocol_error(-32602, "Unknown tool");
        return it->second.invoke(args);
    }

    std::optional<value> dispatch(const value& request) const {
        if (!J::is(request, json_kind::object)) return error(nullptr, -32600, "Invalid request");
        auto id = J::find(request, "id");
        const bool valid_id = id && (J::is(*id, json_kind::string) || J::integer_kind(*id));
        const bool envelope = J::string_field(request, "jsonrpc") &&
            J::as_string(*J::find(request, "jsonrpc")) == "2.0" && J::string_field(request, "method");
        if (!envelope || (id && !valid_id)) return error(valid_id ? id : nullptr, -32600, "Invalid request");
        // This server has no notification handlers. Never reply to notifications.
        if (!id) return std::nullopt;
        try {
            auto params = J::find(request, "params");
            require(params && J::is(*params, json_kind::object), "Request params must be an object with _meta");
            const auto& meta = metadata(*params);
            const auto requested = J::as_string(*J::find(meta, version_key));
            if (requested != protocol) {
                auto response = error(id, -32022, "Unsupported protocol version");
                auto e = *J::find(response, "error");
                J::set(e, "data", J::obj({{"supported", J::arr({J::string(protocol)})}, {"requested", J::string(requested)}}));
                J::set(response, "error", std::move(e));
                return response;
            }
            auto result = route(J::as_string(*J::find(request, "method")), *params);
            J::set(result, "_meta", info());
            return J::obj({{"jsonrpc", J::string("2.0")}, {"id", *id}, {"result", std::move(result)}});
        } catch (const protocol_error& e) {
            return error(id, e.code, e.what());
        } catch (...) {
            return error(id, -32603, "Internal error");
        }
    }
    std::optional<value> process_line(std::string_view line) const {
        try { return dispatch(B::parse(line)); }
        catch (const parse_error&) { return error(nullptr, -32700, "Parse error"); }
    }
    int run(stdio streams = {}) const {
        return detail::run_stdio<B>(*this, streams);
    }
};
} // namespace mcp
