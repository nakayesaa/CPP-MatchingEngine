
// A pre-allocated object pool for fixed-size objects.
// In a matching engine, we will constantly creating and destroying millions orders per second
// Hitting the heap allocator every time would kill performance, so instead we allocate all the Order objects in one big block

// the objectPool work as follows:
// pool_ is a std::vector<T> that holds the actual objects in memory. allocated once at construction and never changed.
// while freeList_, is a std::vector<T*> that holds pointers to available objects in the pool_.

// acquire() pop a pointer from freeList_ and return it, since this pointer points to an object in pool_.
//we can use this pointer to access the object directly without any extra allocation.

// while release() push the pointer back to freeList_ for future reuse.
// but since we will use the object again, we call reset() on the object to make sure there are no stale data leftover.
#pragma once
#include <vector>
#include <cstddef>
#include <cassert>

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