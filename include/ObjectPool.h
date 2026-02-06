#pragma once
#include <vector>
#include <cstddef>
#include <cassert>

// Simple object pool for fixed size objects
//the pool itself is just a vector of objects, while
// the free list are being used to track available objects via pointers to the objects in the pool
template <typename T>
class ObjectPool {
public:
    explicit ObjectPool(size_t capacity) : pool_(capacity) {
        freeList_.reserve(capacity);
        for (size_t i = 0; i < capacity; ++i)
            freeList_.push_back(&pool_[i]);
    }

    T* acquire() {
        assert(!freeList_.empty());
        T* object = freeList_.back();
        freeList_.pop_back();
        return object;
    }

    void release(T* object) {
        object->reset();
        freeList_.push_back(object);
    }

    size_t available() const { return freeList_.size(); }
    size_t capacity()  const { return pool_.size(); }

private:
    std::vector<T>  pool_;
    std::vector<T*> freeList_;
};
