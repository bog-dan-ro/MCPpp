// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#pragma once

#include <mcp++/beast.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/beast/core.hpp>
#include <atomic>
#if defined(MCPP_BEAST_TLS)
#include <boost/asio/ssl.hpp>
#include <boost/beast/ssl.hpp>
#endif

namespace mcp::detail::beast_transport {
namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = net::ip::tcp;

struct connection {
    virtual void cancel() = 0;
    virtual void drain() = 0;
    virtual ~connection() = default;
};

struct service_state {
    beast_handler handler;
    beast_options options;
    std::vector<std::weak_ptr<connection>> connections;
    net::any_io_executor worker;
    std::atomic_bool stopping = false;
#if defined(MCPP_BEAST_TLS)
    std::unique_ptr<net::ssl::context> tls;
#endif
};

template<class Stream>
class session : public connection, public std::enable_shared_from_this<session<Stream>> {
    // The TLS context must outlive the stream.
    std::shared_ptr<service_state> state_;
    Stream stream_;
    beast::flat_buffer buffer_;
    std::optional<http::request_parser<http::string_body>> parser_;
    beast_response response_;
    bool closing_ = false;
    bool processing_ = false;
    bool writing_ = false;
    bool draining_ = false;

    void deadline() { beast::get_lowest_layer(stream_).expires_after(state_->options.io_timeout); }
    void read() {
        parser_.emplace();
        parser_->body_limit(state_->options.body_limit);
        parser_->header_limit(state_->options.header_limit);
        deadline();
        http::async_read(stream_, buffer_, *parser_,
            [self = this->shared_from_this()](beast::error_code ec, std::size_t) { self->on_read(ec); });
    }
    void on_read(beast::error_code ec) {
        if (closing_) return;
        if (ec == http::error::end_of_stream) return shutdown();
        if (ec) {
            if (ec == beast::error::timeout || ec == net::error::operation_aborted) return cancel();
            // A framing failure cannot be resumed on this connection.
            auto status = ec == http::error::body_limit ? http::status::payload_too_large :
                ec == http::error::header_limit ? http::status::request_header_fields_too_large : http::status::bad_request;
            response_ = beast_response{status, 11};
            response_.keep_alive(false);
            response_.prepare_payload();
        } else {
            processing_ = true;
            beast::get_lowest_layer(stream_).expires_never();
            auto keep_running = net::make_work_guard(stream_.get_executor());
            auto self = this->shared_from_this();
            net::post(state_->worker,
                [self, request = parser_->release(), keep_running = std::move(keep_running)]() mutable {
                    beast_response response;
                    if (self->state_->stopping.load()) {
                        response = beast_response{http::status::service_unavailable, 11};
                        response.keep_alive(false);
                        response.prepare_payload();
                    } else {
                        response = self->state_->handler(request);
                    }
                    net::post(self->stream_.get_executor(),
                        [self, response = std::move(response), keep_running = std::move(keep_running)]() mutable {
                            self->processing_ = false;
                            if (self->closing_) return;
                            self->response_ = std::move(response);
                            if (self->draining_) self->response_.keep_alive(false);
                            self->write();
                        });
                });
            return;
        }
        write();
    }
    void write() {
        writing_ = true;
        deadline();
        http::async_write(stream_, response_,
            [self = this->shared_from_this()](beast::error_code error, std::size_t) {
                if (self->closing_) return;
                self->writing_ = false;
                if (error) return self->cancel();
                if (self->draining_ || !self->response_.keep_alive()) return self->shutdown();
                self->read();
            });
    }
    void shutdown() {
        if (closing_) return;
#if defined(MCPP_BEAST_TLS)
        if constexpr (!std::is_same_v<Stream, beast::tcp_stream>) {
            closing_ = true;
            deadline();
            stream_.async_shutdown([self = this->shared_from_this()](beast::error_code) { self->cancel(); });
            return;
        }
#endif
        cancel();
    }
public:
    template<class... Args>
    session(std::shared_ptr<service_state> state, Args&&... args)
        : state_(std::move(state)), stream_(std::forward<Args>(args)...) {}
    void start() {
#if defined(MCPP_BEAST_TLS)
        if constexpr (!std::is_same_v<Stream, beast::tcp_stream>) {
            deadline();
            stream_.async_handshake(net::ssl::stream_base::server,
                [self = this->shared_from_this()](beast::error_code ec) {
                    if (ec) self->cancel(); else self->read();
                });
            return;
        }
#endif
        read();
    }
    void cancel() override {
        closing_ = true;
        beast::error_code ignored;
        beast::get_lowest_layer(stream_).socket().cancel(ignored);
        beast::get_lowest_layer(stream_).socket().shutdown(tcp::socket::shutdown_both, ignored);
        beast::get_lowest_layer(stream_).socket().close(ignored);
    }
    void drain() override {
        draining_ = true;
        if (!processing_ && !writing_) cancel();
    }
};
} // namespace mcp::detail::beast_transport

