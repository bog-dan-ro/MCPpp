# Copyright (c) 2026 BogDan Vatra
# SPDX-License-Identifier: MIT

"""Socket-level tests of CMake-built HTTP and HTTPS examples; no compilation."""
import argparse
import http.client
import json
import socket
import ssl
from example_client import ExampleClient, headers, rpc


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("server")
    parser.add_argument("--tls", nargs=2)
    args = parser.parse_args()
    with ExampleClient(args.server, True, args.tls, timeout_ms=800) as client:
        def send(message, custom_headers=None, **kwargs):
            return client.send_http(message, headers(message) if custom_headers is None else custom_headers, **kwargs)

        call = rpc("tools/call", name="add", arguments={"a": 2, "b": 3})
        status, response_headers, response = send(call)
        assert status == 200 and response["result"]["structuredContent"] == {"result": 5}
        assert response_headers["Content-Type"] == "application/json"
        assert response_headers["Cache-Control"] == "no-store"
        sock = client.connection.sock
        discover = rpc("server/discover")
        assert send(discover)[2]["result"]["supportedVersions"] == ["2026-07-28"]
        assert client.connection.sock is sock, "HTTP keep-alive was not reused"
        listed = send(rpc("tools/list"))[2]["result"]["tools"]
        assert [tool["name"] for tool in listed] == ["add", "printMessage"]
        for name in ["privateMemberFunction", "secret"]:
            failed = send(rpc("tools/call", name=name, arguments={}))
            assert failed[0] == 400 and failed[2]["error"]["code"] == -32602
        failure = send(rpc("tools/call", name="add", arguments={"a": 2**31-1, "b": 1}))
        assert failure[0] == 200 and failure[2]["result"]["isError"]
        invalid_args = send(rpc("tools/call", name="add", arguments={"a": "2", "b": 3}))
        assert invalid_args[0] == 200 and invalid_args[2]["result"]["isError"]
        for header in ["MCP-Protocol-Version", "Mcp-Method", "Mcp-Name"]:
            for mismatch in [None, "wrong"]:
                changed = headers(call)
                if mismatch is None:
                    changed.pop(header)
                else:
                    changed[header] = mismatch
                status, _, failed = send(call, changed)
                assert status == 400 and failed["error"]["code"] == -32020
        encoded = headers(call)
        encoded["Mcp-Name"] = "=?base64?YWRk?="
        assert send(call, encoded)[0] == 200
        for origin in ["https://evil.example", "null", "https://client.example.evil"]:
            changed = {**headers(call), "Origin": origin}
            assert send(call, changed)[0] == 403
        assert send(call, {**headers(call), "Origin": "https://client.example"})[0] == 200
        assert send(call, {**headers(call), "Accept": "text/event-stream"})[0] == 406
        assert send(call, {**headers(call), "Content-Type": "text/plain"})[0] == 415
        assert send(call, method="GET")[0] == 405
        assert send(call, method="DELETE")[0] == 405
        assert send(call, path="/elsewhere")[0] == 404
        unknown = send(rpc("unknown/rpc"))
        assert unknown[0] == 404 and unknown[2]["error"]["code"] == -32601
        old = rpc("server/discover")
        old["params"]["_meta"]["io.modelcontextprotocol/protocolVersion"] = "2025-11-25"
        old_response = send(old)
        assert old_response[0] == 400 and old_response[2]["error"]["code"] == -32022
        assert send(call)[0] == 200
        status, _, malformed = client.send_http("{", headers(call))
        assert status == 400 and malformed["error"]["code"] == -32700
        notification = {"jsonrpc": "2.0", "method": "notifications/cancelled", "params": {"requestId": 1}}
        status, _, result = client.send_http(notification, {"Content-Type": "application/json", "Accept": "application/json, text/event-stream"})
        assert status == 202 and result is None
        assert send(call)[0] == 200

        def raw_socket():
            stream = socket.create_connection(("127.0.0.1", client.port), timeout=5)
            if args.tls:
                context = ssl.create_default_context(cafile=args.tls[0])
                stream = context.wrap_socket(stream, server_hostname="localhost")
            return stream

        def raw_status(raw):
            with raw_socket() as stream:
                stream.sendall(raw)
                response = http.client.HTTPResponse(stream)
                response.begin()
                response.read()
                return response.status

        assert raw_status(b"POST /mcp HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1048577\r\n\r\n") == 413
        assert raw_status(b"POST /mcp HTTP/1.1\r\nHost: localhost\r\nX-Large: " + b"x" * 17000 + b"\r\n\r\n") == 431
        # A slow peer cannot hold the event loop or the accepted socket forever.
        with raw_socket() as slow:
            slow.sendall(b"POST /mcp HTTP/1.1\r\nHost: localhost\r\n")
            try:
                assert slow.recv(1) == b"", "Incomplete request did not time out"
            except (ConnectionResetError, ssl.SSLEOFError):
                pass
        if args.tls:
            with socket.create_connection(("127.0.0.1", client.port), timeout=5) as slow_tls:
                assert slow_tls.recv(1) == b"", "Incomplete TLS handshake did not time out"
        client.connection.close()
        client.connection = client.connect()
        assert send(call)[0] == 200, "Server did not survive bad connections"
        assert send(call, {**headers(call), "Connection": "close"})[0] == 200
        idle = raw_socket()  # stop() must cancel active reads as well as accept().
    with idle:
        try:
            assert idle.recv(1) == b"", "Shutdown left an accepted socket open"
        except (ConnectionResetError, ssl.SSLEOFError):
            pass
    print(f"{'HTTPS (certificate verified)' if args.tls else 'HTTP'}: MCP calls, mirrors, origins, errors, keep-alive, limits and deadlines passed")


if __name__ == "__main__":
    main()
