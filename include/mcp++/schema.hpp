// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#pragma once
#include <mcp++/json.hpp>
#include <mcp++/reflection.hpp>
#include <cmath>
#include <limits>

namespace mcp {
struct argument_error : std::runtime_error { using std::runtime_error::runtime_error; };

// A single codec defines both the schema and decoding policy for each C++ type.
template<class T, json_backend B>
struct codec {
    static_assert(detail::input_type<T>, "MCP++: unsupported parameter type");
    using J = detail::json<B>;
    using value = typename B::value;
    using storage = std::conditional_t<std::is_same_v<T, std::string_view>, std::string, T>;
    static value schema() {
        if constexpr (std::is_same_v<T, int>) {
            return J::obj({{"type", J::string("integer")},
                {"minimum", J::integer(std::numeric_limits<int>::min())},
                {"maximum", J::integer(std::numeric_limits<int>::max())}});
        } else if constexpr (std::is_same_v<T, double>) {
            return J::obj({{"type", J::string("number")},
                {"minimum", J::number(std::numeric_limits<double>::lowest())},
                {"maximum", J::number(std::numeric_limits<double>::max())}});
        } else if constexpr (std::is_same_v<T, bool>) {
            return J::obj({{"type", J::string("boolean")}});
        } else return J::obj({{"type", J::string("string")}});
    }
    static storage decode(const value& v, std::string_view name) {
        auto fail = [&](std::string_view reason) -> storage {
            throw argument_error("Argument '" + std::string(name) + "' " + std::string(reason));
        };
        if constexpr (std::is_same_v<T, int>) {
            // JSON Schema integer includes numbers with a zero fractional part.
            if (J::is(v, json_kind::unsigned_integer)) {
                auto n = J::as_u64(v);
                if (n <= static_cast<std::uint64_t>(std::numeric_limits<int>::max())) return static_cast<int>(n);
            } else if (J::is(v, json_kind::signed_integer)) {
                auto n = J::as_i64(v);
                if (n >= std::numeric_limits<int>::min() && n <= std::numeric_limits<int>::max()) return static_cast<int>(n);
            } else if (J::is(v, json_kind::number)) {
                auto n = J::as_double(v);
                if (std::isfinite(n) && std::trunc(n) == n && n >= std::numeric_limits<int>::min() &&
                    n <= std::numeric_limits<int>::max()) return static_cast<int>(n);
            }
            return fail("must be an integer within the C++ int range");
        } else if constexpr (std::is_same_v<T, double>) {
            if (!J::integer_kind(v) && !J::is(v, json_kind::number)) return fail("must be a number");
            auto n = J::as_double(v);
            if (!std::isfinite(n)) return fail("must be finite");
            return n;
        } else if constexpr (std::is_same_v<T, bool>) {
            if (!J::is(v, json_kind::boolean)) return fail("must be a boolean");
            return J::as_bool(v);
        } else {
            if (!J::is(v, json_kind::string)) return fail("must be a string");
            return std::string(J::as_string(v));
        }
    }
    static T argument(storage& s) { return T(s); }
};

template<std::meta::info M, json_backend B>
typename B::value input_schema() {
    using J = detail::json<B>;
    auto props = J::object();
    auto required = J::array();
    template for (constexpr auto p : std::define_static_array(std::meta::parameters_of(M))) {
        using A = [:std::meta::type_of(p):];
        constexpr auto name = std::meta::identifier_of(p);
        J::set(props, name, codec<A, B>::schema());
        J::append(required, J::string(name));
    }
    return J::obj({{"type", J::string("object")}, {"properties", std::move(props)},
                   {"required", std::move(required)}, {"additionalProperties", J::boolean(false)}});
}
} // namespace mcp
