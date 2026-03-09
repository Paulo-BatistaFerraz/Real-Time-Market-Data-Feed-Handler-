#pragma once

#include <cstddef>
#include <cstdlib>
#include <new>
#include "mdfh/common/constants.hpp"

namespace mdfh {

// Cache-line aligned allocation to prevent false sharing
// Used by SPSC queue for head/tail separation

template <typename T>
T* aligned_new(size_t alignment = CACHE_LINE) {
#ifdef _WIN32
    void* ptr = _aligned_malloc(sizeof(T), alignment);
#else
    void* ptr = std::aligned_alloc(alignment, sizeof(T));
#endif
    if (!ptr) throw std::bad_alloc();
    return new (ptr) T();  // placement new
}

template <typename T>
void aligned_delete(T* ptr) {
    if (!ptr) return;
    ptr->~T();
#ifdef _WIN32
    _aligned_free(ptr);
#else
    std::free(ptr);
#endif
}

// RAII wrapper for aligned memory
template <typename T>
struct AlignedPtr {
    T* ptr = nullptr;

    AlignedPtr() : ptr(aligned_new<T>()) {}
    ~AlignedPtr() { aligned_delete(ptr); }

    AlignedPtr(const AlignedPtr&) = delete;
    AlignedPtr& operator=(const AlignedPtr&) = delete;

    AlignedPtr(AlignedPtr&& other) noexcept : ptr(other.ptr) { other.ptr = nullptr; }
    AlignedPtr& operator=(AlignedPtr&& other) noexcept {
        if (this != &other) {
            aligned_delete(ptr);
            ptr = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }

    T& operator*() { return *ptr; }
    const T& operator*() const { return *ptr; }
    T* operator->() { return ptr; }
    const T* operator->() const { return ptr; }
    T* get() { return ptr; }
    const T* get() const { return ptr; }
};

}  // namespace mdfh
