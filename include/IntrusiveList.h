// IntrusiveList.h
// ─────────────────────────────────────────────────────────────────────
// A doubly linked list where the nodes ARE the objects themselves.
//
// In a normal std::list, each element gets wrapped in an internal node
// that holds prev/next pointers plus your data. That means extra heap
// allocations, extra indirection, and the data itself can end up
// scattered across memory.
//
// An intrusive list flips that around – the object (in our case, Order)
// carries its own prev/next pointers. So we're just wiring up objects
// that already exist somewhere (in the ObjectPool), no wrapper needed.
// This is why PriceLevels can maintain ordered queues of orders without
// allocating anything – the orders are already sitting in the pool,
// and we just thread pointers between them.
//
// The tradeoff is that T must have `prev` and `next` pointer members,
// so this isn't a general-purpose container. But for an order book where
// we control the Order struct, it's a perfect fit.
//
// push_back() appends to the tail – this preserves FIFO order within
// a price level, so earlier orders get matched first.
// remove() unlinks a node in O(1) – critical for fast cancellations.
// ─────────────────────────────────────────────────────────────────────

#pragma once
#include <cstddef>

template <typename T>
class IntrusiveList {
public:
    void push_back(T* node) {
        node->prev = tail_;
        node->next = nullptr;
        if (tail_)
            tail_->next = node;
        else
            head_ = node;
        tail_ = node;
        ++size_;
    }
    void remove(T* node) {
        if (node->prev)
            node->prev->next = node->next;
        else
            head_ = node->next;

        if (node->next)
            node->next->prev = node->prev;
        else
            tail_ = node->prev;

        node->prev = nullptr;
        node->next = nullptr;
        --size_;
    }
    T*     front() const { return head_; }
    bool   empty() const { return head_ == nullptr; }
    size_t size()  const { return size_; }

private:
    T* head_ = nullptr;
    T* tail_ = nullptr;
    size_t size_ = 0;
};































