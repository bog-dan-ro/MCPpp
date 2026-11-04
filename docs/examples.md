# Try the MCP examples

Build the examples with CMake, then use the Python clients below to exercise them.

## Calculator

`calculator` serves stdio. With Beast enabled, `calculator_http` serves the same object over HTTP or
HTTPS. Both expose `add` and `printMessage`; the public unannotated helper and private annotated
method stay inaccessible.

```sh
python3 tests/stdio_smoke.py build-http/calculator
python3 tests/http_smoke.py build-http/calculator_http
python3 tests/http_smoke.py build-http/calculator_http \
  --tls build-http/test-tls/cert.pem build-http/test-tls/key.pem
```

## Fulfillment: two objects and seven tools

[`Stockroom` and `Fulfillment`](../examples/fulfillment.hpp) share domain state through an ordinary
C++ reference. One dispatcher exposes both objects with two `expose()` calls. The tool signatures
use supported scalar types.

| Tool | Purpose |
| --- | --- |
| `serviceInfo()` | Zero-argument const method, string result |
| `available(sku)` | Available quantity, integer result |
| `quote(sku, quantity, discount)` | Integer and double inputs, double result |
| `reserve(sku, quantity)` | Owns a string input, creates an explicit reservation handle |
| `confirm(reservationId)` | First confirmation returns true; an immediate retry returns false |
| `cancel(reservationId)` | Void result; restores stock once, rejects shipped reservations |
| `shippingLabel(reservationId, recipient, express)` | Uses string views, an owning string, and a bool; requires a confirmed reservation |

The application starts with ten widgets at 12.50 and four gadgets at 30.00. Quotes allow a discount
from 0 to 0.5. Reservations hold inventory immediately. Shipping consumes a confirmed reservation;
it cannot be shipped twice. Invalid domain operations become sanitized tool errors and leave the
process usable. The public `claimForShipping` integration helper is intentionally unannotated and
cannot be called remotely.

Reservation handles connect calls explicitly. No state is inferred from an HTTP connection, stdio
stream, client identity, or MCP session. This in-memory demo has no persistence; its sequential IDs
are not credentials or authorization tokens.

```sh
# Stdio workflow
python3 tests/fulfillment_smoke.py build-http/fulfillment
# HTTP workflow
python3 tests/fulfillment_smoke.py build-http/fulfillment_http --http
# Verified HTTPS workflow
python3 tests/fulfillment_smoke.py build-http/fulfillment_http --http \
  --tls build-http/test-tls/cert.pem build-http/test-tls/key.pem
```

The tests each launch a fresh example and perform successful and unsuccessful transitions. The
`_nlohmann` and `_qt` executables run the same workflows when those adapters are enabled.

## Recorded run

An HTTP fulfillment run produced the results below. The full request/response pairs are in
[`fulfillment-exchanges.json`](fulfillment-exchanges.json).

```text
available(widget)                               -> 10
quote(widget, 2, 0.1)                           -> 22.5
reserve(widget, 2)                              -> "reservation-1"
shippingLabel(reservation-1, Ada Lovelace, true) -> tool error (not confirmed)
confirm(reservation-1)                          -> true
shippingLabel(reservation-1, Ada Lovelace, true) -> "EXPRESS reservation-1 -> Ada Lovelace"
cancel(reservation-1)                           -> tool error (already shipped)
claimForShipping(reservation-1)                 -> JSON-RPC -32602 (not exposed)
available(widget)                               -> 8
```

The process suites additionally verify cancellation restores held inventory, repeated cancellation
does not add inventory twice, insufficient stock cannot be reserved, and confirmation retries return
false.
