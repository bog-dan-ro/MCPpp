# Copyright (c) 2026 BogDan Vatra
# SPDX-License-Identifier: MIT

"""Stdlib-only process tests; optionally validate with an official schema."""
import argparse
import copy
import json
import pathlib
import selectors
import subprocess
from protocol_validation import ProtocolValidator

VERSION = "2026-07-28"
META = {
    "io.modelcontextprotocol/protocolVersion": VERSION,
    "io.modelcontextprotocol/clientCapabilities": {},
    "io.modelcontextprotocol/clientInfo": {"name": "mcpp-test", "version": "0.1.0"},
}


def request(method, identity, **params):
    return {"jsonrpc": "2.0", "id": identity, "method": method,
            "params": {"_meta": copy.deepcopy(META), **params}}


def result_contract(response):
    assert response["jsonrpc"] == "2.0"
    assert ("result" in response) != ("error" in response)
    if "result" in response:
        result = response["result"]
        assert result["resultType"] == "complete"
        assert result["_meta"]["io.modelcontextprotocol/serverInfo"]["version"] == "0.1.0"
        if "tools" in result or "supportedVersions" in result:
            assert result["ttlMs"] == 0 and result["cacheScope"] == "private"
        if "content" in result:
            assert isinstance(result["isError"], bool)
            for block in result["content"]:
                assert block["type"] == "text" and isinstance(block["text"], str)
    else:
        assert isinstance(response["error"]["code"], int)
        assert isinstance(response["error"]["message"], str)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("server")
    parser.add_argument("--write-fixtures", type=pathlib.Path)
    parser.add_argument("--schema", type=pathlib.Path,
                        help="Official schema.json; requires optional Python jsonschema package")
    args = parser.parse_args()
    validator = ProtocolValidator(args.schema)
    # Prove the server flushes a response before EOF, with no handshake/discovery.
    first = request("tools/call", "inline", name="add", arguments={"a": 2, "b": 3})
    proc = subprocess.Popen([args.server], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        proc.stdin.write((json.dumps(first) + "\n").encode())
        proc.stdin.flush()
        with selectors.DefaultSelector() as selector:
            selector.register(proc.stdout, selectors.EVENT_READ)
            assert selector.select(5), "Server did not flush its inline response"
        inline = json.loads(proc.stdout.readline())
        validator.validate(inline, first)
        assert inline["result"]["structuredContent"] == {"result": 5}
        assert inline["id"] == "inline"
        proc.stdin.close()
        assert proc.wait(timeout=5) == 0
        assert not proc.stdout.read()
    finally:
        if proc.poll() is None:
            proc.kill()
            proc.wait()

    messages = []
    expected = []
    paired_requests = []

    def send(message, outcome):
        messages.append(message if isinstance(message, str) else json.dumps(message, ensure_ascii=False))
        expected.append(outcome)
        paired_requests.append(message if isinstance(message, dict) else None)

    discover = request("server/discover", "discover-1")
    listing = request("tools/list", "list-1")
    call = request("tools/call", 1, name="add", arguments={"a": 2, "b": 3})
    send(discover, "discover")
    send(listing, "list")
    send(call, "add")
    send(request("tools/call", 2, name="printMessage", arguments={"message": "hello\nworld ☃"}), "void")
    for tool in ["privateMemberFunction", "secret", "does_not_exist"]:
        send(request("tools/call", 3, name=tool, arguments={}), -32602)
    for invalid in [{"a": 2}, {"a": 2, "b": 3, "extra": 1}, {"a": "2", "b": 3},
                    {"a": True, "b": 3}, {"a": 2.5, "b": 3}, {"a": 2**31, "b": 3},
                    {"a": -(2**31)-1, "b": 3}, {"a": 2**64-1, "b": 3}]:
        send(request("tools/call", 4, name="add", arguments=invalid), "tool_error")
    # Integral decimal/exponent JSON values match JSON Schema's integer semantics.
    send(request("tools/call", 5, name="add", arguments={"a": 2.0, "b": 3e0}), "add")
    send(request("tools/call", 6, name="add", arguments={"a": 2**31-1, "b": 1}), "exception")
    send(call, "add")  # A throwing method must leave the process usable.
    for bad_args in [[], None, "bad"]:
        send(request("tools/call", 7, name="add", arguments=bad_args), -32602)
    send(request("tools/call", 8, name="printMessage"), "tool_error")
    send(request("tools/call", 9), -32602)
    for bad_meta in [None, {}, {"io.modelcontextprotocol/protocolVersion": VERSION},
                     {"io.modelcontextprotocol/protocolVersion": VERSION,
                      "io.modelcontextprotocol/clientCapabilities": []}]:
        bad = request("tools/list", 10)
        bad["params"]["_meta"] = bad_meta
        send(bad, -32602)
    missing = request("tools/list", 11)
    missing["params"].pop("_meta")
    send(missing, -32602)
    bad_version = request("tools/list", 12)
    bad_version["params"]["_meta"]["io.modelcontextprotocol/protocolVersion"] = "2025-11-25"
    send(bad_version, -32022)
    send(listing, "list")  # Every request checks its own metadata.
    anonymous = request("tools/list", 2**64-1)
    del anonymous["params"]["_meta"]["io.modelcontextprotocol/clientInfo"]
    send(anonymous, "list")
    for cursor in ["", "bogus", None]:
        send(request("tools/list", 13, cursor=cursor), -32602)
    send(request("unknown/rpc", 14), -32601)
    send(request("initialize", 15), -32601)
    send("{", -32700)
    send('{"x":1e9999}', -32700)
    send("[]", -32600)
    send("42", -32600)
    send("{}", -32600)
    for invalid_id in [None, True, 1.5, []]:
        bad = request("tools/list", invalid_id)
        send(bad, -32600)
    send({"jsonrpc": "1.0", "id": "bad-envelope", "method": "tools/list"}, -32600)
    notification = request("tools/call", 16, name="printMessage", arguments={"message": "must be ignored"})
    del notification["id"]
    messages.append(json.dumps(notification))  # No response and no invocation.
    messages.append(json.dumps({"jsonrpc": "2.0", "method": "notifications/initialized"}))
    send(call, "add")

    run = subprocess.run([args.server], input="\n".join(messages) + "\n", text=True,
                         capture_output=True, timeout=15)
    assert run.returncode == 0, run.stderr
    assert run.stderr == "hello\nworld ☃\n", run.stderr
    responses = [json.loads(line) for line in run.stdout.splitlines()]
    assert len(responses) == len(expected), (len(responses), len(expected))
    for response, outcome, req in zip(responses, expected, paired_requests):
        validator.validate(response, req)
        result_contract(response)
        if outcome not in [-32700, -32600]:
            assert response["id"] == req["id"]
        if isinstance(outcome, int):
            assert response["error"]["code"] == outcome, (req, outcome, response)
            if outcome == -32022:
                assert response["error"]["data"] == {"supported": [VERSION], "requested": "2025-11-25"}
            continue
        result = response["result"]
        if outcome == "discover":
            assert result["supportedVersions"] == [VERSION]
            assert result["capabilities"] == {"tools": {}}
        elif outcome == "list":
            assert [tool["name"] for tool in result["tools"]] == ["add", "printMessage"]
            assert "nextCursor" not in result
            a, p = [tool["inputSchema"] for tool in result["tools"]]
            assert a == {"type": "object", "properties": {
                k: {"type": "integer", "minimum": -(2**31), "maximum": 2**31-1} for k in ["a", "b"]},
                "required": ["a", "b"], "additionalProperties": False}
            assert p == {"type": "object", "properties": {"message": {"type": "string"}},
                         "required": ["message"], "additionalProperties": False}
        elif outcome == "add":
            assert not result["isError"] and result["structuredContent"] == {"result": 5}
            assert json.loads(result["content"][0]["text"]) == {"result": 5}
        elif outcome == "void":
            assert not result["isError"] and "structuredContent" not in result
        else:
            assert result["isError"]
            if outcome == "exception":
                assert result["content"][0]["text"] == "Tool execution failed"

    if args.write_fixtures:
        args.write_fixtures.write_text(json.dumps([
            {"request": req, "response": response}
            for req, response in zip([discover, listing, call], responses[:3])
        ], indent=2, ensure_ascii=False) + "\n")
    print(f"stdio: {len(responses)} responses checked; inline call and notifications checked")


if __name__ == "__main__":
    main()
