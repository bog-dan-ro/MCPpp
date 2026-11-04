// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#pragma once
#include <mcp++/schema.hpp>
#include <tuple>
#include <utility>

namespace mcp::detail {
template<json_backend B>
typename B::value tool_result(std::string_view text, bool error = false) {
    using J = json<B>;
    return J::obj({{"resultType", J::string("complete")}, {"isError", J::boolean(error)},
        {"content", J::arr({J::obj({{"type", J::string("text")}, {"text", J::string(text)}})})}});
}

template<json_backend B, class R>
typename B::value encode_result(const R& result) {
    using J = json<B>;
    typename B::value v;
    if constexpr (std::is_same_v<R, int>) v = J::integer(result);
    else if constexpr (std::is_same_v<R, bool>) v = J::boolean(result);
    else if constexpr (std::is_same_v<R, double>) {
        if (!std::isfinite(result)) return tool_result<B>("Tool returned a non-finite number", true);
        v = J::number(result);
    } else v = J::string(result);
    auto structured = J::obj({{"result", std::move(v)}});
    auto response = tool_result<B>(J::dump(structured));
    J::set(response, "structuredContent", std::move(structured));
    return response;
}

template<std::meta::info M, std::size_t I>
using parameter_type = [:std::meta::type_of(std::meta::parameters_of(M)[I]):];

template<std::meta::info M, json_backend B, class T, std::size_t... I>
typename B::value invoke(T& object, const typename B::value& args, std::index_sequence<I...>) {
    using J = json<B>;
    static constexpr auto params = std::define_static_array(std::meta::parameters_of(M));
    if (!J::is(args, json_kind::object)) throw argument_error("Tool arguments must be an object");
    template for (constexpr auto p : params) {
        constexpr auto name = std::meta::identifier_of(p);
        if (!J::find(args, name)) throw argument_error("Missing argument '" + std::string(name) + "'");
    }
    for (const auto& key : J::keys(args)) {
        bool known = false;
        template for (constexpr auto p : params) {
            if (key == std::meta::identifier_of(p)) known = true;
        }
        if (!known) throw argument_error("Unknown argument '" + key + "'");
    }
    // Keep strings in place before forming views for the call.
    std::tuple<typename codec<parameter_type<M, I>, B>::storage...> storage{
        codec<parameter_type<M, I>, B>::decode(*J::find(args, std::meta::identifier_of(params[I])),
                                              std::meta::identifier_of(params[I]))...};
    using R = [:std::meta::return_type_of(M):];
    try {
        if constexpr (std::is_void_v<R>) {
            object.[:M:](codec<parameter_type<M, I>, B>::argument(std::get<I>(storage))...);
            return tool_result<B>("Completed");
        } else {
            return encode_result<B>(object.[:M:](codec<parameter_type<M, I>, B>::argument(std::get<I>(storage))...));
        }
    } catch (...) {
        // Do not expose exceptions thrown by user code.
        return tool_result<B>("Tool execution failed", true);
    }
}

template<std::meta::info M, json_backend B, class T>
typename B::value call(T& object, const typename B::value& args) {
    try {
        return invoke<M, B>(object, args, std::make_index_sequence<std::meta::parameters_of(M).size()>{});
    } catch (const argument_error& e) {
        return tool_result<B>(e.what(), true);
    } catch (...) {
        return tool_result<B>("Tool execution failed", true);
    }
}
} // namespace mcp::detail
