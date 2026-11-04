// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#pragma once
#include <mcp++/beast.hpp>
#include <mcp++/json/boost.hpp>

struct Provider {
    [[=mcp::tool]] int add(int a, int b) { return a + b; }
};

std::uint16_t port_from_second_translation_unit();
