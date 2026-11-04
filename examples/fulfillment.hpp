// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#pragma once
#include <mcp++/annotations.hpp>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>

// Reservation IDs connect calls; the demo keeps all state in memory.
class Stockroom {
    struct Item {
        int available;
        double price;
    };
    enum class State { held, confirmed, shipped, cancelled };
    struct Reservation {
        std::string sku;
        int quantity;
        State state = State::held;
    };
    std::map<std::string, Item, std::less<>> items_{{"widget", {10, 12.5}}, {"gadget", {4, 30.0}}};
    std::map<std::string, Reservation, std::less<>> reservations_;
    std::size_t sequence_ = 0;

    Item& item(std::string_view sku) {
        auto it = items_.find(sku);
        if (it == items_.end())
            throw std::invalid_argument("Unknown SKU");
        return it->second;
    }
    Reservation& reservation(std::string_view id) {
        auto it = reservations_.find(id);
        if (it == reservations_.end())
            throw std::invalid_argument("Unknown reservation");
        return it->second;
    }
public:
    [[=mcp::tool_t{"Return the available quantity for a SKU"}]]
    int available(std::string_view sku) { return item(sku).available; }

    [[=mcp::tool_t{"Quote a quantity with a discount between zero and one half"}]]
    double quote(std::string_view sku, int quantity, double discount) {
        if (quantity <= 0 || discount < 0 || discount > 0.5)
            throw std::invalid_argument("Invalid quote");
        return item(sku).price * quantity * (1 - discount);
    }

    [[=mcp::tool_t{"Hold stock and return a reservation identifier"}]]
    std::string reserve(std::string sku, int quantity) {
        auto& product = item(sku);
        if (quantity <= 0 || quantity > product.available)
            throw std::invalid_argument("Insufficient stock");
        auto id = "reservation-" + std::to_string(++sequence_);
        reservations_.emplace(id, Reservation{std::move(sku), quantity});
        product.available -= quantity;
        return id;
    }

    [[=mcp::tool_t{"Confirm a held reservation; return false if already confirmed"}]]
    bool confirm(std::string_view reservationId) {
        auto& booking = reservation(reservationId);
        if (booking.state == State::confirmed) return false;
        if (booking.state != State::held)
            throw std::invalid_argument("Reservation cannot be confirmed");
        booking.state = State::confirmed;
        return true;
    }

    [[=mcp::tool_t{"Cancel an unshipped reservation and restore its stock"}]]
    void cancel(std::string_view reservationId) {
        auto& booking = reservation(reservationId);
        if (booking.state == State::cancelled) return;
        if (booking.state == State::shipped) throw std::invalid_argument("Already shipped");
        item(booking.sku).available += booking.quantity;
        booking.state = State::cancelled;
    }

    // Used by Fulfillment, but not exposed as a tool.
    bool claimForShipping(std::string_view reservationId) {
        auto& booking = reservation(reservationId);
        if (booking.state != State::confirmed) return false;
        booking.state = State::shipped;
        return true;
    }
};

class Fulfillment {
    Stockroom& stockroom_;
public:
    explicit Fulfillment(Stockroom& stockroom) : stockroom_(stockroom) {}

    [[=mcp::tool_t{"Describe available shipping services"}]]
    std::string serviceInfo() const { return "Demo shipping: standard or express"; }

    [[=mcp::tool_t{"Ship a confirmed reservation and return its label"}]]
    std::string shippingLabel(std::string_view reservationId, std::string recipient, bool express) {
        if (recipient.empty() || recipient.size() > 200 || recipient.find_first_of("\r\n") != std::string::npos)
            throw std::invalid_argument("Invalid recipient");
        auto label = std::string(express ? "EXPRESS " : "STANDARD ") +
                     std::string(reservationId) + " -> " + recipient;
        if (!stockroom_.claimForShipping(reservationId))
            throw std::invalid_argument("Reservation is not ready to ship");
        return label;
    }
};
