#include "OrderBook.h"
#include "MarketSimulator.h"
#include <iostream>
#include <string>
#include <cassert>
#include <cmath>
#include <sstream>

// ── test harness ────────────────────────────────────────────────────

static int testsPassed = 0;
static int testsFailed = 0;

#define ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "  FAIL: " << #expr \
                      << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            ++testsFailed; \
            return; \
        } \
    } while (0)

#define ASSERT_EQ(a, b) \
    do { \
        auto _a = (a); auto _b = (b); \
        if (static_cast<long long>(_a) != static_cast<long long>(_b)) { \
            std::cerr << "  FAIL: " << #a << " == " << #b \
                      << " (got " << _a << " vs " << _b << ")" \
                      << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            ++testsFailed; \
            return; \
        } \
    } while (0)

#define RUN_TEST(fn) \
    do { \
        std::cout << "  " << #fn << "... "; \
        fn(); \
        ++testsPassed; \
        std::cout << "PASS\n"; \
    } while (0)

// ── helpers ─────────────────────────────────────────────────────────

static OrderRequest newOrder(OrderId id, Side side, Price price, Quantity qty) {
    return {OrderAction::New, side, id, price, qty};
}

static OrderRequest cancelOrder(OrderId id) {
    return {OrderAction::Cancel, Side::Buy, id, 0, 0};
}

static OrderRequest modifyOrder(OrderId id, Side side, Price price, Quantity qty) {
    return {OrderAction::Modify, side, id, price, qty};
}

static bool hasEvent(const std::vector<ExecutionReport>& events,
                     EventType type, OrderId id) {
    for (const auto& e : events)
        if (e.type == type && e.orderId == id) return true;
    return false;
}

static const ExecutionReport* findEvent(const std::vector<ExecutionReport>& events,
                                        EventType type, OrderId id) {
    for (const auto& e : events)
        if (e.type == type && e.orderId == id) return &e;
    return nullptr;
}

static const ExecutionReport* findFill(const std::vector<ExecutionReport>& events,
                                       OrderId aggressor, OrderId passive) {
    for (const auto& e : events)
        if (e.type == EventType::Filled &&
            e.orderId == aggressor && e.matchedOrderId == passive)
            return &e;
    return nullptr;
}

// ════════════════════════════════════════════════════════════════════
// INVARIANT 1: No crossed book — best bid < best ask after every op
// ════════════════════════════════════════════════════════════════════

void test_no_crossed_book_after_resting() {
    OrderBook book;
    book.processOrder(newOrder(1, Side::Buy,  100, 10));
    book.processOrder(newOrder(2, Side::Sell, 105, 10));

    ASSERT_TRUE(book.bestBid() < book.bestAsk());
    ASSERT_EQ(book.bestBid(), 100);
    ASSERT_EQ(book.bestAsk(), 105);
}

void test_no_crossed_book_after_partial_fill() {
    OrderBook book;
    book.processOrder(newOrder(1, Side::Sell, 100, 20));
    book.processOrder(newOrder(2, Side::Buy,  100, 10)); // partial fill of ask

    // incoming fully filled, resting partially filled — ask still at 100
    ASSERT_EQ(book.bestAsk(), 100);
    ASSERT_EQ(book.bestBid(), 0); // no bids resting
    ASSERT_EQ(book.orderCount(), 1);
}

void test_no_crossed_book_after_full_fill() {
    OrderBook book;
    book.processOrder(newOrder(1, Side::Sell, 100, 10));
    book.processOrder(newOrder(2, Side::Buy,  100, 10)); // exact fill

    ASSERT_EQ(book.bestAsk(), 0); // empty
    ASSERT_EQ(book.bestBid(), 0);
    ASSERT_EQ(book.orderCount(), 0);
}

// ════════════════════════════════════════════════════════════════════
// INVARIANT 2: Price priority — better prices match first
// ════════════════════════════════════════════════════════════════════

void test_price_priority_buy_hits_lowest_ask() {
    OrderBook book;
    book.processOrder(newOrder(1, Side::Sell, 105, 10));
    book.processOrder(newOrder(2, Side::Sell, 100, 10)); // lower ask
    book.processOrder(newOrder(3, Side::Sell, 110, 10));

    // buy at 110 should hit the 100 ask first
    book.processOrder(newOrder(4, Side::Buy, 110, 5));
    auto* fill = findFill(book.events(), 4, 2);
    ASSERT_TRUE(fill != nullptr);
    ASSERT_EQ(fill->price, 100); // traded at resting price (best ask)
    ASSERT_EQ(fill->fillQuantity, 5u);
}

