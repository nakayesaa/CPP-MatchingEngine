

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
