# Boost.Beast HTTP(S) adapter

Enable `MCPP_WITH_BEAST=ON` and link `mcp::beast` along with your JSON backend. The target supplies
Beast/Asio, threads and, when enabled, OpenSSL. Include `<mcp++/beast.hpp>` for the transport. See
the [consumer guide](consuming.md) for CMake examples.

```cpp
#include <mcp++/beast.hpp>
#include <mcp++/json/boost.hpp>
#include <boost/asio/signal_set.hpp>
#include <csignal>

Calculator object;
mcp::server<mcp::boost_backend> dispatcher("Calculator");
dispatcher.expose(object);

boost::asio::io_context io;
mcp::beast_options options;
options.port = 8080;
mcp::beast_listener listener(io, dispatcher, options);
boost::asio::signal_set signals(io, SIGINT, SIGTERM);
signals.async_wait([&](auto, int) { listener.stop(); });
io.run();
```

The [HTTP calculator](../examples/calculator_http.cpp) uses the same object as the stdio example.
The listener starts accepting in its constructor; `port()` reports the bound port. The dispatcher,
exposed objects and `io_context` must outlive all callbacks. Run the context on **one thread**. A
separate worker serializes the listener's request handlers, so a blocking tool does not block
network deadlines or signal handling. Synchronize access if the dispatcher is also used elsewhere or
by another listener.

`stop()` closes the acceptor and idle sockets, drains an active handler and its response, and
rejects queued work with HTTP 503. I/O work guards keep `io.run()` alive until worker results are
delivered. Arbitrary synchronous C++ code cannot be killed safely: a tool that never returns can
still prevent complete process shutdown. Supervisors may impose a process-level deadline if the
application needs one.

`mcp::make_beast_handler(dispatcher, options)` returns a `beast_handler` that converts a Beast HTTP
string-body request to a response. It checks HTTP policy and MCP mirror headers before dispatch.
When embedding it in another acceptor, the application is responsible for parser limits, network
deadlines and serialized access to exposed objects.

## Options and application policy

| Option | Default / meaning |
| --- | --- |
| `address` | `127.0.0.1`; explicit IP address to bind |
| `port` | `8080`; zero requests an ephemeral port |
| `path` | `/mcp`; exact target, without query matching |
| `allowed_origins` | Empty; requests without Origin are allowed, present Origin requires an exact allowlist match |
| `authorize` | Application callback receiving the full HTTP request; false returns 401, exception returns a sanitized 500; required on non-loopback bindings |
| `authentication_challenge` | Optional `WWW-Authenticate` value returned with an authorization denial |
| `body_limit` | 1 MiB, enforced by Beast's parser |
| `header_limit` | 16 KiB, enforced by Beast's parser |
| `io_timeout` | 15 seconds for handshake, complete request read, response write, or TLS shutdown |
| `connection_limit` | 64 active connections; further accepted sockets are closed |

Origin checks happen before any dispatch, with duplicate, unlisted, and `null` Origins rejected
unless explicitly allowlisted. This is input validation, not browser CORS support: the adapter does
not implement OPTIONS preflight. Request bodies and credentials are not logged by the adapter.

## Bearer credentials and remote bindings

```cpp
options.authorize = mcp::bearer_token_policy(token_from_secret_source);
options.authentication_challenge = "Bearer realm=\"mcp\"";
```

The policy requires exactly one Authorization header with the Bearer scheme and the configured
credential. Comparison avoids an early exit on a matching token prefix. Empty/malformed configured
tokens are rejected. Denials return 401 and the configured challenge; credentials are not reflected
into errors or logs. An application can replace the callback with its own authorization checks.

The example's `--token-env NAME` reads a credential without placing it in command arguments or URLs.
Use TLS when transmitting credentials beyond loopback. The listener rejects non-loopback addresses
unless both TLS and an authorization callback are configured. This is a pre-shared credential
policy, not an OAuth authorization server, token issuer, user store, or scope system. Applications
own credential provisioning and rotation.