void test_price_priority_sell_hits_highest_bid() {
    OrderBook book;
    book.processOrder(newOrder(1, Side::Buy, 95,  10));
    book.processOrder(newOrder(2, Side::Buy, 100, 10)); // higher bid
    book.processOrder(newOrder(3, Side::Buy, 90,  10));

    // sell at 90 should hit the 100 bid first
    book.processOrder(newOrder(4, Side::Sell, 90, 5));
    auto* fill = findFill(book.events(), 4, 2);
    ASSERT_TRUE(fill != nullptr);
    ASSERT_EQ(fill->price, 100); // traded at resting price (best bid)
}

void test_price_priority_sweeps_multiple_levels() {
    OrderBook book;
    book.processOrder(newOrder(1, Side::Sell, 100, 5));
    book.processOrder(newOrder(2, Side::Sell, 101, 5));
    book.processOrder(newOrder(3, Side::Sell, 102, 5));

    // buy 12 at 102 should sweep 100 (5), 101 (5), 102 (2)
    book.processOrder(newOrder(4, Side::Buy, 102, 12));
    const auto& evts = book.events();

    auto* f1 = findFill(evts, 4, 1);
    auto* f2 = findFill(evts, 4, 2);
    auto* f3 = findFill(evts, 4, 3);

    ASSERT_TRUE(f1 != nullptr);
    ASSERT_TRUE(f2 != nullptr);
    ASSERT_TRUE(f3 != nullptr);

    ASSERT_EQ(f1->price, 100);
    ASSERT_EQ(f1->fillQuantity, 5u);
    ASSERT_EQ(f2->price, 101);
    ASSERT_EQ(f2->fillQuantity, 5u);
    ASSERT_EQ(f3->price, 102);
    ASSERT_EQ(f3->fillQuantity, 2u);

    // order 3 should still rest with 3 remaining
    ASSERT_EQ(book.orderCount(), 1);
    ASSERT_EQ(book.bestAsk(), 102);
}

// ════════════════════════════════════════════════════════════════════
// INVARIANT 3: Time priority (FIFO) — same price, earlier order first
// ════════════════════════════════════════════════════════════════════

void test_time_priority_fifo_at_same_price() {
    OrderBook book;
    book.processOrder(newOrder(1, Side::Sell, 100, 10)); // first
    book.processOrder(newOrder(2, Side::Sell, 100, 10)); // second
    book.processOrder(newOrder(3, Side::Sell, 100, 10)); // third

    // buy 15 — should fill order 1 (10) then order 2 (5)
    book.processOrder(newOrder(4, Side::Buy, 100, 15));
    const auto& evts = book.events();

    auto* f1 = findFill(evts, 4, 1);
    auto* f2 = findFill(evts, 4, 2);
    auto* f3 = findFill(evts, 4, 3);

    ASSERT_TRUE(f1 != nullptr);
    ASSERT_EQ(f1->fillQuantity, 10u);
    ASSERT_TRUE(f2 != nullptr);
    ASSERT_EQ(f2->fillQuantity, 5u);
    ASSERT_TRUE(f3 == nullptr); // order 3 not reached
}

void test_time_priority_partial_fill_keeps_position() {
    OrderBook book;
    book.processOrder(newOrder(1, Side::Sell, 100, 20));
    book.processOrder(newOrder(2, Side::Sell, 100, 10));

    // partial fill of order 1
    book.processOrder(newOrder(3, Side::Buy, 100, 5));
    ASSERT_EQ(book.orderCount(), 2);

    // next buy should continue with order 1 (15 remaining), not skip to 2
    book.processOrder(newOrder(4, Side::Buy, 100, 15));
    auto* fill = findFill(book.events(), 4, 1);
    ASSERT_TRUE(fill != nullptr);
    ASSERT_EQ(fill->fillQuantity, 15u);
    ASSERT_TRUE(findFill(book.events(), 4, 2) == nullptr);

    ASSERT_EQ(book.orderCount(), 1); // only order 2 remains
}

