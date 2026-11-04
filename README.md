# MCP++

MCP++ is a header-only C++26 library for exposing annotated C++ methods as MCP
tools. It uses static reflection to discover methods, generate input
schemas and dispatch calls; there are no registration macros or generated code.

It implements MCP **2026-07-28** discovery, tool listing and tool calls over
stdio, with an optional Boost.Beast HTTP(S) transport. The core is independent
of the JSON library. Adapters are included for Boost.JSON, nlohmann/json and Qt.

> **AI disclosure:** A lot of this code was written by AI. We gave Codex access
> to C++26 reflection and asked it to build an MCP server; it responded by
> reflecting on its tools. If the machines take over, they will first have to
> satisfy the compiler and convince the test suite their arguments are valid.

## Build and test

The tested configuration uses Linux x86-64, GCC 16.2.0 (Debian 16.2.0-3) with
`-std=c++26 -freflection`, CMake 4.3.4, Ninja, Python 3.14.8 and Boost 1.92.0
(JSON and Unit Test Framework). CMake 3.30 or newer is required. Configuration
checks that the compiler supports reflection and annotations; it does not
download dependencies.

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_CXX_COMPILER=g++-16 -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
ctest --test-dir build --output-on-failure
```

This builds `calculator` and `fulfillment` with Boost.JSON. The eight CTest
entries cover reflection, schema generation, dispatch, the examples, ten
expected compilation failures, and installed-package and FetchContent
consumers. Assertions remain enabled in Release tests. The compiler output for
rejected cases is saved under `build/compile_fail/`.

To also test the nlohmann/json 3.11.3 adapter:

```sh
cmake -S . -B build-both -G Ninja -DCMAKE_CXX_COMPILER=g++-16 \
  -DMCPP_WITH_NLOHMANN=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-both -j 4
ctest --test-dir build-both --output-on-failure
```

Those executables have a `_nlohmann` suffix. Set `MCPP_WITH_BOOST=OFF` to use
only nlohmann/json; the unit tests still need Boost.Test. To build and test the
Qt adapter, set `MCPP_WITH_QT=ON` and install Qt 6 Core; its executables have a
`_qt` suffix. For a core-only configuration, use `-DBUILD_TESTING=OFF`,
`-DMCPP_WITH_BOOST=OFF` and `-DMCPP_WITH_NLOHMANN=OFF`. It defines `mcp::mcpp`
without requiring a JSON library or Boost.Test.

## HTTP(S)

```sh
cmake -S . -B build-http -G Ninja -DCMAKE_CXX_COMPILER=g++-16 \
  -DMCPP_WITH_BEAST=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-http -j 3
ctest --test-dir build-http --output-on-failure
build-http/calculator_http --port 8080
# Or run the seven-tool fulfillment example:
build-http/fulfillment_http --port 8080
```

The `mcp::beast` target supplies the Beast/Asio dependencies. TLS is enabled by
default and needs OpenSSL 3.0 or newer (tested with 3.6.5). Set
`MCPP_WITH_BEAST_TLS=OFF` for HTTP without OpenSSL. The core and JSON adapters
do not depend on networking.

The examples listen on `127.0.0.1` by default. `--port 0` selects an available
port and prints its URL to stderr. Use `--tls CERT.pem KEY.pem` for HTTPS,
`--origin URL` to allow an Origin, `--timeout-ms N` to set network deadlines,
and `--token-env NAME` to read a bearer token from the environment.
`--address IP` selects another bind address; non-loopback bindings require TLS
and an authorization policy.

See the [HTTP guide](docs/http.md) for embedding, policy hooks and limits, and
the [fulfillment walkthrough](docs/examples.md) for a two-object example with
recorded exchanges.

## Exposing methods

Use `FetchContent` or an installed `find_package(MCPpp CONFIG REQUIRED)` to
consume the library. Embedded builds default to no tests, examples or install
rules. The [consumer guide](docs/consuming.md) has complete CMake examples.

```cpp
#include <mcp++/mcp.hpp>
#include <mcp++/json/boost.hpp>
#include <iostream>

struct MyStruct {
    [[=mcp::tool]]
    void printMessage(std::string_view message) {
        std::cerr << message << '\n';
    }

    [[=mcp::tool]]
    int add(int a, int b) { return a + b; }

    void privateMemberFunction() {} // public, but not exposed
};

