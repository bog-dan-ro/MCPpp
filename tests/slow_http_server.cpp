// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#include <mcp++/mcp.hpp>
#include <mcp++/json/boost.hpp>
#include "../examples/http_main.hpp"
#include <thread>

struct SlowTool {
    int calls = 0;
    [[=mcp::tool]] int wait(int milliseconds) {
        std::cerr << "Tool started" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
        return ++calls;
    }
};

int main(int argc, char** argv) {
    SlowTool tool;
    mcp::server<mcp::boost_backend> dispatcher("Slow tool test");
    dispatcher.expose(tool);
    return serve_http(dispatcher, argc, argv);
}
