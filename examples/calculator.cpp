// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#include <mcp++/mcp.hpp>
#include "backend.hpp"
#include "calculator.hpp"

int main() {
    Calculator object;
    mcp::server<json_backend> server{"MCP++ Calculator"};
    server.expose(object);
    return server.run(mcp::stdio{});
}
