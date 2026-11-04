# Copyright (c) 2026 BogDan Vatra
# SPDX-License-Identifier: MIT

"""Exercise already-built examples over MCP stdio, HTTP, or verified HTTPS."""
import http.client
import json
import selectors
import ssl
import subprocess
from protocol_validation import validator

VERSION = "2026-07-28"


def rpc(method, identity=1, **params):
    return {"jsonrpc": "2.0", "id": identity, "method": method, "params": {
        "_meta": {"io.modelcontextprotocol/protocolVersion": VERSION,
                  "io.modelcontextprotocol/clientCapabilities": {}}, **params}}


def headers(message):
    result = {"Content-Type": "application/json", "Accept": "application/json, text/event-stream",
              "MCP-Protocol-Version": message["params"]["_meta"]["io.modelcontextprotocol/protocolVersion"],
              "Mcp-Method": message["method"]}
    if message["method"] == "tools/call":
        result["Mcp-Name"] = message["params"]["name"]
    return result


class ExampleClient:
    def __init__(self, executable, use_http=False, tls=None, timeout_ms=1500, extra_args=()):
        self.use_http = use_http
        self.tls = tls
        command = [executable]
        if use_http:
            command += ["--port", "0", "--origin", "https://client.example", "--timeout-ms", str(timeout_ms)]
        if tls:
            command += ["--tls", *tls]
        command += list(extra_args)
        self.proc = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE, text=True)
        self.connection = None
        self.identity = 0

    def __enter__(self):
        if self.use_http:
            with selectors.DefaultSelector() as selector:
                selector.register(self.proc.stderr, selectors.EVENT_READ)
                if not selector.select(10):
                    self.close()
                    raise AssertionError("Example did not start listening")
            ready = self.proc.stderr.readline().strip()
            if not ready.startswith("Listening "):
                self.close()
                raise AssertionError(ready)
            self.port = int(ready.rsplit(":", 1)[1].split("/")[0])
            self.connection = self.connect()
        return self

    def connect(self):
        if self.tls:
            context = ssl.create_default_context(cafile=self.tls[0])
            return http.client.HTTPSConnection("127.0.0.1", self.port, timeout=5, context=context)
        return http.client.HTTPConnection("127.0.0.1", self.port, timeout=5)

    def send_http(self, body, request_headers, method="POST", path="/mcp"):
        text = body if isinstance(body, str) else json.dumps(body, ensure_ascii=False)
        self.connection.request(method, path, text.encode(), request_headers)
        response = self.connection.getresponse()
        payload = response.read()
        parsed = json.loads(payload) if payload else None
        if parsed:
            validator.validate(parsed, body if isinstance(body, dict) else None)
        return response.status, dict(response.getheaders()), parsed

    def request(self, method, **params):
        self.identity += 1
        message = rpc(method, self.identity, **params)
        if self.use_http:
            status, _, response = self.send_http(message, headers(message))
            assert status in [200, 400, 404, 500], (status, response)
        else:
            self.proc.stdin.write(json.dumps(message) + "\n")
            self.proc.stdin.flush()
            with selectors.DefaultSelector() as selector:
                selector.register(self.proc.stdout, selectors.EVENT_READ)
                assert selector.select(5), "No stdio response"
            response = json.loads(self.proc.stdout.readline())
            validator.validate(response, message)
        assert response["id"] == self.identity, response
        return response

    def call(self, name, **arguments):
        response = self.request("tools/call", name=name, arguments=arguments)
        assert "result" in response, response
        return response["result"]

    def close(self):
        if self.connection:
            self.connection.close()
        if self.proc.poll() is None:
            if self.use_http:
                self.proc.terminate()
            else:
                self.proc.stdin.close()
            try:
                self.proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()

    def __exit__(self, exc_type, exc, traceback):
        self.close()
        if exc_type is None:
            assert self.proc.returncode == 0, self.proc.stderr.read()
            assert self.proc.stdout.read() == "", "Unexpected stdout output"
