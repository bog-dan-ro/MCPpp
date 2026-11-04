// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#pragma once
#include <mcp++/beast.hpp>
#include <boost/asio/signal_set.hpp>
#include <csignal>
#include <cstdlib>
#include <iostream>

// Shared command line for both HTTP examples.
template<mcp::json_backend B>
int serve_http(const mcp::server<B>& dispatcher, int argc, char** argv) {
    try {
        mcp::beast_options options;
        std::optional<mcp::beast_tls> tls;
        auto number = [](const char* text, unsigned long long maximum) {
            std::string_view value(text);
            unsigned long long parsed = 0;
            auto [end, ec] = std::from_chars(value.data(), value.data() + value.size(), parsed);
            if (ec != std::errc{} || end != value.data() + value.size() || parsed > maximum)
                throw std::invalid_argument("Invalid numeric option");
            return parsed;
        };
        for (int i = 1; i < argc; ++i) {
            std::string_view arg(argv[i]);
            if (arg == "--port" && i + 1 < argc)
                options.port = static_cast<std::uint16_t>(number(argv[++i], 65535));
            else if (arg == "--origin" && i + 1 < argc)
                options.allowed_origins.emplace_back(argv[++i]);
            else if (arg == "--address" && i + 1 < argc)
                options.address = argv[++i];
            else if (arg == "--token-env" && i + 1 < argc) {
                const char* token = std::getenv(argv[++i]);
                if (!token)
                    throw std::invalid_argument("Bearer token environment variable is missing");
                options.authorize = mcp::bearer_token_policy(token);
                options.authentication_challenge = "Bearer realm=\"mcp\"";
            } else if (arg == "--timeout-ms" && i + 1 < argc)
                options.io_timeout = std::chrono::milliseconds(number(argv[++i], 60000));
            else if (arg == "--tls" && i + 2 < argc) {
                const std::string cert(argv[++i]);
                const std::string key(argv[++i]);
                tls = mcp::beast_tls{cert, key};
            } else
                throw std::invalid_argument("Usage: [--port N] [--address IP] [--origin URL] [--token-env NAME] [--timeout-ms N] [--tls CERT KEY]");
        }
        boost::asio::io_context io;
        mcp::beast_listener listener(io, dispatcher, options, tls);
        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&](const auto&, int) { listener.stop(); });
        std::cerr << "Listening " << (tls ? "https" : "http") << "://" << options.address
                  << ':' << listener.port() << options.path << std::endl;
        io.run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Could not serve MCP: " << error.what() << '\n';
        return 1;
    }
}
