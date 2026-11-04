// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#pragma once
#include <mcp++/annotations.hpp>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

struct Calculator {
    [[=mcp::tool_t{"Write a message to the server diagnostic stream"}]]
    void printMessage(std::string_view message) { std::cerr << message << '\n'; }

    [[=mcp::tool_t{"Add two integers with overflow checking"}]]
    int add(int a, int b) const {
        if ((b > 0 && a > std::numeric_limits<int>::max() - b) ||
            (b < 0 && a < std::numeric_limits<int>::min() - b))
            throw std::overflow_error("Addition overflow");
        return a + b;
    }

    void privateMemberFunction() {}
private:
    [[=mcp::tool]] int secret() { return 42; }
};
