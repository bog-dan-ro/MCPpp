// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#include <mcp++/reflection.hpp>
struct Bad { [[=mcp::tool]] std::string_view f() { return "bad"; } };
constexpr auto methods = mcp::describe<Bad>();
