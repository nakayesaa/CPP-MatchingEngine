#include "OrderBook.h"
#include "MarketSimulator.h"

#include <iostream>
#include <chrono>
#include <iomanip>

int main() {
    SimulatorConfig cfg;
    cfg.midPrice      = 10000;
    cfg.spreadTicks   = 20;
    cfg.minQuantity   = 1;
    cfg.maxQuantity   = 100;
    cfg.cancelRate    = 0.15;
    cfg.modifyRate    = 0.05;
    cfg.totalMessages = 1000000;
    cfg.seed          = 42;

    OrderBook book(200000);
    MarketSimulator simulator(cfg);

    std::cout << "Running " << cfg.totalMessages << " messages\n" << std::endl;

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
    std::cout << "  Messages processed : " << cfg.totalMessages << "\n"
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
              << static_cast<double>(cfg.totalMessages) / elapsed
              << " messages/sec"
              << std::endl;
    return 0;
}
