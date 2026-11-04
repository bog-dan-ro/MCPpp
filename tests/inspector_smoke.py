# Copyright (c) 2026 BogDan Vatra
# SPDX-License-Identifier: MIT

"""Exercise real official Inspector clients in modern mode, without user state."""
import argparse
import json
import os
import pathlib
import subprocess
from example_client import ExampleClient


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("stdio_server")
    parser.add_argument("http_server")
    parser.add_argument("inspector")
    parser.add_argument("reports", type=pathlib.Path)
    parser.add_argument("--tls", nargs=2)
    args = parser.parse_args()
    args.reports.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    environment["MCP_STORAGE_DIR"] = str(args.reports / "inspector-state")
    environment["MCP_INSPECTOR_SECRET_STORE"] = "memory"
    if args.tls:
        environment["NODE_EXTRA_CA_CERTS"] = str(pathlib.Path(args.tls[0]).resolve())
    outcomes = []

    def invoke(target, transport, method, extra=()):
        command = [args.inspector, "--cli", target, "--protocol-era", "modern", "--format", "json",
                   "--stored-auth-only", "--method", method, *extra]
        run = subprocess.run(command, capture_output=True, text=True, env=environment, timeout=25)
        assert run.returncode == 0, run.stderr
        parsed = json.loads(run.stdout)
        result = parsed["result"]
        if method == "tools/list":
            assert [tool["name"] for tool in result["tools"]] == ["add", "printMessage"]
        else:
            assert result["structuredContent"] == {"result": 5}
        outcomes.append({"transport": transport, "method": method, "result": result})

    invoke(str(pathlib.Path(args.stdio_server).resolve()), "stdio", "tools/list")
    invoke(str(pathlib.Path(args.stdio_server).resolve()), "stdio", "tools/call", ["--tool-name", "add", "--tool-arg", "a=2", "--tool-arg", "b=3"])
    # A public integration-test token, not a user credential.
    environment["MCPP_INSPECTOR_TEST_TOKEN"] = "public-inspector-test-token"
    os.environ["MCPP_INSPECTOR_TEST_TOKEN"] = environment["MCPP_INSPECTOR_TEST_TOKEN"]
    with ExampleClient(args.http_server, True, args.tls,
                       extra_args=["--token-env", "MCPP_INSPECTOR_TEST_TOKEN"]) as client:
        target = f"{'https' if args.tls else 'http'}://127.0.0.1:{client.port}/mcp"
        authorization = ["--header", "Authorization: Bearer public-inspector-test-token"]
        invoke(target, "https" if args.tls else "http", "tools/list", authorization)
        invoke(target, "https" if args.tls else "http", "tools/call", [*authorization, "--tool-name", "add", "--tool-arg", "a=2", "--tool-arg", "b=3"])
    (args.reports / "inspector.json").write_text(json.dumps({"version": "2.13.0", "exchanges": outcomes}, indent=2) + "\n")
    print("Inspector: real modern stdio and authenticated HTTP(S) list/call passed")


if __name__ == "__main__":
    main()
