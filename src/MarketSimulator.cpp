#include "MarketSimulator.h"

MarketSimulator::MarketSimulator(const SimulatorConfig& cfg)
    : config_(cfg), rngState_(cfg.seed) {}

// xorshift32 — fast, deterministic, reproducible
uint32_t MarketSimulator::rng() {
    rngState_ ^= rngState_ << 13;
    rngState_ ^= rngState_ >> 17;
    rngState_ ^= rngState_ << 5;
    return rngState_;
}

int MarketSimulator::randRange(int lo, int hi) {
    return lo + static_cast<int>(rng() % static_cast<uint32_t>(hi - lo + 1));
}

double MarketSimulator::randDouble() {
    return static_cast<double>(rng()) / static_cast<double>(UINT32_MAX);
}

void MarketSimulator::trackActiveOrder(OrderId id) {
    size_t idx = activeOrders_.size();
    activeOrders_.push_back(id);
    activeIndex_[id] = idx;
}

void MarketSimulator::removeActiveOrder(OrderId id) {
    auto it = activeIndex_.find(id);
    if (it == activeIndex_.end()) return;

    size_t idx  = it->second;
    size_t last = activeOrders_.size() - 1;

    if (idx != last) {
        OrderId movedId       = activeOrders_[last];
        activeOrders_[idx]    = movedId;
        activeIndex_[movedId] = idx;
    }

    activeOrders_.pop_back();
    activeIndex_.erase(it);
}

OrderRequest MarketSimulator::nextOrder() {
    ++sent_;

    double roll = randDouble();

    // attempt cancel
    if (roll < config_.cancelRate && !activeOrders_.empty()) {
        int idx = randRange(0, static_cast<int>(activeOrders_.size()) - 1);
        OrderId target = activeOrders_[static_cast<size_t>(idx)];
        return {OrderAction::Cancel, Side::Buy, target, 0, 0};
    }

    // attempt modify
    if (roll < config_.cancelRate + config_.modifyRate && !activeOrders_.empty()) {
        int idx = randRange(0, static_cast<int>(activeOrders_.size()) - 1);
        OrderId target = activeOrders_[static_cast<size_t>(idx)];

        Side     side  = (rng() & 1) ? Side::Buy : Side::Sell;
        Price    price = config_.midPrice + randRange(-config_.spreadTicks, config_.spreadTicks);
        Quantity qty   = static_cast<Quantity>(randRange(
            static_cast<int>(config_.minQuantity), static_cast<int>(config_.maxQuantity)));

        return {OrderAction::Modify, side, target, price, qty};
    }

    // new order
    OrderId  id    = nextId_++;
    Side     side  = (rng() & 1) ? Side::Buy : Side::Sell;
    Price    price = config_.midPrice + randRange(-config_.spreadTicks, config_.spreadTicks);
    Quantity qty   = static_cast<Quantity>(randRange(
        static_cast<int>(config_.minQuantity), static_cast<int>(config_.maxQuantity)));

    return {OrderAction::New, side, id, price, qty};
}
