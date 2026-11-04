// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#include "provider.hpp"

std::uint16_t port_from_second_translation_unit() {
    Provider object;
    mcp::server<mcp::boost_backend> dispatcher("Second translation unit");
    dispatcher.expose(object);
    boost::asio::io_context io;
    mcp::beast_options options;
    options.port = 0;
    mcp::beast_listener listener(io, dispatcher, options);
    auto port = listener.port();
    listener.stop();
    io.run();
    return port;
}
