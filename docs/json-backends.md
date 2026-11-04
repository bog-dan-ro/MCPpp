# JSON adapter contract

The core uses the `mcp::json_backend` concept in [`json.hpp`](../include/mcp++/json.hpp). Select an
adapter with `mcp::server<Backend>`; the umbrella header does not include one. CMake links JSON
dependencies to the examples and test consumers.

`Backend::value` must be a default-constructible, copyable, owning DOM value. Copies must support
independent modification. The compile-time concept checks the signatures below; their semantics
remain the adapter author's responsibility.

| Static operation | Contract |
| --- | --- |
| `parse(string_view) -> value` | Consume one complete strict JSON value, reject trailing non-whitespace and invalid UTF-8, translate syntax/range failures to `mcp::parse_error` |
| `dump(const value&) -> string` | Produce strict UTF-8 JSON without literal newlines; reject invalid output rather than silently corrupting it |
| `kind(const value&) -> mcp::json_kind` | Distinguish object, array, string, boolean, signed integer, unsigned integer, floating number, null |
| `object()`, `array() -> value` | Produce empty containers |
| `string(string_view) -> value` | Copy the complete string, including embedded NULs |
| `integer(int64_t)`, `number(double)`, `boolean(bool) -> value` | Construct exactly the named JSON category; finite doubles only |
| `find(const value&, string_view) -> const value*` | Null for a non-object or absent property; otherwise borrow the field until the DOM changes |
| `keys(const value&) -> vector<string>` | Return all object property names, once each; called only on objects |
| `as_string(const value&) -> string_view` | Borrow the complete stored string; its DOM owner must remain unchanged |
| `as_i64`, `as_u64`, `as_double`, `as_bool` | Access checked categories without coercing strings/booleans to numbers; `as_double` also accepts both integer kinds |
| `set(value&, string_view, value) -> void` | Copy the key and replace/add an object property with an owned value |
| `append(value&, value) -> void` | Append an owned value to an array |

The core checks category and integer bounds before numeric access and narrowing. It materializes
string inputs into owned tuple entries before constructing views, and invokes synchronously while
those entries live. Parser number overflow must be rejected, rather than turned into infinity or
null. Duplicate JSON object keys follow the selected parser's policy (the included adapters keep the
last occurrence).

The Boost.JSON adapter uses precise floating point parsing, rejects non-finite values in its parsed
DOM, and validates serialized JSON with Boost's parser to catch invalid UTF-8 in user-returned
strings. This adds a validation pass to serialization. The nlohmann adapter uses its strict parser
and serializer. The Qt adapter (`mcp::qt_backend` in
[`include/mcp++/json/qt.hpp`](../include/mcp++/json/qt.hpp)) uses `Qt6::Core` UTF-8 validation and
provides `QJsonValue` conversion helpers alongside an owning DOM that preserves exact 64-bit integer
categories and stable borrowed pointers/views.

To add another adapter, include its header in a consumer, instantiate the server with its type, and
reuse the schema/dispatch and subprocess suites. Reflection, invocation, schema and protocol code do
not need library-specific changes.
