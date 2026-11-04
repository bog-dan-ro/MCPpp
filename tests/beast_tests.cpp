// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#define BOOST_TEST_MODULE mcpp_beast
#include <boost/test/unit_test.hpp>
#include <mcp++/beast.hpp>
#include "backend.hpp"

namespace http = boost::beast::http;

Value message(std::string_view method = "tools/call") {
    auto meta = J::obj({{"io.modelcontextprotocol/protocolVersion", J::string("2026-07-28")},
        {"io.modelcontextprotocol/clientCapabilities", J::object()}});
    auto params = J::obj({{"_meta", std::move(meta)}, {"name", J::string("add")},
        {"arguments", J::obj({{"a", J::integer(2)}, {"b", J::integer(3)}})}});
    return J::obj({{"jsonrpc", J::string("2.0")}, {"id", J::string("http-1")},
        {"method", J::string(method)}, {"params", std::move(params)}});
}

mcp::beast_request request(std::string_view method = "tools/call") {
    mcp::beast_request req{http::verb::post, "/mcp", 11};
    req.set(http::field::host, "localhost");
    req.set(http::field::content_type, "application/json; charset=utf-8");
    req.set(http::field::accept, "application/json, text/event-stream");
    req.set("MCP-Protocol-Version", "2026-07-28");
    req.set("Mcp-Method", method);
    if (method == "tools/call") req.set("Mcp-Name", "add");
    req.body() = J::dump(message(method));
    req.prepare_payload();
    return req;
}

int rpc_code(const mcp::beast_response& response) {
    auto body = J::parse(response.body());
    return static_cast<int>(J::as_i64(field(field(body, "error"), "code")));
}

BOOST_AUTO_TEST_CASE(success_and_error_mapping) {
    Sample obj;
    mcp::server<Backend> dispatcher("http");
    dispatcher.expose(obj);
    auto handle = mcp::make_beast_handler(dispatcher);
    auto response = handle(request());
    BOOST_TEST(response.result_int() == 200u);
    auto body = J::parse(response.body());
    BOOST_TEST(J::as_string(field(body, "id")) == "http-1");
    BOOST_TEST(J::as_i64(field(field(field(body, "result"), "structuredContent"), "result")) == 5);
    BOOST_TEST(obj.calls == 1);
    BOOST_TEST(response.keep_alive());
    BOOST_TEST(std::string(response[http::field::content_type]) == "application/json");
    BOOST_TEST(handle(request("unknown/rpc")).result_int() == 404u);
    auto bad = request();
    bad.body() = "{";
    BOOST_TEST(rpc_code(handle(bad)) == -32700);
    auto valid = message();
    auto params = field(valid, "params");
    J::set(params, "name", J::string("privateMemberFunction"));
    J::set(valid, "params", params);
    bad = request();
    bad.set("Mcp-Name", "privateMemberFunction");
    bad.body() = J::dump(valid);
    BOOST_TEST(handle(bad).result_int() == 400u);
    BOOST_TEST(rpc_code(handle(bad)) == -32602);
    BOOST_TEST(obj.calls == 1);
}

BOOST_AUTO_TEST_CASE(mirrors_are_checked_before_invocation) {
    Sample obj;
    mcp::server<Backend> dispatcher("mirrors");
    dispatcher.expose(obj);
    auto handle = mcp::make_beast_handler(dispatcher);
    for (auto name : {"Mcp-Method", "MCP-Protocol-Version", "Mcp-Name"}) {
        auto req = request();
        req.erase(name);
        auto response = handle(req);
        BOOST_TEST(response.result_int() == 400u);
        BOOST_TEST(rpc_code(response) == -32020);
        req.set(name, "mismatch");
        BOOST_TEST(rpc_code(handle(req)) == -32020);
        req.insert(name, "second-value");
        BOOST_TEST(rpc_code(handle(req)) == -32020);
    }
    BOOST_TEST(obj.calls == 0);
    auto req = request();
    req.set("mcp-name", "=?base64?YWRk?=");
    BOOST_TEST(handle(req).result_int() == 200u);
    BOOST_TEST(obj.calls == 1);
    req.set("mcp-name", "=?base64?YWRk====?=");
    BOOST_TEST(rpc_code(handle(req)) == -32020);
    auto body = message();
    auto params = field(body, "params");
    auto meta = field(params, "_meta");
    J::set(meta, "io.modelcontextprotocol/protocolVersion", J::string("2025-11-25"));
    J::set(params, "_meta", meta);
    J::set(body, "params", params);
    req.body() = J::dump(body);
    req.set("Mcp-Name", "add");
    req.set("MCP-Protocol-Version", "2025-11-25");
    BOOST_TEST(rpc_code(handle(req)) == -32022);
    BOOST_TEST(obj.calls == 1);
}