// ════════════════════════════════════════════════════════════════════
// INVARIANT 4: Trade price is always the resting order's price
// ════════════════════════════════════════════════════════════════════

void test_fill_price_is_resting_price_buy_aggressor() {
    OrderBook book;
    book.processOrder(newOrder(1, Side::Sell, 100, 10));
    book.processOrder(newOrder(2, Side::Buy,  105, 10)); // aggressive buy

    auto* fill = findFill(book.events(), 2, 1);
    ASSERT_TRUE(fill != nullptr);
    ASSERT_EQ(fill->price, 100); // resting ask price, not incoming 105
}

void test_fill_price_is_resting_price_sell_aggressor() {
    OrderBook book;
    book.processOrder(newOrder(1, Side::Buy,  105, 10));
    book.processOrder(newOrder(2, Side::Sell, 100, 10)); // aggressive sell

    auto* fill = findFill(book.events(), 2, 1);
    ASSERT_TRUE(fill != nullptr);
    ASSERT_EQ(fill->price, 105); // resting bid price, not incoming 100
}

// ════════════════════════════════════════════════════════════════════
// INVARIANT 5: Cancel removes order completely
// ════════════════════════════════════════════════════════════════════

void test_cancel_removes_order() {
    OrderBook book;
    book.processOrder(newOrder(1, Side::Buy, 100, 10));
    ASSERT_EQ(book.orderCount(), 1);

    book.processOrder(cancelOrder(1));
    ASSERT_EQ(book.orderCount(), 0);
    ASSERT_EQ(book.bestBid(), 0);
    ASSERT_TRUE(hasEvent(book.events(), EventType::Canceled, 1));
}

void test_cancel_nonexistent_rejects() {
    OrderBook book;
    book.processOrder(cancelOrder(999));
    ASSERT_TRUE(hasEvent(book.events(), EventType::Rejected, 999));
    ASSERT_EQ(book.totalRejects(), 1u);
}

void test_cancel_removes_only_target() {
    OrderBook book;
    book.processOrder(newOrder(1, Side::Buy, 100, 10));
    book.processOrder(newOrder(2, Side::Buy, 100, 20));
    book.processOrder(newOrder(3, Side::Buy, 100, 30));

    book.processOrder(cancelOrder(2));
    ASSERT_EQ(book.orderCount(), 2);

    // order 1 and 3 should still be matchable
    book.processOrder(newOrder(4, Side::Sell, 100, 35));
    const auto& evts = book.events();
    auto* f1 = findFill(evts, 4, 1);
    auto* f3 = findFill(evts, 4, 3);
    ASSERT_TRUE(f1 != nullptr);
    ASSERT_EQ(f1->fillQuantity, 10u);
    ASSERT_TRUE(f3 != nullptr);
    ASSERT_EQ(f3->fillQuantity, 25u);
}

void test_cancel_cleans_up_empty_level() {
    OrderBook book;
    book.processOrder(newOrder(1, Side::Sell, 100, 10));
    ASSERT_EQ(book.askLevelCount(), 1u);

    book.processOrder(cancelOrder(1));
    ASSERT_EQ(book.askLevelCount(), 0u);
}

// ════════════════════════════════════════════════════════════════════
// INVARIANT 6: Modify is cancel-replace (loses time priority)
// ════════════════════════════════════════════════════════════════════

void test_modify_loses_time_priority() {
    OrderBook book;
    book.processOrder(newOrder(1, Side::Sell, 100, 10)); // first
    book.processOrder(newOrder(2, Side::Sell, 100, 10)); // second

    // modify order 1 — same price/qty but loses position
    book.processOrder(modifyOrder(1, Side::Sell, 100, 10));

    // buy should match order 2 first (it's now earlier in FIFO)
    book.processOrder(newOrder(3, Side::Buy, 100, 5));
    auto* fill = findFill(book.events(), 3, 2);
    ASSERT_TRUE(fill != nullptr);
    ASSERT_EQ(fill->fillQuantity, 5u);
}

void test_modify_changes_price() {
    OrderBook book;
    book.processOrder(newOrder(1, Side::Sell, 100, 10));
    ASSERT_EQ(book.bestAsk(), 100);

    book.processOrder(modifyOrder(1, Side::Sell, 105, 10));
    ASSERT_EQ(book.bestAsk(), 105);
    ASSERT_EQ(book.orderCount(), 1);
}

