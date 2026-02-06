#pragma once
#include <cstddef>

//basically it is doubly linked list, but only works with objects that have prev and next pointers
//so we just give an existing object a pointer to the prev and next object, instead of wrapping it in a node structure
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

































