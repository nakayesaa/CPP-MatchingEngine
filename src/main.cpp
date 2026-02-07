// Entry point where we set up the simulator and order book, then run the main loop.
// basically the main loop is simple that it repeats these following steps:
//   1. Simulator generates a request, either its new, cancel, or modify
//   2. Order book processes the request and produces a list of events
//   3. Simulator processes the events to keep its state in sync
//   4. Repeat until we hit the total message count

// mind you that the simulator using events from the order book to track its active orders,
// so it can pick valid targets for cancels and modifies. this keeps the simulator view consistent with the actual book state.

// the primary metric at the end of the run is throughput that we got from the elapsed time and total messages processed.

#include "OrderBook.h"
#include "MarketSimulator.h"

#include <iostream>
#include <chrono>
#include <iomanip>

int main() {
    SimulatorConfig config;
    config.midPrice      = 10000;
    config.spreadTicks   = 20;
    config.minQuantity   = 1;
    config.maxQuantity   = 100;
    config.cancelRate    = 0.15;
    config.modifyRate    = 0.05;
    config.totalMessages = 1000000;
    config.seed          = 42;

    OrderBook book(200000);
    MarketSimulator simulator(config);

    std::cout << "Running " << config.totalMessages << " messages\n" << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    while (!simulator.done()) {
        OrderRequest request = simulator.nextOrder();
        book.processOrder(request);

        for (const auto& event : book.events()) {
            switch (event.type) {
                case EventType::Acknowledged:
                    simulator.trackActiveOrder(event.orderId);
                    break;
                case EventType::Canceled:
                    simulator.removeActiveOrder(event.orderId);
                    break;
                case EventType::Filled:
                    if (event.matchedRemainingQuantity == 0)
                        simulator.removeActiveOrder(event.matchedOrderId);
                    if (event.remainingQuantity == 0)
                        simulator.removeActiveOrder(event.orderId);
                    break;
                default:
                    break;
            }
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<double>(end - start).count();

    book.printBook(10);
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Messages processed : " << config.totalMessages << "\n"
              << "  Total orders       : " << book.totalOrders()  << "\n"
              << "  Total fills        : " << book.totalFills()   << "\n"
              << "  Total volume       : " << book.totalVolume()  << "\n"
              << "  Total cancels      : " << book.totalCancels() << "\n"
              << "  Total rejects      : " << book.totalRejects() << "\n"
              << "  Resting orders     : " << book.orderCount()   << "\n"
              << "  Bid levels         : " << book.bidLevelCount() << "\n"
              << "  Ask levels         : " << book.askLevelCount() << "\n"
              << "  Best bid           : " << book.bestBid()       << "\n"
              << "  Best ask           : " << book.bestAsk()       << "\n"
              << "  Elapsed time       : " << elapsed << " s\n"
              << "  Throughput         : "
              << static_cast<double>(config.totalMessages) / elapsed
              << " messages/sec"
              << std::endl;
    return 0;
}
