// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#pragma once
#include <mcp++/json.hpp>
#include <nlohmann/json.hpp>
#include <cmath>

namespace mcp {
struct nlohmann_backend {
    using value = nlohmann::json;
    static value parse(std::string_view s) {
        try { return value::parse(s); }
        catch (const value::exception&) { throw parse_error("Invalid JSON"); }
    }
    static std::string dump(const value& v) { return v.dump(); }
    static json_kind kind(const value& v) {
        if (v.is_object()) return json_kind::object;
        if (v.is_array()) return json_kind::array;
        if (v.is_string()) return json_kind::string;
        if (v.is_boolean()) return json_kind::boolean;
        if (v.is_number_unsigned()) return json_kind::unsigned_integer;
        if (v.is_number_integer()) return json_kind::signed_integer;
        if (v.is_number_float()) return json_kind::number;
        return json_kind::null;
    }
    static value object() { return value::object(); }
    static value array() { return value::array(); }
    static value string(std::string_view s) { return std::string(s); }
    static value integer(std::int64_t n) { return n; }
    static value number(double n) {
        if (!std::isfinite(n)) throw std::domain_error("Non-finite JSON number");
        return n;
    }
    static value boolean(bool b) { return b; }
    static const value* find(const value& v, std::string_view name) {
        if (!v.is_object()) return nullptr;
        auto it = v.find(name);
        return it == v.end() ? nullptr : &*it;
    }
    static std::vector<std::string> keys(const value& v) {
        std::vector<std::string> result;
        for (auto it = v.begin(); it != v.end(); ++it) result.push_back(it.key());
        return result;
    }
    static std::string_view as_string(const value& v) { return v.get_ref<const std::string&>(); }
    static std::int64_t as_i64(const value& v) { return v.get<std::int64_t>(); }
    static std::uint64_t as_u64(const value& v) { return v.get<std::uint64_t>(); }
    static double as_double(const value& v) { return v.get<double>(); }
    static bool as_bool(const value& v) { return v.get<bool>(); }
    static void set(value& v, std::string_view name, value field) { v[std::string(name)] = std::move(field); }
    static void append(value& v, value item) { v.push_back(std::move(item)); }
};
static_assert(json_backend<nlohmann_backend>);
} // namespace mcp
