// main.cpp
// ─────────────────────────────────────────────────────────────────────
// Entry point – wires the order book and market simulator together
// and runs a benchmark.
//
// The main loop is intentionally simple:
//   1. Simulator generates a request (new / cancel / modify)
//   2. Order book processes it and produces execution events
//   3. We feed those events back into the simulator so it knows which
//      orders are still alive (needed for valid cancel/modify targets)
//   4. Repeat until the simulator has sent all its messages
//
// The event feedback loop in step 3 is important – the simulator
// and the book don't share state directly. The simulator learns about
// the book's state through the same ExecutionReports that any real
// consumer would see. Acknowledged means the order is resting,
// Canceled means it's gone, and Filled with zero remaining means it
// was fully consumed.
//
// After the run, we print the final book state and a performance
// summary. The throughput number (messages/sec) is the main metric
// we care about when optimizing.
// ─────────────────────────────────────────────────────────────────────

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
        OrderRequest req = simulator.nextOrder();
        book.processOrder(req);

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
