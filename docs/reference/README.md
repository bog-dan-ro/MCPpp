# References

## C++ reflection

- [GCC C++ status](https://gcc.gnu.org/projects/cxx-status.html): reflection support and `-freflection`.
- [P2996R13](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p2996r13.html): reflection, access contexts, member queries and splicing.
- [P3394R4](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p3394r4.html): annotations and typed annotation queries. These queries do not accept function template reflections.

## MCP

- [Base protocol](https://modelcontextprotocol.io/specification/2026-07-28/basic): request processing, IDs, errors and the default JSON Schema dialect.
- [Versioning](https://modelcontextprotocol.io/specification/2026-07-28/basic/versioning): discovery and unsupported-version errors.
- [Discovery](https://modelcontextprotocol.io/specification/2026-07-28/server/discover), [tools](https://modelcontextprotocol.io/specification/2026-07-28/server/tools), [stdio](https://modelcontextprotocol.io/specification/2026-07-28/basic/transports/stdio), [pagination](https://modelcontextprotocol.io/specification/2026-07-28/server/utilities/pagination) and [caching](https://modelcontextprotocol.io/specification/2026-07-28/server/utilities/caching).
- [Streamable HTTP](https://modelcontextprotocol.io/specification/2026-07-28/basic/transports/streamable-http): HTTP requests, mirror headers, Origin validation and responses.
- [JSON schema at the pinned commit](https://github.com/modelcontextprotocol/modelcontextprotocol/blob/d0e246ea3c69eafaa2806bc4cd28025703ba2c2b/schema/2026-07-28/schema.json). The local copy, hash and license are in [`tests/protocol/`](../../tests/protocol/).

## Boost.Beast

- [Timeouts](https://www.boost.org/doc/libs/latest/libs/beast/doc/html/beast/using_io/timeouts.html): `tcp_stream` deadlines require asynchronous operations.
- [TLS shutdown](https://www.boost.org/doc/libs/latest/libs/beast/doc/html/beast/using_io/ssl_tls_shutdown.html): asynchronous `close_notify` and HTTP framing.
