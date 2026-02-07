// PriceLevel.h
// ─────────────────────────────────────────────────────────────────────
// A price level groups all resting orders that share the same price.
//
// Think of the order book as a two-sided ladder. Each rung on that
// ladder is a PriceLevel – it sits at a specific price and holds a
// queue of orders waiting to be filled. When a match happens, we
// consume from the front of the queue (FIFO), which is the fair way
// to prioritize orders: first come, first served.
//
// aggregateQuantity tracks the total remaining quantity across all
// orders at this price. This is useful for displaying book depth
// and lets us quickly see how much liquidity sits at a given price
// without iterating through every order.
//
// orderCount is maintained separately from orders.size() mainly
// for cheap O(1) access – we use it a lot when deciding whether
// to create or clean up price levels in the book.
//
// addOrder() appends to the back of the intrusive list (FIFO).
// removeOrder() unlinks an order and adjusts the aggregate – used
// by both cancellations and fully-filled resting orders.
// reduceQuantity() is for partial fills where the order stays in
// the queue but its remaining quantity shrinks.
// ─────────────────────────────────────────────────────────────────────

#pragma once
#include "Types.h"
#include "Order.h"
#include "IntrusiveList.h"

struct PriceLevel {
    Price price = 0;
    Quantity aggregateQuantity = 0;
    uint32_t orderCount = 0;
    IntrusiveList<Order> orders;

    void addOrder(Order* order) {
        orders.push_back(order);
        aggregateQuantity += order->remainingQuantity;
        ++orderCount;
    }

    void removeOrder(Order* order) {
        aggregateQuantity -= order->remainingQuantity;
        orders.remove(order);
        --orderCount;
    }

    void reduceQuantity(Quantity qty) {
        aggregateQuantity -= qty;
    }
    bool empty() const { return orders.empty(); }
};
