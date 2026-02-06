#pragma once
#include <cstdint>

using OrderId  = uint64_t;
using Price    = int32_t;//make sure there are no floating-point
using Quantity = uint32_t;

enum class Side : uint8_t { Buy, Sell };

enum class OrderAction : uint8_t { New, Cancel, Modify };

struct OrderRequest {
    OrderAction action;
    Side        side;
    OrderId     id;
    Price       price;
    Quantity    quantity;
}; //what comes into the engine, basically the input 

enum class EventType : uint8_t {
    Acknowledged,
    Filled,
    Canceled,
    Rejected
};

struct ExecutionReport {
    EventType type;
    OrderId   orderId;
    OrderId   matchedOrderId;
    Price     price;
    Quantity  fillQuantity;
    Quantity  remainingQuantity;       
    Quantity  matchedRemainingQuantity;
}; //what comes out of the engine, basically the output
