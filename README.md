# matching-engine

> A single-threaded, low-latency limit order book matching engine in C++17. Focused on correctness, deterministic behavior, and realistic exchange semantics.

![C++17](https://img.shields.io/badge/language-C%2B%2B17-blue?logo=cplusplus)
![License](https://img.shields.io/badge/license-MIT-green)

---

## Overview

The core component of any exchange, receives buy and sell orders, matches them by price-time priority, and reports trade executions. Also handles cancel and modify operations.

Intentionally single-threaded. Every operation runs in a deterministic sequential loop, eliminating concurrency bugs and keeping latency predictable. A built-in market simulator generates realistic randomized order flow to exercise the engine under load.

---

## Performance

Benchmarked on 1M messages, single-threaded, consumer hardware:

```
Messages processed : 1,000,000
Elapsed time       : 0.19 s
Throughput         : 5.31 M msgs/s (~188 ns/msg)
Total fills        : 642,913
Total rejects      : 0
```

---

## Architecture

```
OrderRequest (New / Cancel / Modify)
          │
          ▼
+-------------------+
│     OrderBook     │  ◄── single entry point
│  processOrder()   │
+-------------------+
   │        │        │
   ▼        ▼        ▼
handleNew  handleCancel  handleModify
   │                         │
   ▼                     cancel + handleNew
matchOrder()
   │
fills ──► restOrder() ──► pool_.acquire()
                  │
          ExecutionReport events
```

---

## Design Decisions

| Component | Implementation | Why |
|-----------|---------------|-----|
| Order lookup | `unordered_map<OrderId, Order*>` | O(1) find for cancel/modify |
| Price levels | `std::map` with custom comparators | O(log n) insert, O(1) best price via `begin()` |
| FIFO queue | `IntrusiveList<Order>` (custom) | O(1) push/remove, zero allocations, strict time priority |
| Memory | `ObjectPool<Order>` (custom) | Pre-allocated array + free stack — zero heap allocs at runtime |
| Pricing | `int32_t` ticks | No floating-point rounding errors, fast integer comparisons |
| RNG | xorshift32 | Fast, deterministic, reproducible with seed |

---

## How It Works

### Matching — Visual Example

Starting book:
```
ASK  102  [Order5: 5]
ASK  101  [Order3: 10] -> [Order4: 5]
ASK  100  [Order1: 8]  -> [Order2: 12]
──────────── spread ────────────
BID   99  [Order6: 20]
```

**Incoming: Buy 25 @ 101**

- Level 100 — crosses. Fill Order1 (8), fill Order2 (12). Level empty, removed.
- Level 101 — crosses. Fill Order3 partially (5). Incoming fully filled. Stop.

**Result:**
```
ASK  102  [Order5: 5]
ASK  101  [Order3: 5] -> [Order4: 5]
──────────── spread ────────────
BID   99  [Order6: 20]
```

Three fills emitted: `8 @ 100`, `12 @ 100`, `5 @ 101` — all at resting prices.

### Order Lifecycle

**New** — validates → acquires from pool → matches against opposite side → rests remainder → emits `Acknowledged` / `Filled`.

**Cancel** — O(1) lookup → removes from price level + index → returns to pool → emits `Canceled`.

**Modify** — cancel-and-replace: removes old order, emits `Canceled`, submits new order with new sequence number (loses time priority). Correct exchange behavior.

---

## Data Structures

```
OrderBook
├── bids_: map<Price, PriceLevel, greater<Price>>   (begin() = best bid)
│     └── PriceLevel
│           └── IntrusiveList<Order>: [A (oldest)] <-> [B] <-> [C (newest)]
│
├── asks_: map<Price, PriceLevel>                   (begin() = best ask)
│     └── PriceLevel
│           └── IntrusiveList<Order>: [D] <-> [E]
│
├── orderIndex_: unordered_map<OrderId, Order*>     O(1) lookup by ID
│
└── pool_: ObjectPool<Order>                        contiguous array, no heap
```

Every `Order*` in `orderIndex_` points to a node inside exactly one `IntrusiveList` inside exactly one `PriceLevel`. No order ever lives on the heap.

---

## Project Structure

```
MatchingEngine/
├── CMakeLists.txt
├── include/
│   ├── Types.h               Core type aliases and structs
│   ├── IntrusiveList.h       Doubly-linked list (zero allocations)
│   ├── ObjectPool.h          Pre-allocated object pool
│   ├── Order.h               Order node with intrusive hooks
│   ├── PriceLevel.h          FIFO queue + aggregate quantity cache
│   ├── OrderBook.h           Book interface and matching declarations
│   └── MarketSimulator.h     Random order flow generator
└── src/
    ├── OrderBook.cpp         Core matching loop and order handling
    ├── MarketSimulator.cpp   xorshift32 RNG, O(1) active-order tracking
    ├── main.cpp              Wiring, simulation loop, timing, statistics
    └── tests.cpp             32 correctness tests across 12 invariants
```

---
## Build & Run

**Engine:**
```bash
g++ -std=c++17 -O2 -I include src/OrderBook.cpp src/MarketSimulator.cpp src/main.cpp -o matching_engine
./matching_engine
```

**Tests:**
```bash
g++ -std=c++17 -O2 -I include src/OrderBook.cpp src/MarketSimulator.cpp src/tests.cpp -o tests
./tests
```

**Configure** in `src/main.cpp`:
```cpp
cfg.totalMessages = 1000000;  // messages to process
cfg.midPrice      = 10000;    // center price in ticks
cfg.spreadTicks   = 20;       // price range: mid ± this
cfg.cancelRate    = 0.15;     // 15% cancel messages
cfg.modifyRate    = 0.05;     // 5% modify messages
cfg.seed          = 42;       // deterministic RNG seed
```

---

## License

MIT