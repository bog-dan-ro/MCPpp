# Copyright (c) 2026 BogDan Vatra
# SPDX-License-Identifier: MIT

import argparse
from example_client import ExampleClient


def value(result):
    assert not result["isError"], result
    return result["structuredContent"]["result"]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("server")
    parser.add_argument("--http", action="store_true")
    parser.add_argument("--tls", nargs=2)
    args = parser.parse_args()
    with ExampleClient(args.server, args.http, args.tls) as client:
        listed = client.request("tools/list")["result"]["tools"]
        assert [tool["name"] for tool in listed] == [
            "available", "cancel", "confirm", "quote", "reserve", "serviceInfo", "shippingLabel"]
        schema = next(tool["inputSchema"] for tool in listed if tool["name"] == "shippingLabel")
        assert schema["required"] == ["reservationId", "recipient", "express"]
        assert "Demo shipping" in value(client.call("serviceInfo"))
        assert value(client.call("available", sku="widget")) == 10
        assert value(client.call("quote", sku="widget", quantity=2, discount=0.1)) == 22.5
        held = value(client.call("reserve", sku="widget", quantity=2))
        assert value(client.call("available", sku="widget")) == 8
        assert client.call("shippingLabel", reservationId=held, recipient="Ada", express=True)["isError"]
        assert value(client.call("confirm", reservationId=held)) is True
        assert value(client.call("confirm", reservationId=held)) is False
        label = value(client.call("shippingLabel", reservationId=held, recipient="Ada", express=True))
        assert label == f"EXPRESS {held} -> Ada"
        assert client.call("cancel", reservationId=held)["isError"]
        assert client.call("shippingLabel", reservationId=held, recipient="Ada", express=True)["isError"]
        cancellable = value(client.call("reserve", sku="widget", quantity=3))
        assert value(client.call("available", sku="widget")) == 5
        assert not client.call("cancel", reservationId=cancellable)["isError"]
        assert not client.call("cancel", reservationId=cancellable)["isError"]
        assert value(client.call("available", sku="widget")) == 8
        for arguments in [dict(sku="missing", quantity=1), dict(sku="widget", quantity=99), dict(sku="widget", quantity=0)]:
            assert client.call("reserve", **arguments)["isError"]
        assert client.call("quote", sku="widget", quantity=1, discount=0.8)["isError"]
        hidden = client.request("tools/call", name="claimForShipping", arguments={"reservationId": held})
        assert hidden["error"]["code"] == -32602
        assert value(client.call("available", sku="widget")) == 8
    print(f"fulfillment {'HTTPS' if args.tls else 'HTTP' if args.http else 'stdio'}: reserve -> confirm -> ship; rollback and hidden helpers checked")


if __name__ == "__main__":
    main()
