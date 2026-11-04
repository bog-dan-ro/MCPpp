// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#include <mcp++/reflection.hpp>
struct Bad { template<class T> [[=mcp::tool]] void f(T number) {} };
constexpr auto methods = mcp::describe<Bad>();
