// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#pragma once
#include <mcp++/annotations.hpp>
#include <meta>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace mcp {
namespace detail {
template<class T>
inline constexpr bool input_type = std::is_same_v<T, int> || std::is_same_v<T, double> ||
    std::is_same_v<T, bool> || std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view>;
template<class T>
inline constexpr bool output_type = std::is_same_v<T, void> ||
    (input_type<T> && !std::is_same_v<T, std::string_view>);

template<std::meta::info M>
consteval void validate_method() {
    static_assert(std::meta::is_function(M) && !std::meta::is_function_template(M),
                  "MCP++: tool must be an ordinary member function, not a template");
    if constexpr (std::meta::is_function(M)) {
        static_assert(!std::meta::is_static_member(M), "MCP++: static tools are unsupported");
        static_assert(!std::meta::is_deleted(M), "MCP++: deleted tools are unsupported");
        static_assert(!std::meta::is_operator_function(M) && !std::meta::is_special_member_function(M),
                      "MCP++: tool must be an ordinary named method");
        static_assert(!std::meta::is_lvalue_reference_qualified(M) &&
                      !std::meta::is_rvalue_reference_qualified(M) && !std::meta::is_volatile(M),
                      "MCP++: ref-qualified and volatile tools are unsupported");
        static_assert(!std::meta::is_vararg_function(M), "MCP++: variadic tools are unsupported");
        using R = [:std::meta::return_type_of(M):];
        static_assert(output_type<R>, "MCP++: unsupported return type (string_view is input only)");
        template for (constexpr auto p : std::define_static_array(std::meta::parameters_of(M))) {
            static_assert(std::meta::has_identifier(p), "MCP++: every tool parameter must have a name");
            static_assert(!std::meta::is_explicit_object_parameter(p),
                          "MCP++: explicit object parameters are unsupported");
            using A = [:std::meta::type_of(p):];
            static_assert(input_type<A>, "MCP++: unsupported parameter type; use int, double, bool, string or string_view by value");
        }
    }
}

template<class T>
consteval auto discover() {
    std::vector<std::meta::info> result;
    template for (constexpr auto m : std::define_static_array(std::meta::members_of(
                     ^^T, std::meta::access_context::unprivileged()))) {
        // Annotations cannot be queried on function templates, so reject them.
        if constexpr (std::meta::is_function_template(m)) {
            static_assert(!std::meta::is_function_template(m),
                          "MCP++: tool must be an ordinary member function; accessible member templates are unsupported");
        } else if constexpr (!std::meta::is_template(m)) {
            if constexpr (std::meta::is_public(m) &&
                          !std::meta::annotations_of_with_type(m, ^^tool_t).empty()) {
                validate_method<m>();
                result.push_back(m);
            }
        }
    }
    return result;
}

template<class T>
consteval bool unique_names() {
    auto methods = discover<T>();
    for (std::size_t i = 0; i < methods.size(); ++i)
        for (std::size_t j = i + 1; j < methods.size(); ++j)
            if (std::meta::identifier_of(methods[i]) == std::meta::identifier_of(methods[j])) return false;
    return true;
}
} // namespace detail

// Only accessible direct declarations are considered. Bases are never traversed.
template<class T>
consteval auto describe() {
    static_assert(detail::unique_names<T>(), "MCP++: duplicate tool name (annotated overloads are unsupported)");
    return std::define_static_array(detail::discover<T>());
}
} // namespace mcp
