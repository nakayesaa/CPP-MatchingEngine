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