## MCP HTTP contract

This is the JSON-response subset of the
[2026-07-28 Streamable HTTP binding](https://modelcontextprotocol.io/specification/2026-07-28/basic/transports/streamable-http).
Each POST contains one JSON-RPC message. Every request includes `Content-Type: application/json`, an
Accept list containing both `application/json` and `text/event-stream`, and the required MCP headers.
The server always chooses an ordinary JSON response; it advertises no streaming features.

| Header | Compared against |
| --- | --- |
| `MCP-Protocol-Version` | `params._meta["io.modelcontextprotocol/protocolVersion"]` |
| `Mcp-Method` | JSON-RPC `method` |
| `Mcp-Name` | `params.name` for tools/call and prompts/get, or `params.uri` for resources/read |

Header names are case-insensitive; values are compared exactly. Required singleton headers with
duplicate, missing, malformed, or mismatching values produce HTTP 400 with JSON-RPC `-32020` before
invocation. `Mcp-Name` supports the standard `=?base64?…?=` sentinel, including UTF-8 names, with
strict padding validation. The core has no `x-mcp-header` annotations; unknown `Mcp-Param-*` headers
are ignored. The resources/prompts RPCs themselves remain unsupported.

| Outcome | HTTP behavior |
| --- | --- |
| Tool success or tool execution/argument-validation error | 200 with the existing MCP tool result |
| Accepted notification | 202, empty body; there are no notification handlers |
| Unknown RPC method | 404 with JSON-RPC `-32601` |
| Unsupported version | 400 with `-32022` and supported/requested version data |
| Malformed JSON or request, invalid params, unknown tool | 400 with the corresponding JSON-RPC error |
| Internal failure | 500 with sanitized JSON-RPC `-32603` |
| GET/DELETE/other verbs on the MCP endpoint | 405, `Allow: POST` |
| Invalid Origin / authorization denial | 403 / 401 |
| Unsupported content type/encoding or Accept list | 415 / 406 |
| Oversized body / headers | 413 / 431; connection closed |
| Incomplete request or TLS handshake exceeds deadline | Socket closed |
| Request queued behind an active call during shutdown | 503, connection closed; no method invocation |

Success and error responses preserve valid request IDs. HTTP-level policy errors can have an empty
body. Notifications have no protocol-version header requirement in this revision; accepted
notifications are ignored without invoking tools. HTTP keep-alive works, but every request
revalidates its own metadata. Session IDs and `Last-Event-ID` have no effect and are never minted or
echoed. Responses set `Cache-Control: no-store`.

## HTTPS

```cpp
mcp::beast_tls tls{"server-chain.pem", "server-key.pem"};
mcp::beast_listener listener(io, dispatcher, options, tls);
```

`MCPP_WITH_BEAST_TLS=ON` enables OpenSSL, server certificate-chain/key loading, key matching, TLS
1.2 or newer, handshake deadlines, and asynchronous TLS shutdown. A plain-HTTP build has no OpenSSL
linkage and rejects a TLS configuration with a clear exception.

To try the development certificate that CTest generated:

```sh
build-http/calculator_http --port 8443 \
  --tls build-http/test-tls/cert.pem build-http/test-tls/key.pem
curl --cacert build-http/test-tls/cert.pem \
  -H 'Content-Type: application/json' \
  -H 'Accept: application/json, text/event-stream' \
  -H 'MCP-Protocol-Version: 2026-07-28' \
  -H 'Mcp-Method: tools/call' -H 'Mcp-Name: add' \
  --data-binary '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"add","arguments":{"a":2,"b":3},"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientCapabilities":{}}}}' \
  https://127.0.0.1:8443/mcp
```

The certificate is a temporary test fixture. Supply your own certificate and key for deployment.

## Current bounds

HTTP/1.1 only, JSON responses only, synchronous tools on one serialized worker, one event-loop
thread, and no CORS preflight, SSE, subscriptions, OAuth, HTTP/2, certificate reload, or forced
execution cancellation.
