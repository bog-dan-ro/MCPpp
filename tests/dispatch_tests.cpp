// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#define BOOST_TEST_MODULE mcpp_dispatch
#include <boost/test/unit_test.hpp>
#include "backend.hpp"
#include <limits>
#include <sstream>

BOOST_AUTO_TEST_CASE(exposure_and_validation) {
    Sample sample;
    mcp::server<Backend> server("test");
    server.expose(sample);
    auto list = J::dump(server.list_tools());
    BOOST_TEST(list.find("privateMemberFunction") == std::string::npos);
    BOOST_TEST(list.find("secret") == std::string::npos);
    auto result = server.call_tool("add", J::parse(R"({"a":2,"b":3})"));
    BOOST_TEST(!is_error(result));
    BOOST_TEST(J::as_i64(field(field(result, "structuredContent"), "result")) == 5);
    BOOST_TEST(sample.calls == 1);
    for (auto invalid : {R"({"a":2})", R"({"a":2,"b":3,"c":4})", R"({"a":"2","b":3})",
        R"({"a":true,"b":3})", R"({"a":2.5,"b":3})", R"({"a":2147483648,"b":3})",
        R"({"a":-2147483649,"b":3})", R"({"a":18446744073709551615,"b":3})",
        R"({"a":null,"b":3})", "[]", "null"}) {
        BOOST_TEST(is_error(server.call_tool("add", J::parse(invalid))));
        BOOST_TEST(sample.calls == 1);
    }
    BOOST_CHECK_THROW(server.call_tool("privateMemberFunction", J::object()), mcp::protocol_error);
    BOOST_CHECK_THROW(server.call_tool("secret", J::object()), mcp::protocol_error);
    BOOST_TEST(sample.calls == 1);
    auto void_result = server.call_tool("printMessage", J::parse(R"({"message":"hello"})"));
    BOOST_TEST(!is_error(void_result));
    BOOST_TEST(J::find(void_result, "structuredContent") == nullptr);
    BOOST_TEST(sample.message == "hello");
}

struct Extras {
    [[=mcp::tool]] std::string views(std::string_view shortText, std::string_view longText) {
        return std::string(shortText) + std::string(longText);
    }
    [[=mcp::tool]] bool invert(bool flag) const noexcept { return !flag; }
    [[=mcp::tool]] double twice(double number) { return number * 2; }
    [[=mcp::tool]] std::string echo(std::string text) { return text; }
    [[=mcp::tool]] void fail() { throw std::runtime_error("SECRET password=123"); }
    [[=mcp::tool]] void failArgument() { throw mcp::argument_error("SECRET validation lookalike"); }
    [[=mcp::tool]] void failOther() { throw 42; }
    [[=mcp::tool]] double nonfinite() { return std::numeric_limits<double>::infinity(); }
    [[=mcp::tool]] std::string invalidUtf8() { return std::string(1, '\xff'); }
    [[=mcp::tool]] int zero() { return 0; }
};

BOOST_AUTO_TEST_CASE(types_lifetimes_and_exceptions) {
    Extras obj;
    mcp::server<Backend> server("types");
    server.expose(obj);
    auto args = J::obj({{"shortText", J::string(std::string("a\0b", 3))},
                        {"longText", J::string(std::string(10000, 'x'))}});
    auto result = server.call_tool("views", args);
    const auto text = J::as_string(field(field(result, "structuredContent"), "result"));
    BOOST_TEST(text.size() == 10003u);
    BOOST_TEST(text[1] == '\0');
    BOOST_TEST(text.substr(3) == std::string(10000, 'x'));
    auto b = server.call_tool("invert", J::parse(R"({"flag":false})"));
    BOOST_TEST(J::as_bool(field(field(b, "structuredContent"), "result")));
    BOOST_TEST(is_error(server.call_tool("invert", J::parse(R"({"flag":1})"))));
    auto d = server.call_tool("twice", J::parse(R"({"number":2.5})"));
    BOOST_TEST(J::as_double(field(field(d, "structuredContent"), "result")) == 5.0);
    BOOST_TEST(is_error(server.call_tool("twice", J::parse(R"({"number":"2.5"})"))));
    auto s = server.call_tool("echo", J::parse(R"({"text":"owned"})"));
    BOOST_TEST(J::as_string(field(field(s, "structuredContent"), "result")) == "owned");
    for (auto name : {"fail", "failArgument", "failOther", "nonfinite", "invalidUtf8"}) {
        auto failure = server.call_tool(name, J::object());
        BOOST_TEST(is_error(failure));
        BOOST_TEST(J::dump(failure).find("SECRET") == std::string::npos);
    }
    BOOST_TEST(!is_error(server.call_tool("zero", J::object())));
}

