// OrderBook.h
// ─────────────────────────────────────────────────────────────────────
// The central order book – this is where the actual matching happens.
//
// The book maintains two sorted sides:
//   bids_ – buy orders, sorted highest price first (std::greater)
//   asks_ – sell orders, sorted lowest price first (default std::less)
//
// Both sides are std::map<Price, PriceLevel>, which gives us O(log n)
// insertion and removal of price levels, and O(1) access to the best
// bid/ask since std::map keeps things sorted via a red-black tree.
//
// orderIndex_ is an unordered_map that maps OrderId → Order*, so we
// can look up any order in O(1) for cancellations and modifications.
// Without this, canceling an order would require searching through
// price levels, which would be way too slow.
//
// The order lifecycle goes like this:
//   1. processOrder() receives an OrderRequest and dispatches it
//   2. handleNew() acquires from pool → tries to match → rests remainder
//   3. handleCancel() looks up by id → removes from book → releases to pool
//   4. handleModify() is cancel-then-new (no in-place modification,
//      which means modified orders lose their time priority – this is
//      standard exchange behavior)
//
// matchOrder() walks the opposite side of the book and fills as much
// as possible at each price level. restOrder() places whatever is left
// into the appropriate side. removeFromBook() unlinks from the price
// level and cleans up empty levels.
//
// events_ is a per-request scratch buffer of ExecutionReports. It gets
// cleared at the start of each processOrder() call, so the caller can
// read the events produced by the most recent request.
//
// Stats (totalFills_, totalVolume_, etc.) are tracked incrementally
// for the final performance summary, no post-processing needed.
// ─────────────────────────────────────────────────────────────────────

#pragma once
#include "Types.h"
#include "Order.h"
#include "PriceLevel.h"
#include "ObjectPool.h"
#include <map>
#include <unordered_map>
#include <vector>
#include <functional>
#include <cstddef>

class OrderBook {
public:
    explicit OrderBook(size_t poolCapacity = 100000);

    void processOrder(const OrderRequest& request);

    Price  bestBid() const;
    Price  bestAsk() const;
    size_t bidLevelCount() const { return bids_.size(); }
    size_t askLevelCount() const { return asks_.size(); }
    size_t orderCount()    const { return orderIndex_.size(); }

    const std::vector<ExecutionReport>& events() const { return events_; }

    void printBook(int depth = 10) const;

    uint64_t totalFills()    const { return totalFills_; }
    uint64_t totalVolume()   const { return totalVolume_; }
    uint64_t totalCancels()  const { return totalCancels_; }
    uint64_t totalRejects()  const { return totalRejects_; }
    uint64_t totalOrders()   const { return totalOrders_; }

private:
    void handleNew(const OrderRequest& request);
    void handleCancel(const OrderRequest& request);
    void handleModify(const OrderRequest& request);

    void matchOrder(Order* order);
    void restOrder(Order* order);
    void removeFromBook(Order* order);

    void emit(EventType type, OrderId id, OrderId matchId,
              Price price, Quantity fillQty, Quantity remaining,
              Quantity matchedRemaining = 0);

    // bids, highest price first
    std::map<Price, PriceLevel, std::greater<Price>> bids_;
    // asks, lowest price first
    std::map<Price, PriceLevel> asks_;

    std::unordered_map<OrderId, Order*> orderIndex_;
    ObjectPool<Order> pool_;

    std::vector<ExecutionReport> events_;

    // stats for final report
    uint64_t totalFills_   = 0;
    uint64_t totalVolume_  = 0;
    uint64_t totalCancels_ = 0;
    uint64_t totalRejects_ = 0;
    uint64_t totalOrders_  = 0;
};
