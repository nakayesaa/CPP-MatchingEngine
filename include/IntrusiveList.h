
// Basically a doubly linked list where the nodes Are the objects themselves.
// so we already have the existing objects that we want to link together, and then we just need to assign their prev and next pointers accordingly.
// this ensure that we didnt have to allocate extra memory for list nodes 
// the object themselves is expected to have T* prev and T* next members for linking.
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