struct Conflict {
    [[=mcp::tool]] int aaa() { return 1; }
    [[=mcp::tool]] int add(int a, int b) { return a + b; }
};
BOOST_AUTO_TEST_CASE(duplicate_registration_is_atomic) {
    Sample a, b;
    Conflict conflict;
    mcp::server<Backend> server("duplicates");
    server.expose(a);
    auto before = J::dump(server.list_tools());
    BOOST_CHECK_THROW(server.expose(b), std::invalid_argument);
    BOOST_CHECK_THROW(server.expose(conflict), std::invalid_argument);
    BOOST_TEST(J::dump(server.list_tools()) == before);
}

struct Base { [[=mcp::tool]] int inherited() { return 1; } };
struct Derived : Base { [[=mcp::tool]] int own() const { return 2; } };
struct Redeclared { [[=mcp::tool]] int method(int value); };
inline int Redeclared::method(int value) { return value; }
struct MixedOverloads {
    [[=mcp::tool]] int selected() const { return 1; }
    int selected() { return 99; }
};
BOOST_AUTO_TEST_CASE(deliberate_discovery_boundaries) {
    static_assert(mcp::describe<Derived>().size() == 1);
    static_assert(std::meta::identifier_of(mcp::describe<Derived>()[0]) == "own");
    static_assert(mcp::describe<Redeclared>().size() == 1);
    Redeclared object;
    mcp::server<Backend> server("declarations");
    server.expose(object);
    BOOST_TEST(!is_error(server.call_tool("method", J::parse(R"({"value":7})"))));
    MixedOverloads overloads;
    server.expose(overloads);
    auto result = server.call_tool("selected", J::object());
    BOOST_TEST(J::as_i64(field(field(result, "structuredContent"), "result")) == 1);
}

BOOST_AUTO_TEST_CASE(adapter_contract) {
    for (auto invalid : {"{", "{} trailing", "NaN", "Infinity", "1e9999", "[1,]", "/*x*/{}"})
        BOOST_CHECK_THROW(J::parse(invalid), mcp::parse_error);
    BOOST_CHECK_THROW(J::parse(std::string("\"\xff\"", 3)), mcp::parse_error);
    auto value = J::parse(R"({"key":"value","n":-1,"u":18446744073709551615})");
    auto copy = value;
    J::set(copy, "key", J::string("changed"));
    BOOST_TEST(J::as_string(field(value, "key")) == "value");
    BOOST_TEST(J::find(value, "absent") == nullptr);
    BOOST_TEST(J::as_u64(field(value, "u")) == std::numeric_limits<std::uint64_t>::max());
}

BOOST_AUTO_TEST_CASE(transport_independent_routing_and_streams) {
    Sample sample;
    mcp::server<Backend> server("streams");
    server.expose(sample);
    std::string request = R"({"jsonrpc":"2.0","id":"a","method":"tools/call","params":{"name":"add","arguments":{"a":2,"b":3},"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientCapabilities":{}}}})";
    std::istringstream input(request + "\n{\n" + request + "\n");
    std::ostringstream output;
    BOOST_TEST(server.run({input, output}) == 0);
    std::istringstream responses(output.str());
    std::string line;
    int count = 0;
    while (std::getline(responses, line)) { J::parse(line); ++count; }
    BOOST_TEST(count == 3);
    BOOST_TEST(sample.calls == 2);
}
