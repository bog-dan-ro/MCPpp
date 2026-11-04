// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#define BOOST_TEST_MODULE mcpp_schema
#include <boost/test/unit_test.hpp>
#include "backend.hpp"
#include <limits>

BOOST_AUTO_TEST_CASE(reflected_schemas) {
    static constexpr auto methods = mcp::describe<Sample>();
    static_assert(methods.size() == 2);
    template for (constexpr auto method : methods) {
        auto s = mcp::input_schema<method, Backend>();
        BOOST_TEST(J::as_string(field(s, "type")) == "object");
        BOOST_TEST(!J::as_bool(field(s, "additionalProperties")));
        const auto& props = field(s, "properties");
        if constexpr (std::meta::identifier_of(method) == "add") {
            BOOST_TEST(J::keys(props).size() == 2u);
            BOOST_TEST(J::dump(field(s, "required")) == "[\"a\",\"b\"]");
            for (auto name : {"a", "b"}) {
                const auto& p = field(props, name);
                BOOST_TEST(J::as_string(field(p, "type")) == "integer");
                BOOST_TEST(J::as_i64(field(p, "minimum")) == std::numeric_limits<int>::min());
                BOOST_TEST(J::as_i64(field(p, "maximum")) == std::numeric_limits<int>::max());
            }
        } else {
            BOOST_TEST(J::keys(props).size() == 1u);
            BOOST_TEST(J::as_string(field(field(props, "message"), "type")) == "string");
            BOOST_TEST(J::dump(field(s, "required")) == "[\"message\"]");
        }
    }
}

BOOST_AUTO_TEST_CASE(type_codec_contract) {
    BOOST_TEST((J::as_string(field(mcp::codec<double, Backend>::schema(), "type")) == "number"));
    BOOST_TEST((J::as_string(field(mcp::codec<bool, Backend>::schema(), "type")) == "boolean"));
    BOOST_TEST((J::as_string(field(mcp::codec<std::string, Backend>::schema(), "type")) == "string"));
    BOOST_TEST((mcp::codec<int, Backend>::decode(J::parse("2.0"), "n") == 2));
    BOOST_TEST((mcp::codec<double, Backend>::decode(J::parse("18446744073709551615"), "n") > 0));
    BOOST_CHECK_THROW((mcp::codec<bool, Backend>::decode(J::parse("1"), "b")), mcp::argument_error);
    BOOST_CHECK_THROW((mcp::codec<double, Backend>::decode(J::parse("\"2\""), "n")), mcp::argument_error);
}

struct Empty { [[=mcp::tool]] void ping() const noexcept {} };
BOOST_AUTO_TEST_CASE(empty_object_schema) {
    constexpr auto method = mcp::describe<Empty>()[0];
    auto s = mcp::input_schema<method, Backend>();
    BOOST_TEST(J::keys(field(s, "properties")).empty());
    BOOST_TEST(J::dump(field(s, "required")) == "[]");
}
