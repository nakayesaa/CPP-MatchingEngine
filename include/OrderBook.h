
//this file is a one hell of a code 
// so basically this is the order book class that holds all the logic for processing orders


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

    std::map<Price, PriceLevel, std::greater<Price>> bids_;
    std::map<Price, PriceLevel> asks_;

    std::unordered_map<OrderId, Order*> orderIndex_;
    ObjectPool<Order> pool_;

    std::vector<ExecutionReport> events_;

    uint64_t totalFills_   = 0;
    uint64_t totalVolume_  = 0;
    uint64_t totalCancels_ = 0;
    uint64_t totalRejects_ = 0;
    uint64_t totalOrders_  = 0;
};
