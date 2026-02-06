#include "OrderBook.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

OrderBook::OrderBook(size_t poolCapacity) : pool_(poolCapacity) {}

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

void OrderBook::handleNew(const OrderRequest& request){
    ++totalOrders_;
    if(request.quantity > 0 && request.price <= 0){
        ++totalRejects_;
        emit(EventType::Rejected, request.id, 0, request.price, 0, 0);
        return;
    }
    //take(acquire) order object that alr pre allocated in object pool
    //assign the order with neccesary value from request
    Order* order = pool_.acquire();
    order->id = request.id;
    order->side = request.side;
    order->price = request.price;
    order->quantity = request.quantity;
    order->remainingQuantity = request.quantity;
    order->sequence = nextSequence_++;

    matchOrder(order);
    //check whether its fully fill or partial fill
    //if its partial, put it in the rest order. if its the opposite, release node to pool
    if(order->remainingQuantity > 0){
        restOrder(order);
        emit(EventType::Acknowledged, order->id, 0, order->price, 0, order->remainingQuantity);
    }else pool_.release(order);
}

void OrderBook::handleCancel(const OrderRequest& request){
    auto orderIndex = orderIndex_.find(request.id);
    //didnt find the id, cant cancel the order
    if(orderIndex == orderIndex_.end()){
        ++totalRejects_;
        emit(EventType::Rejected, request.id, 0,0,0,0);
        return;
    }

    //orderIndex was a std::pair with (first : Key(orderId), second : Order*(the value))
    //grab the value by pointer
    Order* order = orderIndex->second;
    Quantity remaining = order->remainingQuantity;
    Price price = order->price;

    removeFromBook(order);
    pool_.release(order);
    ++totalCancels_;
    emit(EventType::Canceled, request.id, 0, price, 0, remaining);
}

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
// ── resting ─────────────────────────────────────────────────────────
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
// ── remove from book (cancel path) ─────────────────────────────────
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

// ── event emission ──────────────────────────────────────────────────
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



// ── debug print ─────────────────────────────────────────────────────
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