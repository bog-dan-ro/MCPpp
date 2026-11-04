# Verification history

These are historical results from the configurations described below. For the
current commands and conformance scope, see [validation.md](validation.md) and
[ci.md](ci.md).

## Baseline — 2026-11-01

The baseline used Linux x86-64, GCC 16.2.0 (Debian 16.2.0-3) with
`-std=c++26 -freflection`, CMake 4.3.4, Ninja, Python 3.14.8, Boost 1.92.0 and,
for the second backend, nlohmann/json 3.11.3.

| Configuration | Result |
| --- | --- |
| Boost.JSON, Release | 5/5 CTest entries |
| Boost.JSON and nlohmann/json, Debug | 8/8 CTest entries |
| Both backends with ASan/UBSan, Debug | 8/8 CTest entries; leak detection disabled due to a ptrace restriction |
| Core only | Configured and built without JSON or unit-test packages |

The standalone reflection test checked method and parameter names and types,
return types, and invocation. The suites covered schema generation, dispatch,
stdio framing, supported scalar types, invalid arguments and results, tool
visibility, duplicate registration and ten expected compilation failures.
CTest uses `try_compile` for the latter and saves compiler logs in the build
tree's `compile_fail/` directory.

The official schema, Inspector and conformance suite were not available for the
baseline. The later checks below supersede that limitation. The core-only build
contains no executable, so it did not test a custom backend at runtime.

## HTTP(S) and fulfillment — 2026-11-01

```sh
cmake -S . -B build-http -G Ninja -DCMAKE_CXX_COMPILER=g++-16 \
  -DMCPP_WITH_BEAST=ON -DMCPP_WITH_NLOHMANN=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-http -j 3
ctest --test-dir build-http --output-on-failure -j 4
```

This configuration passed **21/21 CTest entries** with Boost 1.92.0 and
OpenSSL 3.6.5. It covered both JSON backends, HTTP handler tests, calculator
HTTP/HTTPS tests and fulfillment workflows over stdio, HTTP and HTTPS. Clients
verified the generated test certificate and hostname. The HTTP tests covered
header validation, Origins, authorization, content negotiation, errors,
keep-alive, limits, deadlines and shutdown.

An HTTP-only Release build with `MCPP_WITH_BEAST_TLS=OFF` passed its smoke
test; `ldd` showed no `libssl` or `libcrypto` dependency. The original stdio
configuration passed 10/10 entries with both backends and the added fulfillment
tests. The default-backend HTTP sanitizer build passed 12/12 entries with
ASan/UBSan and leak detection disabled.

Manual stdio and HTTPS calculator calls returned 5, and an HTTP call listed both
calculator tools. A fulfillment run produced the
[recorded exchanges](fulfillment-exchanges.json).

## Schema, clients, authorization and shutdown — 2026-11-01

The official schema was added at commit
`d0e246ea3c69eafaa2806bc4cd28025703ba2c2b`; its hash and license are recorded in
`tests/protocol/`. Python and npm verification dependencies were installed
separately using the locks in `tools/verification/`.

The HTTP build with both JSON backends, schema validation and external checks
passed **30/30 CTest entries**. Inspector 2.13.0 listed and called tools over
stdio and bearer-authenticated HTTP/HTTPS with certificate verification.
Conformance 0.2.0-alpha.12 ran five scenarios: **45 checks passed and seven
unsupported checks failed** in the original reports. Three scenarios passed in
full. The [conformance summary](conformance-summary.json) records the official
exit codes and exclusions.

The authorization tests covered missing, malformed and duplicate credentials,
and the requirement for TLS and an authorization policy on non-loopback
bindings. Shutdown tests ran a 1.2-second tool across a 200-ms network deadline:
the active call completed, the idle peer timed out and queued work received
HTTP 503 without executing.

The default-backend sanitizer build subsequently passed **16/16 entries** with
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`; the earlier ptrace restriction no
longer applied. The TLS-disabled Release build also passed socket and bearer
authentication tests.

## Packaging and CI — 2026-11-04

The CI container was built and run locally using the commands in
[ci.md](ci.md). Its profiles passed **32/32 verified**, **18/18 sanitizer**,
**13/13 HTTP-only** and **4/4 core-only** CTest entries. The verified profile
included schema validation, Inspector and the scoped conformance checks above.
The workflow passed `actionlint` 1.7.12; a hosted run had not yet been made at
the time of this record.

Each profile included installed-package and FetchContent consumers. The tests
moved the install prefix, built core consumers without transport dependencies,
and compiled Beast consumers across two translation units. The image pins its
base digest and GCC package; other system packages come from Debian testing.
