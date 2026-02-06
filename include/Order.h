#pragma once
#include "Types.h"

// Represents an order in the order book with all the necessary fields
// and pointers for intrusive list implementation
struct Order {
    OrderId id = 0;
    Side side = Side::Buy;
    Price price = 0;
    Quantity quantity = 0;
    Quantity remainingQuantity = 0;
    uint64_t sequence = 0;

    // intrusive list
    Order* prev = nullptr;
    Order* next = nullptr;

    void reset() {
        id = 0;
        side = Side::Buy;
        price = 0;
        quantity = 0;
        remainingQuantity = 0;
        sequence = 0;
        prev = nullptr;
        next = nullptr;
    }
};
