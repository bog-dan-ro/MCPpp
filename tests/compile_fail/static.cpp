// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#include <mcp++/reflection.hpp>
struct Bad { [[=mcp::tool]] static void f(int number) {} };
constexpr auto methods = mcp::describe<Bad>();
