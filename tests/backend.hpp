// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#pragma once
#include <mcp++/mcp.hpp>
#if defined(MCPP_USE_BOOST)
#include <mcp++/json/boost.hpp>
using Backend = mcp::boost_backend;
#elif defined(MCPP_USE_QT)
#include <mcp++/json/qt.hpp>
using Backend = mcp::qt_backend;
#else
#include <mcp++/json/nlohmann.hpp>
using Backend = mcp::nlohmann_backend;
#endif
using J = mcp::detail::json<Backend>;
using Value = Backend::value;

inline const Value& field(const Value& v, std::string_view name) {
    auto p = J::find(v, name);
    if (!p) throw std::runtime_error("Missing test field: " + std::string(name));
    return *p;
}
inline bool is_error(const Value& v) { return J::as_bool(field(v, "isError")); }

struct Sample {
    std::string message;
    int calls = 0;
    [[=mcp::tool]] void printMessage(std::string_view message) { this->message = message; ++calls; }
    [[=mcp::tool]] int add(int a, int b) { ++calls; return a + b; }
    void privateMemberFunction() { ++calls; }
private:
    [[=mcp::tool]] int secret() { return 99; }
    template<class T> [[=mcp::tool]] void privateTemplate(T value) {}
};