void test_modify_changes_quantity() {
    OrderBook book;
    book.processOrder(newOrder(1, Side::Sell, 100, 10));

    book.processOrder(modifyOrder(1, Side::Sell, 100, 50));

    // buy 50 should fully fill the modified order
    book.processOrder(newOrder(2, Side::Buy, 100, 50));
    auto* fill = findFill(book.events(), 2, 1);
    ASSERT_TRUE(fill != nullptr);
    ASSERT_EQ(fill->fillQuantity, 50u);
    ASSERT_EQ(book.orderCount(), 0);
}

void test_modify_nonexistent_rejects() {
    OrderBook book;
    book.processOrder(modifyOrder(999, Side::Buy, 100, 10));
    ASSERT_TRUE(hasEvent(book.events(), EventType::Rejected, 999));
}

void test_modify_can_trigger_immediate_fill() {
    OrderBook book;
    book.processOrder(newOrder(1, Side::Buy, 100, 10));
    book.processOrder(newOrder(2, Side::Sell, 110, 10));

    // modify sell order to a price that crosses the bid
    book.processOrder(modifyOrder(2, Side::Sell, 100, 10));
    ASSERT_TRUE(hasEvent(book.events(), EventType::Filled, 2));
    ASSERT_EQ(book.orderCount(), 0);
}

// ════════════════════════════════════════════════════════════════════
// INVARIANT 7: Aggregate quantity consistency
// ════════════════════════════════════════════════════════════════════

void test_aggregate_quantity_after_resting() {
    OrderBook book;
    book.processOrder(newOrder(1, Side::Buy, 100, 10));
    book.processOrder(newOrder(2, Side::Buy, 100, 20));
    book.processOrder(newOrder(3, Side::Buy, 100, 30));

    // total at price 100 should be 60
    // verify via sweep: sell 60 at 100 should fully consume all 3
    book.processOrder(newOrder(4, Side::Sell, 100, 60));
    ASSERT_EQ(book.orderCount(), 0);
    ASSERT_EQ(book.bidLevelCount(), 0u);
}

void test_aggregate_quantity_after_partial_fill() {
    OrderBook book;
    book.processOrder(newOrder(1, Side::Buy, 100, 30));
    book.processOrder(newOrder(2, Side::Buy, 100, 20));

    // sell 15 — leaves 15 + 20 = 35 remaining at price 100
    book.processOrder(newOrder(3, Side::Sell, 100, 15));

    // sell 35 should exactly consume the rest
    book.processOrder(newOrder(4, Side::Sell, 100, 35));
    ASSERT_EQ(book.orderCount(), 0);
    ASSERT_EQ(book.bidLevelCount(), 0u);
}

void test_aggregate_quantity_after_cancel() {
    OrderBook book;
    book.processOrder(newOrder(1, Side::Sell, 100, 10));
    book.processOrder(newOrder(2, Side::Sell, 100, 20));
    book.processOrder(newOrder(3, Side::Sell, 100, 30));

    book.processOrder(cancelOrder(2)); // remove 20

    // buy 40 = 10 (order 1) + 30 (order 3), exactly consuming the level
    book.processOrder(newOrder(4, Side::Buy, 100, 40));
    ASSERT_EQ(book.orderCount(), 0);
    ASSERT_EQ(book.askLevelCount(), 0u);
}

// ════════════════════════════════════════════════════════════════════
// INVARIANT 8: Empty levels are removed immediately
// ════════════════════════════════════════════════════════════════════

void test_empty_level_removed_after_full_fill() {
    OrderBook book;
    book.processOrder(newOrder(1, Side::Sell, 100, 10));
    ASSERT_EQ(book.askLevelCount(), 1u);

    book.processOrder(newOrder(2, Side::Buy, 100, 10));
    ASSERT_EQ(book.askLevelCount(), 0u);
}

void test_empty_level_removed_after_sweep() {
    OrderBook book;
    book.processOrder(newOrder(1, Side::Sell, 100, 5));
    book.processOrder(newOrder(2, Side::Sell, 101, 5));
    ASSERT_EQ(book.askLevelCount(), 2u);

    book.processOrder(newOrder(3, Side::Buy, 101, 10));
    ASSERT_EQ(book.askLevelCount(), 0u);
}

// ════════════════════════════════════════════════════════════════════
// INVARIANT 9: Validation rejects bad orders
// ════════════════════════════════════════════════════════════════════