int main() {
    MyStruct object;
    mcp::server<mcp::boost_backend> server{"MyServer"};
    server.expose(object);
    return server.run(mcp::stdio{});
}
```

Link `mcp::mcpp` and `Boost::json` for this example. The complete
[calculator](examples/calculator.cpp) also checks arithmetic overflow.
Registered objects are borrowed: they must outlive the server and its calls.
Register them before serving requests, and reserve stdout for protocol frames;
write diagnostics to stderr or an internal buffer.

An annotation can include a description, for example
`[[=mcp::tool_t{"Add two integers"}]]`. It holds up to 511 bytes;
`[[=mcp::tool]]` works when no description is needed.

For direct use, `server.list_tools()` returns the backend's JSON array and
`server.call_tool(name, arguments)` returns a tool result or throws
`mcp::protocol_error` for an unknown tool. `server.dispatch(value)` handles a
parsed request and returns `std::optional<Backend::value>` (empty for a
notification); `server.process_line(text)` also parses JSON.
`server.run({input, output})` accepts injected streams, and `mcp::describe<T>()`
returns the compile-time tool catalog.

## JSON and supported types

Select an adapter with `mcp::server<Backend>`. The umbrella header includes no
JSON library. See the [adapter contract](docs/json-backends.md) and the
[Boost](include/mcp++/json/boost.hpp), [nlohmann](include/mcp++/json/nlohmann.hpp)
and [Qt](include/mcp++/json/qt.hpp) implementations.

Tools accept `int`, `double`, `bool`, `std::string` and `std::string_view` by
value; return types also include `void`, but not `std::string_view`. Input views
refer to strings owned for the duration of the call and must not be retained.
Each `mcp::codec<T, Backend>` defines both schema generation and decoding.

Schemas use JSON Schema 2020-12, including C++ `int` and finite `double` bounds.
Integral JSON numbers such as `2.0` are accepted for `int`; fractional values,
booleans, strings, overflow and implicit coercions are rejected. Floating-point
parsing has the usual double precision limits.

A successful scalar result includes `structuredContent: {"result": value}` and
a text block with the JSON rendering of that object. A successful `void` method
returns the text `Completed` without `structuredContent`. All tool results
include `resultType: "complete"` and `isError`.

## Protocol behavior

[Recorded exchanges](docs/exchanges.json) cover discovery, listing and
`add(2, 3)`. Regenerate them with:

```sh
python3 tests/stdio_smoke.py build/calculator --write-fixtures docs/exchanges.json
```

For a single call, start `build/calculator` and send this line on stdin:

```json
{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"add","arguments":{"a":2,"b":3},"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientCapabilities":{}}}}
```

The result contains `structuredContent: {"result":5}`. No handshake or prior
discovery is required. Each request supplies the protocol version and client
capabilities. Client identity is optional; if supplied, it must include name
and version strings. Success responses identify the server in `result._meta`.

| Condition | Response |
| --- | --- |
| Invalid JSON, UTF-8, or numeric parsing overflow | JSON-RPC `-32700` |
| Invalid message envelope or ID; batch array | JSON-RPC `-32600` |
| Unknown RPC method | JSON-RPC `-32601` |
| Missing/invalid request metadata, unknown tool, malformed call shape, invalid cursor | JSON-RPC `-32602` |
| Unsupported protocol version | `-32022`, with `data.supported` and `data.requested` |
| Tool argument validation failure | Tool result with `isError: true` and actionable text |
| User exception or invalid string output | Tool result with `isError: true`; exception details suppressed |
| Non-finite double return | Tool result with `isError: true` |

Valid string and integer IDs are preserved, including unsigned 64-bit IDs.
Errors omit invalid or unreadable IDs rather than emitting null. Notifications
are ignored without invocation or reply. Stdio requests run synchronously until
EOF, with one flushed response per line.

Tools are returned in lexical order in a single page; any supplied cursor is
invalid. Discovery and listing include `ttlMs: 0` and `cacheScope: "private"`.
The only advertised capability is `tools: {}`; list-change notifications are
not advertised.

## Limitations

- Only public, annotated direct ordinary member functions are exposed; base
  classes are not traversed. Const and `noexcept` methods work on non-const
  exposed objects. Private annotated and public unannotated methods are hidden.
- Annotated overloads with the same name fail at compile time. An unannotated
  overload may coexist with an annotated one. Name collisions between objects
  throw before the registry changes.
- Accessible member function templates cause a compile-time diagnostic even
  when unannotated: standard annotation queries cannot inspect them. Other
  member templates are skipped; private templates are excluded by the access
  context.
- Static, deleted, operator, special-member, ref-qualified, volatile, variadic
  and explicit-object tools are rejected, as are unnamed parameters and
  unsupported types. All arguments are required, including parameters with
  C++ defaults.
- References, pointers, records, containers, enums, optional values,
  string-view returns, async or concurrent tools, cancellation, MRTR
  continuations, subscriptions and batch messages are unsupported. Stdio lines
  have no size limit; HTTP has header, body and connection limits.
- HTTP supports HTTP/1.1 JSON responses, optional TLS, bearer credentials and an
  application authorization hook. One worker serializes tool calls while the
  network loop remains responsive. Shutdown drains active execution and rejects
  queued calls; network deadlines cannot interrupt C++ code.
- SSE, HTTP/2, CORS preflight, OAuth, sessions, resources, prompts and the old
  `initialize` handshake are unsupported. Clients that only implement the old
  handshake are incompatible. Full MCP conformance is not claimed.

## Verification

The [validation guide](docs/validation.md) covers the pinned official schema,
Inspector clients, scoped conformance checks and sanitizer runs. The verified
configuration passes 32 CTest entries across both JSON backends. Inspector
lists and calls tools over stdio and bearer-authenticated HTTP/HTTPS. Five
official scenarios have 45 passing checks and seven unsupported checks; the
original reports retain those failures. See [verification history](docs/verification.md)
and the [CI guide](docs/ci.md) for details.

## License

MIT. See [LICENSE](LICENSE). Project-owned sources carry MIT SPDX notices; the
vendored MCP schema retains its upstream license. [Reference notes](docs/reference/README.md)
link to the relevant specifications and documentation.
