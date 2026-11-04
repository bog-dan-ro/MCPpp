# Copyright (c) 2026 BogDan Vatra
# SPDX-License-Identifier: MIT

import argparse
import os
from example_client import ExampleClient, headers, rpc


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("server")
    parser.add_argument("--tls", nargs=2)
    args = parser.parse_args()
    # Public test-only credential, supplied through the environment, never argv.
    os.environ["MCPP_TEST_TOKEN"] = "public-integration-test-token"
    with ExampleClient(args.server, True, args.tls, extra_args=["--token-env", "MCPP_TEST_TOKEN"]) as client:
        message = rpc("tools/call", name="add", arguments={"a": 2, "b": 3})
        for credential in [None, "Bearer wrong-token", "Basic public-integration-test-token"]:
            request_headers = headers(message)
            if credential:
                request_headers["Authorization"] = credential
            status, response_headers, body = client.send_http(message, request_headers)
            assert status == 401 and body is None
            assert response_headers["WWW-Authenticate"] == 'Bearer realm="mcp"'
        status, _, body = client.send_http(message, {**headers(message), "Authorization": "Bearer public-integration-test-token"})
        assert status == 200 and body["result"]["structuredContent"] == {"result": 5}
    print("authentication: bearer challenge, invalid credentials and authenticated call checked")


if __name__ == "__main__":
    main()