namespace mcp {
struct beast_listener::impl : std::enable_shared_from_this<impl> {
    using tcp = boost::asio::ip::tcp;
    using service_state = detail::beast_transport::service_state;
    template<class Stream> using session = detail::beast_transport::session<Stream>;

    tcp::acceptor acceptor;
    // The listener and its worker outlive all callbacks; io work guards keep
    // run() alive until worker results have been delivered during shutdown.
    boost::asio::thread_pool worker{1};
    std::shared_ptr<service_state> state;
    std::uint16_t bound_port;

    impl(boost::asio::io_context& io, beast_handler handler, beast_options options, std::optional<beast_tls> tls)
        : acceptor(io), state(std::make_shared<service_state>()) {
        if (options.io_timeout.count() <= 0 || !options.body_limit || !options.header_limit ||
            !options.connection_limit || options.path.empty() || options.path.front() != '/')
            throw std::invalid_argument("Invalid Beast listener options");
        state->handler = std::move(handler);
        state->options = std::move(options);
        state->worker = worker.get_executor();
        auto address = boost::asio::ip::make_address(state->options.address);
        if (!address.is_loopback() && (!tls || !state->options.authorize))
            throw std::invalid_argument("Non-loopback listeners require TLS and an authorization policy");
        if (tls) {
#if defined(MCPP_BEAST_TLS)
            state->tls = std::make_unique<boost::asio::ssl::context>(boost::asio::ssl::context::tls_server);
            if (!SSL_CTX_set_min_proto_version(state->tls->native_handle(), TLS1_2_VERSION))
                throw std::runtime_error("Could not configure TLS minimum version");
            state->tls->use_certificate_chain_file(tls->certificate_chain_file);
            state->tls->use_private_key_file(tls->private_key_file, boost::asio::ssl::context::pem);
            if (!SSL_CTX_check_private_key(state->tls->native_handle()))
                throw std::invalid_argument("TLS certificate and private key do not match");
#else
            throw std::invalid_argument("TLS support requires MCPP_WITH_BEAST_TLS=ON");
#endif
        }
        tcp::endpoint endpoint(address, state->options.port);
        acceptor.open(endpoint.protocol());
        acceptor.set_option(boost::asio::socket_base::reuse_address(true));
        acceptor.bind(endpoint);
        acceptor.listen(boost::asio::socket_base::max_listen_connections);
        bound_port = acceptor.local_endpoint().port();
    }
    void accept() {
        acceptor.async_accept([self = shared_from_this()](boost::beast::error_code ec, tcp::socket socket) {
            if (!self->acceptor.is_open()) return;
            if (ec) { self->stop(); return; }
            auto& state = self->state;
            std::erase_if(state->connections, [](const auto& weak) { return weak.expired(); });
            if (state->connections.size() < state->options.connection_limit) {
#if defined(MCPP_BEAST_TLS)
                if (state->tls) {
                    auto client = std::make_shared<session<boost::beast::ssl_stream<boost::beast::tcp_stream>>>(state, std::move(socket), *state->tls);
                    state->connections.push_back(client);
                    client->start();
                } else
#endif
                {
                    auto client = std::make_shared<session<boost::beast::tcp_stream>>(state, std::move(socket));
                    state->connections.push_back(client);
                    client->start();
                }
            }
            // The socket closes here if the connection limit was reached.
            self->accept();
        });
    }
    void stop() {
        state->stopping.store(true);
        boost::beast::error_code ignored;
        acceptor.close(ignored);
        for (const auto& weak : state->connections) if (auto client = weak.lock()) client->drain();
    }
};

inline beast_listener::beast_listener(boost::asio::io_context& io, beast_handler handler, beast_options options, std::optional<beast_tls> tls)
    : impl_(std::make_shared<impl>(io, std::move(handler), std::move(options), std::move(tls))) { impl_->accept(); }

inline beast_listener::~beast_listener() { stop(); }
inline std::uint16_t beast_listener::port() const { return impl_->bound_port; }
inline void beast_listener::stop() {
    boost::asio::dispatch(impl_->acceptor.get_executor(), [state = impl_] { state->stop(); });
}
} // namespace mcp
