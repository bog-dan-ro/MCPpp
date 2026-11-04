# Reproduce protocol validation and real-client checks

The optional checks use Python for JSON Schema validation and npm for the
official Inspector and conformance tools. Neither is a library dependency.

## Install the pinned verification dependencies

```sh
python3 -m venv build/verification-venv
build/verification-venv/bin/python -m pip install -r tools/verification/requirements.txt
npm ci --prefix tools/verification --ignore-scripts --no-audit --no-fund
```

The npm lockfile pins **Inspector 2.13.0** and **conformance 0.2.0-alpha.12**,
including transitive package integrity hashes. The stable conformance 0.1.16
does not understand the 2026-07-28 revision. Node 24.21.0 was used here; the
Inspector requires Node 22.19.0 or newer. Installation is explicit; ordinary
CMake configuration/build never downloads packages.

```sh
cmake -S . -B build-verified -G Ninja \
  -DCMAKE_CXX_COMPILER=g++-16 -DCMAKE_BUILD_TYPE=Debug \
  -DMCPP_WITH_BEAST=ON -DMCPP_WITH_NLOHMANN=ON \
  -DMCPP_VALIDATE_PROTOCOL=ON -DMCPP_EXTERNAL_CHECKS=ON \
  -DPython3_EXECUTABLE="$PWD/build/verification-venv/bin/python"
cmake --build build-verified -j 3
ctest --test-dir build-verified --output-on-failure -j 4
```

The verified build runs 32 CTest entries, including installed-package and
FetchContent consumer projects. Both public library targets are header-only;
the Beast consumer uses two translation units to check linkage.

If the npm tools are installed elsewhere, set the absolute paths in
`MCPP_INSPECTOR_EXECUTABLE` and `MCPP_CONFORMANCE_EXECUTABLE`. The external
checks are opt-in; the standard suites still use Python's standard library.

## Official schema

The schema is stored under `tests/protocol/`, pinned to spec repository commit
`d0e246ea3c69eafaa2806bc4cd28025703ba2c2b` and SHA-256
`ef70b61f99b6d2e5e3b46863822eab08dff6a45bedc7a08914e0e5b133f40203`.
The downloaded bytes are unmodified. Tests verify that hash and use
`jsonschema` 4.26.0 with Draft 2020-12. Responses are checked against
`DiscoverResultResponse`, `ListToolsResultResponse`, `CallToolResultResponse`,
or `JSONRPCErrorResponse`, rather than the permissive generic result union.

Validation covers all calculator/fulfillment success and error responses over
stdio, HTTP and HTTPS, including direct inline calls, input validation errors,
protocol errors, and bearer-authenticated calls. HTTP policy rejections with an
empty body are checked by status/header assertions.

## Real Inspector clients

The CTest `inspector_smoke` and `inspector_https_smoke` entries run the official
CLI in explicit modern mode. They list tools and call `add(2, 3)` over stdio and
bearer-authenticated HTTP(S). HTTPS uses `NODE_EXTRA_CA_CERTS` to trust the
generated test certificate; verification is enabled. Test credentials are
public fixture strings, and the Inspector uses a build-local storage directory
and in-memory secret store instead of user configuration.

Recorded client results are written under `inspector-http/` and
`inspector-https/` in the selected build directory.

## Official conformance scope

Five unmodified upstream scenarios are run at the 2026-07-28 wire revision:

| Scenario | Passing checks | Explicitly unsupported checks |
| --- | ---: | ---: |
| `server-stateless` | 21 | 4 |
| `tools-list` | 4 | 0 |
| `dns-rebinding-protection` | 2 | 0 |
| `caching` | 4 | 3 |
| `http-header-validation` | 14 | 0 |

**45 checks pass; seven are unsupported. This is not a full conformance pass.**
Four require upstream diagnostic tools for client-capability requests, streaming
elicitation, or protocol log notifications. Three require the unadvertised
prompts/resources APIs. They are listed with reasons in
`tools/verification/conformance-scope.json`.
Each stateless exemption must be reported `untestable` by the official runner;
each unsupported caching endpoint must return method-not-found. Any other
failing check fails CTest.

The original official exit codes and complete check reports remain under
`conformance-results/`. The summary preserves unsupported failures instead of
rewriting them into passes. The DNS test client explicitly allowlists its own
bound origin; production Origin defaults are unchanged.

SSE and HTTP/2 are excluded. The full requirements set also covers prompts,
resources, media and client interactions that MCP++ does not implement.

## Memory and shutdown checks

```sh
cmake -S . -B build-http-sanitize -G Ninja \
  -DCMAKE_CXX_COMPILER=g++-16 -DMCPP_WITH_BEAST=ON \
  -DMCPP_SANITIZERS=ON -DMCPP_VALIDATE_PROTOCOL=ON \
  -DPython3_EXECUTABLE="$PWD/build/verification-venv/bin/python" \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build-http-sanitize -j 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
  ctest --test-dir build-http-sanitize --output-on-failure -j 4
```

These tests cover network connections, TLS shutdown, bearer checks, object and
string lifetimes, and a tool call that outlasts a network deadline. The network
loop stays responsive, active execution completes, and queued work receives 503
during shutdown. A tool that never returns can prevent shutdown.
