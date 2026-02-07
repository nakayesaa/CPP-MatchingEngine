// MarketSimulator.h
// ─────────────────────────────────────────────────────────────────────
// A synthetic order flow generator for benchmarking the matching engine.
//
// This isn't part of the engine itself – it's the test harness. The
// simulator produces a stream of OrderRequests that mimic realistic
// market activity: mostly new orders, with some cancels and modifies
// mixed in according to configurable rates.
//
// SimulatorConfig controls the shape of the generated traffic:
//   - midPrice / spreadTicks define the price range. Orders land
//     randomly within [midPrice - spreadTicks, midPrice + spreadTicks],
//     so the book builds up around a central price.
//   - cancelRate / modifyRate control the mix of actions. With the
//     defaults (15% cancel, 5% modify), roughly 80% of messages are
//     new orders – which is fairly close to real market ratios.
//   - totalMessages is how many requests to generate before stopping.
//   - seed makes everything deterministic. Same seed, same sequence,
//     same results – useful for reproducible benchmarks.
//
// The simulator needs to track which orders are currently alive in the
// book so it can generate valid cancel/modify requests (you can't cancel
// an order that doesn't exist). activeOrders_ is a vector of live
// OrderIds, and activeIndex_ maps each id to its position in that vector.
// Removal uses the swap-with-last trick for O(1) deletion without
// leaving gaps.
//
// The RNG is xorshift32 – dead simple, fast, and good enough for
// generating order flow. We're not doing crypto here, we just need
// a decent spread of prices and quantities.
// ─────────────────────────────────────────────────────────────────────

#pragma once
#include "Types.h"
#include <vector>
#include <unordered_map>
#include <cstdint>

struct SimulatorConfig {
    Price    midPrice       = 10000;
    Price    spreadTicks    = 10;
    Quantity minQuantity    = 1;
    Quantity maxQuantity    = 100;
    double   cancelRate     = 0.15;
    double   modifyRate     = 0.05;
    uint64_t totalMessages  = 100000;
    uint32_t seed           = 42;
};

class MarketSimulator {
public:
    explicit MarketSimulator(const SimulatorConfig& config = {});

    OrderRequest nextOrder();
    bool         done() const { return sent_ >= config_.totalMessages;}
    uint64_t     sent() const { return sent_; }

    void trackActiveOrder(OrderId id);
    void removeActiveOrder(OrderId id);

private:
    SimulatorConfig          config_;
    uint64_t                 sent_     = 0;
    OrderId                  nextId_   = 1;
    uint32_t                 rngState_;
    
    std::vector<OrderId>                  activeOrders_;
    std::unordered_map<OrderId, size_t>   activeIndex_;

    uint32_t rng();
    int      randRange(int lo, int hi);
    double   randDouble();
};
