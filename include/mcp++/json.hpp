// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#pragma once

#include <concepts>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mcp {
enum class json_kind { null, object, array, string, boolean, signed_integer, unsigned_integer, number };
struct parse_error : std::runtime_error { using std::runtime_error::runtime_error; };

// Adapters own their DOM values. find()/as_string() borrow from an unchanged DOM.
// parse() must reject invalid UTF-8, non-finite numbers, and trailing input, and
// translate parser errors to mcp::parse_error. dump() must produce strict JSON.
template<class B>
concept json_backend = std::default_initializable<typename B::value> &&
    std::copyable<typename B::value> && requires(
    typename B::value v, const typename B::value cv, std::string_view s,
    std::int64_t i, double d, bool b) {
    { B::parse(s) } -> std::same_as<typename B::value>;
    { B::dump(cv) } -> std::same_as<std::string>;
    { B::kind(cv) } -> std::same_as<json_kind>;
    { B::object() } -> std::same_as<typename B::value>;
    { B::array() } -> std::same_as<typename B::value>;
    { B::string(s) } -> std::same_as<typename B::value>;
    { B::integer(i) } -> std::same_as<typename B::value>;
    { B::number(d) } -> std::same_as<typename B::value>;
    { B::boolean(b) } -> std::same_as<typename B::value>;
    { B::find(cv, s) } -> std::same_as<const typename B::value*>;
    { B::keys(cv) } -> std::same_as<std::vector<std::string>>;
    { B::as_string(cv) } -> std::same_as<std::string_view>;
    { B::as_i64(cv) } -> std::same_as<std::int64_t>;
    { B::as_u64(cv) } -> std::same_as<std::uint64_t>;
    { B::as_double(cv) } -> std::same_as<double>;
    { B::as_bool(cv) } -> std::same_as<bool>;
    { B::set(v, s, v) } -> std::same_as<void>;
    { B::append(v, v) } -> std::same_as<void>;
};

namespace detail {
template<json_backend B>
struct json : B {
    using value = typename B::value;
    static value obj(std::initializer_list<std::pair<std::string_view, value>> fields) {
        auto result = B::object();
        for (const auto& [name, v] : fields) B::set(result, name, v);
        return result;
    }
    static value arr(std::initializer_list<value> values) {
        auto result = B::array();
        for (const auto& v : values) B::append(result, v);
        return result;
    }
    static bool is(const value& v, json_kind k) { return B::kind(v) == k; }
    static bool integer_kind(const value& v) {
        return is(v, json_kind::signed_integer) || is(v, json_kind::unsigned_integer);
    }
    static bool string_field(const value& v, std::string_view name) {
        auto p = B::find(v, name);
        return p && is(*p, json_kind::string);
    }
};
} // namespace detail
} // namespace mcp
