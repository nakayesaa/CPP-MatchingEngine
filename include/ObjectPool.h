// ObjectPool.h
// ─────────────────────────────────────────────────────────────────────
// A pre-allocated object pool for fixed-size objects.
//
// In a matching engine, we're creating and destroying orders constantly –
// potentially millions per second. Hitting the heap allocator every time
// would kill performance, so instead we allocate all the Order objects
// up front in one big contiguous vector and hand them out on demand.
//
// How it works:
//   - pool_ is a std::vector<T> that holds the actual objects in memory.
//     This is allocated once at construction and never resized.
//   - freeList_ is a stack of pointers (std::vector<T*>) pointing into pool_.
//     When you need an object, pop a pointer off the free list. When you're
//     done, push it back. That's it – O(1) acquire and release, no malloc.
//
// acquire() grabs an object from the free list. It asserts if the pool is
// exhausted, which is intentional – running out of pool space means the
// capacity was misconfigured, and we want to catch that immediately
// rather than silently degrading.
//
// release() resets the object (via T::reset()) and returns it to the free list.
// The reset is important so we don't accidentally carry stale state from
// a previous order into a new one.
// ─────────────────────────────────────────────────────────────────────

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