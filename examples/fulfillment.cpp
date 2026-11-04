// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#include <mcp++/mcp.hpp>
#include "backend.hpp"
#include "fulfillment.hpp"
#if defined(MCPP_EXAMPLE_HTTP)
#include "http_main.hpp"
#endif

int main(int argc, char** argv) {
    Stockroom stockroom;
    Fulfillment fulfillment(stockroom);
    mcp::server<json_backend> dispatcher("MCP++ Fulfillment");
    dispatcher.expose(stockroom);
    dispatcher.expose(fulfillment);
#if defined(MCPP_EXAMPLE_HTTP)
    return serve_http(dispatcher, argc, argv);
#else
    return dispatcher.run(mcp::stdio{});
#endif
}
