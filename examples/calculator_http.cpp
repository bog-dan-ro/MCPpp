// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#include "backend.hpp"
#include "calculator.hpp"
#include "http_main.hpp"

int main(int argc, char** argv) {
    Calculator object;
    mcp::server<json_backend> dispatcher("MCP++ Calculator");
    dispatcher.expose(object);
    return serve_http(dispatcher, argc, argv);
}
