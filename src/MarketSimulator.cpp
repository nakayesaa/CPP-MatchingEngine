// MarketSimulator.cpp
// ─────────────────────────────────────────────────────────────────────
// Implementation of the synthetic order flow generator.
//
// ── RNG ─────────────────────────────────────────────────────────────
// We use xorshift32 for pseudo-random number generation. It's a
// three-shift PRNG: fast, stateless beyond a single uint32_t, and
// produces a reasonable distribution for our purposes. Not
// cryptographically secure, but that's irrelevant for market simulation.
//
// randRange() maps the raw RNG output to an integer range [lo, hi]
// via modulo. randDouble() normalizes to [0.0, 1.0] for probability
// checks (deciding whether to cancel, modify, or submit new).
//
// ── Active order tracking ───────────────────────────────────────────
// The simulator needs to know which orders are currently alive in the
// book so it can generate valid cancels and modifies. We maintain a
// flat vector of active OrderIds with an index map for O(1) lookup.
//
// removeActiveOrder() uses swap-with-last to avoid shifting elements –
// swap the target with the back of the vector, pop the back, and update
// the moved element's index. Simple and O(1).
//
// ── Order generation ────────────────────────────────────────────────
// nextOrder() rolls a random number and decides the action type:
//   - If the roll falls within cancelRate → cancel a random active order
//   - Else if within cancelRate + modifyRate → modify a random active order
//   - Otherwise → generate a fresh new order
//
// New orders get a monotonically increasing id (nextId_++), random side,
// a price centered around midPrice with some spread, and a random
// quantity. The result is a plausible-looking stream of market activity
// that stress-tests the engine across all three code paths.
// ─────────────────────────────────────────────────────────────────────

#include "MarketSimulator.h"

MarketSimulator::MarketSimulator(const SimulatorConfig& cfg)
    : config_(cfg), rngState_(cfg.seed) {}

// xorshift32 – three shifts, one uint32_t of state, period of 2^32-1
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

// ── active order tracking ───────────────────────────────────────────
// these are called by main() after processing each request's events,
// so the simulator's view stays in sync with the actual book state.

void MarketSimulator::trackActiveOrder(OrderId id) {
    size_t idx = activeOrders_.size();
    activeOrders_.push_back(id);
    activeIndex_[id] = idx;
}

// swap-with-last removal – O(1), no gaps, order doesn't matter here
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

// ── order generation ────────────────────────────────────────────────
// roll a number, pick an action, build the request. straightforward.
OrderRequest MarketSimulator::nextOrder() {
    ++sent_;

    double roll = randDouble();

    // cancel path – pick a random active order and kill it
    if (roll < config_.cancelRate && !activeOrders_.empty()) {
        int idx = randRange(0, static_cast<int>(activeOrders_.size()) - 1);
        OrderId target = activeOrders_[static_cast<size_t>(idx)];
        return {OrderAction::Cancel, Side::Buy, target, 0, 0};
    }

    // modify path – cancel-replace with new random parameters
    if (roll < config_.cancelRate + config_.modifyRate && !activeOrders_.empty()) {
        int idx = randRange(0, static_cast<int>(activeOrders_.size()) - 1);
        OrderId target = activeOrders_[static_cast<size_t>(idx)];

        Side     side  = (rng() & 1) ? Side::Buy : Side::Sell;
        Price    price = config_.midPrice + randRange(-config_.spreadTicks, config_.spreadTicks);
        Quantity qty   = static_cast<Quantity>(randRange(
            static_cast<int>(config_.minQuantity), static_cast<int>(config_.maxQuantity)));

        return {OrderAction::Modify, side, target, price, qty};
    }

    // new order path – fresh order with a new id
    OrderId  id    = nextId_++;
    Side     side  = (rng() & 1) ? Side::Buy : Side::Sell;
    Price    price = config_.midPrice + randRange(-config_.spreadTicks, config_.spreadTicks);
    Quantity qty   = static_cast<Quantity>(randRange(
        static_cast<int>(config_.minQuantity), static_cast<int>(config_.maxQuantity)));

    return {OrderAction::New, side, id, price, qty};
}
