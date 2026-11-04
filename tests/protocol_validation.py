# Copyright (c) 2026 BogDan Vatra
# SPDX-License-Identifier: MIT

"""Official schema validation, opt-in through CTest's environment."""
import hashlib
import json
import os
import pathlib


class ProtocolValidator:
    def __init__(self, schema_path=None):
        self.validators = {}
        self.schema = None
        path = schema_path or os.environ.get("MCPP_PROTOCOL_SCHEMA")
        if path:
            from jsonschema import Draft202012Validator
            self.validator_type = Draft202012Validator
            path = pathlib.Path(path)
            data = path.read_bytes()
            provenance = path.with_name("schema-source.json")
            if provenance.exists():
                expected = json.loads(provenance.read_text())["sha256"]
                assert hashlib.sha256(data).hexdigest() == expected, "Pinned MCP schema hash mismatch"
            self.schema = json.loads(data)
            Draft202012Validator.check_schema(self.schema)

    def validate(self, response, request=None):
        if self.schema is None:
            return
        definition = "JSONRPCErrorResponse" if "error" in response else {
            "server/discover": "DiscoverResultResponse",
            "tools/list": "ListToolsResultResponse",
            "tools/call": "CallToolResultResponse",
        }[request["method"]]
        if definition not in self.validators:
            self.validators[definition] = self.validator_type({
                "$schema": self.schema["$schema"], "$ref": "#/$defs/" + definition,
                "$defs": self.schema["$defs"],
            })
        self.validators[definition].validate(response)
        if request and request.get("method") == "tools/list" and "result" in response:
            for tool in response["result"]["tools"]:
                self.validator_type.check_schema(tool["inputSchema"])


validator = ProtocolValidator()
