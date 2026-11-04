# Copyright (c) 2026 BogDan Vatra
# SPDX-License-Identifier: MIT

"""Keep the network loop responsive while a serialized C++ tool is running."""
import argparse
import concurrent.futures
import http.client
import json
import selectors
import socket
import time
from example_client import ExampleClient, headers, rpc


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("server")
    parser.add_argument("--tls", nargs=2)
    args = parser.parse_args()
    with ExampleClient(args.server, True, args.tls, timeout_ms=200) as client:
        first = rpc("tools/call", name="wait", arguments={"milliseconds": 1200})
        second = rpc("tools/call", identity=2, name="wait", arguments={"milliseconds": 1200})
        queued_connection = client.connect()

        def active_call():
            return client.send_http(first, headers(first))

        with concurrent.futures.ThreadPoolExecutor(1) as pool:
            active = pool.submit(active_call)
            with selectors.DefaultSelector() as selector:
                selector.register(client.proc.stderr, selectors.EVENT_READ)
                assert selector.select(5), "Tool did not start"
            assert client.proc.stderr.readline().strip() == "Tool started"
            # The network timer must fire while the first tool is still running.
            with socket.create_connection(("127.0.0.1", client.port), timeout=2) as stalled:
                before = time.monotonic()
                assert stalled.recv(1) == b""
                assert time.monotonic() - before < 0.9
            queued_connection.request("POST", "/mcp", json.dumps(second), headers(second))
            # Give the network loop time to queue the fully received request.
            time.sleep(0.05)
            client.proc.terminate()
            result = active.result(timeout=5)
            assert result[0] == 200 and result[2]["result"]["structuredContent"] == {"result": 1}
            queued = queued_connection.getresponse()
            assert queued.status == 503, queued.status
            assert not queued.read()
            queued_connection.close()
            assert client.proc.wait(timeout=5) == 0
            assert "Tool started" not in client.proc.stderr.read(), "Queued tool executed after shutdown"
    print("shutdown: timers stayed responsive; active tool drained; queued tool rejected")


if __name__ == "__main__":
    main()