BOOST_AUTO_TEST_CASE(origin_media_and_http_policy) {
    Sample obj;
    mcp::server<Backend> dispatcher("policy");
    dispatcher.expose(obj);
    mcp::beast_options options;
    options.allowed_origins = {"https://client.example"};
    auto handle = mcp::make_beast_handler(dispatcher, options);
    auto req = request();
    req.set(http::field::origin, "https://evil.example");
    BOOST_TEST(handle(req).result_int() == 403u);
    req.set(http::field::origin, "null");
    BOOST_TEST(handle(req).result_int() == 403u);
    req.set(http::field::origin, "https://client.example");
    req.insert(http::field::origin, "https://client.example");
    BOOST_TEST(handle(req).result_int() == 403u);
    req = request();
    req.method(http::verb::get);
    BOOST_TEST(handle(req).result_int() == 405u);
    BOOST_TEST(std::string(handle(req)[http::field::allow]) == "POST");
    req = request();
    req.target("/other");
    BOOST_TEST(handle(req).result_int() == 404u);
    req = request();
    req.set(http::field::content_type, "text/plain");
    BOOST_TEST(handle(req).result_int() == 415u);
    req = request();
    req.set(http::field::content_encoding, "gzip");
    BOOST_TEST(handle(req).result_int() == 415u);
    for (auto accept : {"", "application/json", "text/event-stream", "application/json;q=0, text/event-stream"}) {
        req = request();
        req.set(http::field::accept, accept);
        BOOST_TEST(handle(req).result_int() == 406u);
    }
    BOOST_TEST(obj.calls == 0);
    req = request();
    req.set(http::field::origin, "https://client.example");
    req.set(http::field::accept, "APPLICATION/JSON; q=0.9, text/event-stream;q=1");
    BOOST_TEST(handle(req).result_int() == 200u);
    BOOST_TEST(obj.calls == 1);
    options.authorize = [](const auto&) { return false; };
    BOOST_TEST(mcp::make_beast_handler(dispatcher, options)(request()).result_int() == 401u);
    options.authorize = [](const auto&) -> bool { throw std::runtime_error("SECRET"); };
    auto failed = mcp::make_beast_handler(dispatcher, options)(request());
    BOOST_TEST(failed.result_int() == 500u);
    BOOST_TEST(failed.body().find("SECRET") == std::string::npos);
    options.authorize = {};
    options.body_limit = 4;
    BOOST_TEST(mcp::make_beast_handler(dispatcher, options)(request()).result_int() == 413u);
    BOOST_TEST(obj.calls == 1);
}

