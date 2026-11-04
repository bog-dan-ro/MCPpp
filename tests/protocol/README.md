# Pinned MCP protocol schema

`mcp-2026-07-28.schema.json` is the unmodified official JSON Schema, downloaded
from the immutable commit recorded in `schema-source.json`. Its SHA-256 is
checked by the validation helper before use. The upstream license is preserved
in `LICENSE`. Tests do not fetch schemas or resolve network references.

Enable `MCPP_VALIDATE_PROTOCOL=ON` and select a Python environment with the
pinned dependencies in `tools/verification/requirements.txt`. Every response
from the calculator and fulfillment process suites is then validated against
its specific official response type, including negative cases and the first
inline stdio call. Generated input schemas are also checked as JSON Schema
2020-12 schemas.
