// Order.h
// ─────────────────────────────────────────────────────────────────────
// The Order struct – a single resting order inside the book.
//
// Each order lives in two places at once:
//   1. The ObjectPool (which owns the actual memory)
//   2. An IntrusiveList inside a PriceLevel (which chains orders at the same price)
//
// That's why it carries prev/next pointers directly on the struct itself.
// Instead of wrapping orders in some external linked-list node, the order
// IS the node. This keeps things cache-friendly and avoids extra allocations.
//
// `quantity` is the original size the order came in with.
// `remainingQuantity` is what's left after partial fills.
// reset() zeroes everything out so the ObjectPool can hand this object
// to someone else with a clean slate.
// ─────────────────────────────────────────────────────────────────────

#pragma once
#include "Types.h"

struct Order {
    OrderId id = 0;
    Side side = Side::Buy;
    Price price = 0;
    Quantity quantity = 0;
    Quantity remainingQuantity = 0;

    Order* prev = nullptr;
    Order* next = nullptr;

    void reset() {
        id = 0;
        side = Side::Buy;
        price = 0;
        quantity = 0;
        remainingQuantity = 0;
        prev = nullptr;
        next = nullptr;
    }
};