BOOST_AUTO_TEST_CASE(notification_and_invalid_envelopes) {
    Sample obj;
    mcp::server<Backend> dispatcher("envelopes");
    dispatcher.expose(obj);
    auto handle = mcp::make_beast_handler(dispatcher);
    auto req = request();
    req.body() = R"({"jsonrpc":"2.0","method":"notifications/cancelled","params":{"requestId":1}})";
    req.erase("Mcp-Method");
    req.erase("MCP-Protocol-Version");
    auto response = handle(req);
    BOOST_TEST(response.result_int() == 202u);
    BOOST_TEST(response.body().empty());
    for (auto invalid : {"[]", "42", "{}", R"({"jsonrpc":"2.0","id":null,"method":"tools/call"})"}) {
        req.body() = invalid;
        BOOST_TEST(handle(req).result_int() == 400u);
        BOOST_TEST(rpc_code(handle(req)) == -32600);
    }
    req = request();
    req.body() = R"({"jsonrpc":"2.0","id":1,"method":"tools/call","params":{}})";
    BOOST_TEST(rpc_code(handle(req)) == -32602);
    BOOST_TEST(obj.calls == 0);
}

BOOST_AUTO_TEST_CASE(base64_sentinel_validation) {
    for (auto text : {"=?base64?A?=", "=?base64?AA=A?=", "=?base64?YQ==AAAA?=", "=?base64?YR==?=", "=?base64?YWJ=?="})
        BOOST_TEST(!mcp::detail::decode_header(text).has_value());
    BOOST_TEST(*mcp::detail::decode_header("=?base64?YQ==?=") == "a");
    BOOST_TEST(*mcp::detail::decode_header("=?base64?YWI=?=") == "ab");
    BOOST_TEST(*mcp::detail::decode_header("=?base64?SGVsbG8sIOS4lueVjA==?=") == "Hello, 世界");
    BOOST_TEST(*mcp::detail::decode_header("=?base64?PT9iYXNlNjQ/bGl0ZXJhbD89?=") == "=?base64?literal?=");
}

BOOST_AUTO_TEST_CASE(bearer_authorization_policy) {
    Sample object;
    mcp::server<Backend> dispatcher("authenticated");
    dispatcher.expose(object);
    mcp::beast_options options;
    options.authorize = mcp::bearer_token_policy("example-test-token");
    options.authentication_challenge = "Bearer realm=\"mcp\"";
    auto handle = mcp::make_beast_handler(dispatcher, options);
    for (auto credential : {"", "Basic example-test-token", "Bearer wrong", "Bearer example-test-toke", "Bearer example-test-token-extra"}) {
        auto req = request();
        if (*credential) req.set(http::field::authorization, credential);
        auto response = handle(req);
        BOOST_TEST(response.result_int() == 401u);
        BOOST_TEST(std::string(response[http::field::www_authenticate]) == "Bearer realm=\"mcp\"");
        BOOST_TEST(response.body().find("example-test-token") == std::string::npos);
    }
    BOOST_TEST(object.calls == 0);
    auto req = request();
    req.set(http::field::authorization, "bEaReR example-test-token");
    BOOST_TEST(handle(req).result_int() == 200u);
    req.insert(http::field::authorization, "Bearer example-test-token");
    BOOST_TEST(handle(req).result_int() == 401u);
    BOOST_TEST(object.calls == 1);
    for (auto token : {"", "invalid token", "=padding", "middle=paddingx", "line\nbreak"})
        BOOST_CHECK_THROW(mcp::bearer_token_policy{token}, std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(non_loopback_requires_tls_and_authorization) {
    boost::asio::io_context io;
    Sample object;
    mcp::server<Backend> dispatcher("remote policy");
    dispatcher.expose(object);
    mcp::beast_options options;
    options.address = "0.0.0.0";
    options.port = 0;
    auto policy_error = [](const std::invalid_argument& e) {
        return std::string(e.what()).find("Non-loopback listeners require TLS and an authorization policy") != std::string::npos;
    };
    BOOST_CHECK_EXCEPTION((mcp::beast_listener(io, dispatcher, options)), std::invalid_argument, policy_error);
    BOOST_CHECK_EXCEPTION((mcp::beast_listener(io, dispatcher, options, mcp::beast_tls{"unused", "unused"})),
                          std::invalid_argument, policy_error);
    options.authorize = mcp::bearer_token_policy("public-test-token");
    BOOST_CHECK_EXCEPTION((mcp::beast_listener(io, dispatcher, options)), std::invalid_argument, policy_error);
}