void test_reject_zero_quantity() {
    OrderBook book;
    book.processOrder(newOrder(1, Side::Buy, 100, 0));
    ASSERT_TRUE(hasEvent(book.events(), EventType::Rejected, 1));
    ASSERT_EQ(book.orderCount(), 0);
}

void test_reject_zero_price() {
    OrderBook book;
    book.processOrder(newOrder(1, Side::Buy, 0, 10));
    ASSERT_TRUE(hasEvent(book.events(), EventType::Rejected, 1));
    ASSERT_EQ(book.orderCount(), 0);
}

void test_reject_negative_price() {
    OrderBook book;
    book.processOrder(newOrder(1, Side::Buy, -5, 10));
    ASSERT_TRUE(hasEvent(book.events(), EventType::Rejected, 1));
    ASSERT_EQ(book.orderCount(), 0);
}

// ════════════════════════════════════════════════════════════════════
// INVARIANT 10: No match when prices don't cross
// ════════════════════════════════════════════════════════════════════

void test_no_match_when_prices_dont_cross() {
    OrderBook book;
    book.processOrder(newOrder(1, Side::Sell, 105, 10));
    book.processOrder(newOrder(2, Side::Buy,  100, 10)); // bid < ask

    ASSERT_EQ(book.orderCount(), 2);
    ASSERT_EQ(book.totalFills(), 0u);
    ASSERT_EQ(book.bestBid(), 100);
    ASSERT_EQ(book.bestAsk(), 105);
}

// ════════════════════════════════════════════════════════════════════
// INVARIANT 11: Fill event accounting
// ════════════════════════════════════════════════════════════════════

void test_fill_quantities_sum_correctly() {
    OrderBook book;
    book.processOrder(newOrder(1, Side::Sell, 100, 5));
    book.processOrder(newOrder(2, Side::Sell, 101, 5));
    book.processOrder(newOrder(3, Side::Sell, 102, 5));

    book.processOrder(newOrder(4, Side::Buy, 102, 12));

    Quantity totalFilled = 0;
    for (const auto& e : book.events())
        if (e.type == EventType::Filled)
            totalFilled += e.fillQuantity;

    ASSERT_EQ(totalFilled, 12u);
}

void test_matched_remaining_quantity_correct() {
    OrderBook book;
    book.processOrder(newOrder(1, Side::Sell, 100, 20));

    book.processOrder(newOrder(2, Side::Buy, 100, 8));
    auto* fill = findFill(book.events(), 2, 1);
    ASSERT_TRUE(fill != nullptr);
    ASSERT_EQ(fill->matchedRemainingQuantity, 12u); // 20 - 8

    book.processOrder(newOrder(3, Side::Buy, 100, 12));
    fill = findFill(book.events(), 3, 1);
    ASSERT_TRUE(fill != nullptr);
    ASSERT_EQ(fill->matchedRemainingQuantity, 0u); // fully consumed
}

// ════════════════════════════════════════════════════════════════════
// INVARIANT 12: Stats accuracy
// ════════════════════════════════════════════════════════════════════

void test_stats_accuracy() {
    OrderBook book;
    book.processOrder(newOrder(1, Side::Sell, 100, 10));
    book.processOrder(newOrder(2, Side::Buy,  100, 10)); // fill
    book.processOrder(newOrder(3, Side::Buy,  95,  10)); // rest
    book.processOrder(cancelOrder(3));                    // cancel
    book.processOrder(cancelOrder(999));                  // reject

    ASSERT_EQ(book.totalOrders(), 3u);   // 3 new orders
    ASSERT_EQ(book.totalFills(), 1u);
    ASSERT_EQ(book.totalVolume(), 10u);
    ASSERT_EQ(book.totalCancels(), 1u);
    ASSERT_EQ(book.totalRejects(), 1u);  // cancel of 999
}

// ════════════════════════════════════════════════════════════════════
// STRESS: deterministic simulation invariant check
// ════════════════════════════════════════════════════════════════════

