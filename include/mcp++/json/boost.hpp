// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#pragma once
#include <mcp++/json.hpp>
#include <boost/json.hpp>
#include <cmath>

namespace mcp {
struct boost_backend {
    using value = boost::json::value;
    static void check_finite(const value& v) {
        if (v.is_double() && !std::isfinite(v.as_double()))
            throw parse_error("JSON number is outside the finite double range");
        if (v.is_array()) for (const auto& child : v.as_array()) check_finite(child);
        if (v.is_object()) for (const auto& child : v.as_object()) check_finite(child.value());
    }
    static value parse(std::string_view s) {
        boost::system::error_code ec;
        boost::json::parse_options options;
        options.numbers = boost::json::number_precision::precise;
        auto result = boost::json::parse(s, ec, {}, options);
        if (ec) throw parse_error("Invalid JSON");
        check_finite(result);
        return result;
    }
    static std::string dump(const value& v) {
        check_finite(v);
        auto text = boost::json::serialize(v);
        // Boost's DOM permits invalid UTF-8; validate before writing a frame.
        boost::system::error_code ec;
        boost::json::parse(text, ec);
        if (ec) throw std::domain_error("Invalid UTF-8 in JSON output");
        return text;
    }
    static json_kind kind(const value& v) {
        switch (v.kind()) {
        case boost::json::kind::object: return json_kind::object;
        case boost::json::kind::array: return json_kind::array;
        case boost::json::kind::string: return json_kind::string;
        case boost::json::kind::bool_: return json_kind::boolean;
        case boost::json::kind::int64: return json_kind::signed_integer;
        case boost::json::kind::uint64: return json_kind::unsigned_integer;
        case boost::json::kind::double_: return json_kind::number;
        default: return json_kind::null;
        }
    }
    static value object() { return boost::json::object{}; }
    static value array() { return boost::json::array{}; }
    static value string(std::string_view s) { return boost::json::string(s); }
    static value integer(std::int64_t n) { return n; }
    static value number(double n) {
        if (!std::isfinite(n)) throw std::domain_error("Non-finite JSON number");
        return n;
    }
    static value boolean(bool b) { return b; }
    static const value* find(const value& v, std::string_view name) {
        return v.is_object() ? v.as_object().if_contains(name) : nullptr;
    }
    static std::vector<std::string> keys(const value& v) {
        std::vector<std::string> result;
        for (const auto& field : v.as_object()) result.emplace_back(field.key());
        return result;
    }
    static std::string_view as_string(const value& v) { return v.as_string(); }
    static std::int64_t as_i64(const value& v) { return v.as_int64(); }
    static std::uint64_t as_u64(const value& v) { return v.as_uint64(); }
    static double as_double(const value& v) {
        if (v.is_int64()) return static_cast<double>(v.as_int64());
        if (v.is_uint64()) return static_cast<double>(v.as_uint64());
        return v.as_double();
    }
    static bool as_bool(const value& v) { return v.as_bool(); }
    static void set(value& v, std::string_view name, value field) {
        v.as_object().insert_or_assign(name, std::move(field));
    }
    static void append(value& v, value item) { v.as_array().push_back(std::move(item)); }
};
static_assert(json_backend<boost_backend>);
} // namespace mcp
