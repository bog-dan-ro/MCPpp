// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#include <mcp++/reflection.hpp>
struct Bad {
    [[=mcp::tool]] void f(int number) {}
    [[=mcp::tool]] void f(double number) {}
};
constexpr auto methods = mcp::describe<Bad>();
