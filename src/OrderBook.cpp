
#include "OrderBook.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

// constructor initializes the object pool with given capacity.
OrderBook::OrderBook(size_t poolCapacity) : pool_(poolCapacity) {}

// this would be the main entry point for processing an incoming order request.
void OrderBook::processOrder(const OrderRequest& request){
    events_.clear();
    switch(request.action){
        case OrderAction::New:
            handleNew(request);
            break;
        case OrderAction::Cancel:
            handleCancel(request);
            break;
        case OrderAction::Modify:
            handleModify(request);
            break;
    }
}

// acquire a pre-allocated order from the pool and using request data to fill it.
// then try to match it with resting orders on the opposite side.
// if it partial fills, we put order into the book as a resting order, otherwise we release the object back to the pool.
void OrderBook::handleNew(const OrderRequest& request){
    ++totalOrders_;
    if(request.quantity > 0 && request.price <= 0){
        ++totalRejects_;
        emit(EventType::Rejected, request.id, 0, request.price, 0, 0);
        return;
    }

    Order* order = pool_.acquire();
    order->id = request.id;
    order->side = request.side;
    order->price = request.price;
    order->quantity = request.quantity;
    order->remainingQuantity = request.quantity;

    matchOrder(order);

    if(order->remainingQuantity > 0){
        restOrder(order);
        emit(EventType::Acknowledged, order->id, 0, order->price, 0, order->remainingQuantity);
    }else pool_.release(order);
}

// find the order by id using the index, if the id is not found we reject it.
// if its there, we remove it from the book and release the object back to the pool.
void OrderBook::handleCancel(const OrderRequest& request){
    auto orderIndex = orderIndex_.find(request.id);
    if(orderIndex == orderIndex_.end()){
        ++totalRejects_;
        emit(EventType::Rejected, request.id, 0,0,0,0);
        return;
    }

    Order* order = orderIndex->second;
    Quantity remaining = order->remainingQuantity;
    Price price = order->price;

    removeFromBook(order);
    pool_.release(order);
    ++totalCancels_;
    emit(EventType::Canceled, request.id, 0, price, 0, remaining);
}

// cancel the existing order, then create a new order with the new data
// if the existing order id is not found we reject it.
// otherwise, we remove the old order from the book and release it back to the pool,
// then we handle the new order as usual using new request data.
void OrderBook::handleModify(const OrderRequest& request){
    auto orderIndex = orderIndex_.find(request.id);
    if (orderIndex == orderIndex_.end()) {
        ++totalRejects_;
        emit(EventType::Rejected, request.id, 0, 0, 0, 0);
        return;
    }
    Order* oldOrder = orderIndex->second;
    Price oldPrice = oldOrder->price;
    Quantity oldRemaining = oldOrder->remainingQuantity;

    removeFromBook(oldOrder);
    pool_.release(oldOrder);
    emit(EventType::Canceled, request.id, 0, oldPrice, 0, oldRemaining);

    handleNew(request);
}


// basically we are trying to walk the opposite side of the book to get resting orders that are crossing the price of incoming order.
// as long as there is remaining quantity on incoming order and there are price levels on opposite side, we keep trying to match.
// if theres a match, we take resting order using time priority (level.orders.front() : the most front order in thsi price level).
// calculate the fill quantity and modify the reamining quantities of both side that are matches.
// if opposide side is fully filled, we remove from from the book, erase from index, and release it back to the pool.
void OrderBook::matchOrder(Order* incomingOrder){
    auto matchSide = [&](auto& levels){
        while(incomingOrder->remainingQuantity > 0 && !levels.empty()){
            auto bestLevels = levels.begin();
            PriceLevel& level = bestLevels->second;

            bool orderMatch = (incomingOrder->side == Side::Buy)? incomingOrder->price >= level.price : incomingOrder->price <= level.price;
            if(!orderMatch) break;

            while(incomingOrder->remainingQuantity >0 && !level.empty()){
                Order* restingOrder = level.orders.front();
                Quantity fillQuantity       = std::min(incomingOrder->remainingQuantity, restingOrder->remainingQuantity);

                incomingOrder->remainingQuantity -= fillQuantity;
                restingOrder->remainingQuantity -= fillQuantity;
                level.reduceQuantity(fillQuantity);

                ++totalFills_;
                totalVolume_ += fillQuantity;
                emit(EventType::Filled, incomingOrder->id, restingOrder->id,
                     restingOrder->price, fillQuantity, incomingOrder->remainingQuantity,
                     restingOrder->remainingQuantity);

                if(restingOrder->remainingQuantity == 0){
                    level.orders.remove(restingOrder);
                    orderIndex_.erase(restingOrder->id);
                    pool_.release(restingOrder);
                    --level.orderCount;
                }
                if(level.empty()) levels.erase(bestLevels);
            }
        }
    };
    if (incomingOrder->side == Side::Buy) matchSide(asks_);
    else matchSide(bids_);
}

// place a partially filled or unfilled order into the book as a resting order.
// if the price level doesnt exist, we create new price level entry in the map using add order.
// put the order into the index for fast lookup when we need it.
void OrderBook::restOrder(Order* incomingOrder){
    if(incomingOrder->side == Side::Buy){
        auto& level = bids_[incomingOrder->price];
        if(level.orderCount == 0) level.price = incomingOrder->price;
        level.addOrder(incomingOrder);
    }else{
        auto& level = asks_[incomingOrder->price];
        if(level.orderCount == 0) level.price = incomingOrder->price;
        level.addOrder(incomingOrder);
    }
    orderIndex_[incomingOrder->id] = incomingOrder;
}

// unlink an order from its price level and erase it from the index.
// find a price level on side of the order using its price.
// if price level exist, we remove the order from the price level using remove order.
// if the price level becomes empty after removal, we erase the price level.
// erase the index entry for the order id.
void OrderBook::removeFromBook(Order* order){
    if(order->side == Side::Buy){
        auto price = bids_.find(order->price);
        if(price != bids_.end()){
            price->second.removeOrder(order);
            if(price->second.empty()) bids_.erase(price);
        }
    }else{
        auto price = asks_.find(order->price);
        if(price != asks_.end()){
            price->second.removeOrder(order);
            if(price->second.empty()) asks_.erase(price);
        }
    }
    orderIndex_.erase(order->id);
}

// just for the reporting purpose after processing each order request.
void OrderBook::emit(EventType type, OrderId id, OrderId matchId,
                     Price price, Quantity fillQty, Quantity remaining,
                     Quantity matchedRemaining) {
    events_.push_back({type, id, matchId, price, fillQty, remaining, matchedRemaining});
}

Price OrderBook::bestBid() const {
    return bids_.empty() ? 0 : bids_.begin()->first;
}

Price OrderBook::bestAsk() const {
    return asks_.empty() ? 0 : asks_.begin()->first;
}

// print out the top N levels of the book for both sides.
void OrderBook::printBook(int depth) const {
    std::cout << "Order Book\n";
    int count = 0;
    for (auto it = asks_.begin(); it != asks_.end() && count < depth; ++it, ++count) {
        const auto& [price, lvl] = *it;
        std::cout << "ask : " << price
                  << " with quantity of " << lvl.aggregateQuantity
                  << " (" << lvl.orderCount << " orders)\n";
    }
    std::cout << "\n";
    count = 0;
    for (auto it = bids_.begin(); it != bids_.end() && count < depth; ++it, ++count) {
        const auto& [price, lvl] = *it;
        std::cout << "bid : " << price
                  << " with quantity of " << lvl.aggregateQuantity
                  << " (" << lvl.orderCount << " orders)\n";
    }

    std::cout << std::endl;
}