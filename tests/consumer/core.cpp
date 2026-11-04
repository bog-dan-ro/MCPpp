// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#include <mcp++/reflection.hpp>

struct Provider {
    [[=mcp::tool]] int twice(int value) { return value * 2; }
};

int main() {
    static_assert(mcp::describe<Provider>().size() == 1);
    constexpr auto method = mcp::describe<Provider>()[0];
    Provider object;
    return object.[:method:](21) == 42 ? 0 : 1;
}
