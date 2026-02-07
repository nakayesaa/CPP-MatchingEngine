// Types.h
// ─────────────────────────────────────────────────────────────────────
// Core type definitions for the matching engine.
//
// This is the foundation that everything else sits on top of. We define
// the type aliases (OrderId, Price, Quantity) so the rest of the codebase
// speaks the same language, and we define the two main message structures
// that form the engine's public contract:
//
//   OrderRequest     – what goes INTO the engine (new order, cancel, modify)
//   ExecutionReport  – what comes OUT of the engine (ack, fill, cancel, reject)
//
// The idea is simple: the outside world talks to the engine exclusively
// through OrderRequests, and the engine responds exclusively through
// ExecutionReports. Everything in between is internal.
//
// Price is signed (int32_t) so we can represent offsets and deltas without
// casting headaches. Quantity is unsigned because negative quantities
// don't make sense. OrderId is uint64_t because we want to support
// long-running simulations without worrying about wrapping.
// ─────────────────────────────────────────────────────────────────────

#pragma once
#include <cstdint>

using OrderId  = uint64_t;
using Price    = int32_t;
using Quantity = uint32_t;

enum class Side : uint8_t { Buy, Sell };

enum class OrderAction : uint8_t { New, Cancel, Modify };

// the input – describes what the caller wants the engine to do
struct OrderRequest {
    OrderAction action;
    Side        side;
    OrderId     id;
    Price       price;
    Quantity    quantity;
};

enum class EventType : uint8_t {
    Acknowledged,
    Filled,
    Canceled,
    Rejected
};

// the output – describes what actually happened inside the engine
// every processed request produces one or more of these, so the caller
// can keep its own state in sync (track active orders, record fills, etc.)
struct ExecutionReport {
    EventType type;
    OrderId   orderId;
    OrderId   matchedOrderId;
    Price     price;
    Quantity  fillQuantity;
    Quantity  remainingQuantity;
    Quantity  matchedRemainingQuantity;
};