void test_simulation_no_rejects_with_sync() {
    OrderBook book(50000);
    MarketSimulator sim(SimulatorConfig{
        10000, 20, 1, 100, 0.15, 0.05, 50000, 123
    });

    while (!sim.done()) {
        OrderRequest req = sim.nextOrder();
        book.processOrder(req);

        for (const auto& evt : book.events()) {
            switch (evt.type) {
                case EventType::Acknowledged:
                    sim.trackActiveOrder(evt.orderId);
                    break;
                case EventType::Canceled:
                    sim.removeActiveOrder(evt.orderId);
                    break;
                case EventType::Filled:
                    if (evt.matchedRemainingQuantity == 0)
                        sim.removeActiveOrder(evt.matchedOrderId);
                    if (evt.remainingQuantity == 0)
                        sim.removeActiveOrder(evt.orderId);
                    break;
                default:
                    break;
            }
        }
    }

    ASSERT_EQ(book.totalRejects(), 0u);
    ASSERT_TRUE(book.totalFills() > 0);
    ASSERT_TRUE(book.totalCancels() > 0);

    // book must not be crossed
    if (book.bestBid() != 0 && book.bestAsk() != 0) {
        ASSERT_TRUE(book.bestBid() < book.bestAsk());
    }
}

// ════════════════════════════════════════════════════════════════════
// MAIN
// ════════════════════════════════════════════════════════════════════

int main() {
    std::cout << "\n=== Matching Engine Correctness Tests ===\n\n";

    std::cout << "[Invariant 1] No crossed book\n";
    RUN_TEST(test_no_crossed_book_after_resting);
    RUN_TEST(test_no_crossed_book_after_partial_fill);
    RUN_TEST(test_no_crossed_book_after_full_fill);

    std::cout << "\n[Invariant 2] Price priority\n";
    RUN_TEST(test_price_priority_buy_hits_lowest_ask);
    RUN_TEST(test_price_priority_sell_hits_highest_bid);
    RUN_TEST(test_price_priority_sweeps_multiple_levels);

    std::cout << "\n[Invariant 3] Time priority (FIFO)\n";
    RUN_TEST(test_time_priority_fifo_at_same_price);
    RUN_TEST(test_time_priority_partial_fill_keeps_position);

    std::cout << "\n[Invariant 4] Trade price = resting price\n";
    RUN_TEST(test_fill_price_is_resting_price_buy_aggressor);
    RUN_TEST(test_fill_price_is_resting_price_sell_aggressor);

    std::cout << "\n[Invariant 5] Cancel correctness\n";
    RUN_TEST(test_cancel_removes_order);
    RUN_TEST(test_cancel_nonexistent_rejects);
    RUN_TEST(test_cancel_removes_only_target);
    RUN_TEST(test_cancel_cleans_up_empty_level);

    std::cout << "\n[Invariant 6] Modify = cancel-replace\n";
    RUN_TEST(test_modify_loses_time_priority);
    RUN_TEST(test_modify_changes_price);
    RUN_TEST(test_modify_changes_quantity);
    RUN_TEST(test_modify_nonexistent_rejects);
    RUN_TEST(test_modify_can_trigger_immediate_fill);

    std::cout << "\n[Invariant 7] Aggregate quantity consistency\n";
    RUN_TEST(test_aggregate_quantity_after_resting);
    RUN_TEST(test_aggregate_quantity_after_partial_fill);
    RUN_TEST(test_aggregate_quantity_after_cancel);

    std::cout << "\n[Invariant 8] Empty levels removed\n";
    RUN_TEST(test_empty_level_removed_after_full_fill);
    RUN_TEST(test_empty_level_removed_after_sweep);

    std::cout << "\n[Invariant 9] Validation\n";
    RUN_TEST(test_reject_zero_quantity);
    RUN_TEST(test_reject_zero_price);
    RUN_TEST(test_reject_negative_price);

    std::cout << "\n[Invariant 10] No match when prices don't cross\n";
    RUN_TEST(test_no_match_when_prices_dont_cross);

    std::cout << "\n[Invariant 11] Fill event accounting\n";
    RUN_TEST(test_fill_quantities_sum_correctly);
    RUN_TEST(test_matched_remaining_quantity_correct);

    std::cout << "\n[Invariant 12] Stats accuracy\n";
    RUN_TEST(test_stats_accuracy);

    std::cout << "\n[Stress] Simulation invariant check\n";
    RUN_TEST(test_simulation_no_rejects_with_sync);

    std::cout << "\n=== Results: " << testsPassed << " passed, "
              << testsFailed << " failed ===\n" << std::endl;

    return testsFailed > 0 ? 1 : 0;
}
