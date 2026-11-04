// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#pragma once
#include <cstddef>

namespace mcp {
struct tool_t {
    char description[512]{}; // Structural storage required by annotation values.
    constexpr tool_t() = default;
    template<std::size_t N>
    consteval tool_t(const char (&text)[N]) {
        static_assert(N <= sizeof(description), "MCP++: tool description exceeds 511 characters");
        for (std::size_t i = 0; i < N; ++i) description[i] = text[i];
    }
};
inline constexpr tool_t tool{};
} // namespace mcp
