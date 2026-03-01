
//just a simple struct to represent an order in the order book.
// we make sure we have prev and next pointer for intrusive list usage.
// also reset() function to clear the data when we release back to the pool.

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
