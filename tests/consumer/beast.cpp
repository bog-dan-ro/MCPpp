// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#include "provider.hpp"

#if MCPP_EXPECT_TLS && !defined(MCPP_BEAST_TLS)
#error "The installed target lost its TLS usage requirements"
#elif !MCPP_EXPECT_TLS && defined(MCPP_BEAST_TLS)
#error "The installed target enabled TLS in an HTTP-only package"
#endif

int main() {
    Provider object;
    mcp::server<mcp::boost_backend> dispatcher("Installed consumer");
    dispatcher.expose(object);
    boost::asio::io_context io;
    mcp::beast_options options;
    options.port = 0;
    mcp::beast_listener listener(io, dispatcher, options);
    const auto port = listener.port();
    const auto other_port = port_from_second_translation_unit();
    auto result = dispatcher.call_tool("add", mcp::boost_backend::parse(R"({"a":2,"b":3})"));
    listener.stop();
    io.run();
    return port && other_port && result.as_object().at("structuredContent").as_object().at("result").as_int64() == 5 ? 0 : 1;
}
