// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#pragma once
#include <mcp++/json.hpp>
#include <iostream>

namespace mcp {
struct stdio {
    std::istream& input = std::cin;
    std::ostream& output = std::cout;
};

namespace detail {
template<json_backend B, class Dispatcher>
int run_stdio(const Dispatcher& dispatcher, stdio streams) {
    std::string line;
    while (std::getline(streams.input, line)) {
        if (auto response = dispatcher.process_line(line)) {
            streams.output << B::dump(*response) << '\n';
            streams.output.flush();
            if (!streams.output) return 1;
        }
    }
    return streams.input.bad() ? 1 : 0;
}
} // namespace detail
} // namespace mcp
