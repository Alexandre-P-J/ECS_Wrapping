#pragma once
#include <cassert>
#include <optional>

template <class T>
class opt_ref {
    T *ptr = nullptr;

public:
    opt_ref() noexcept = default;
    opt_ref(T &reference) noexcept : ptr(std::addressof(reference)) {}
    opt_ref(std::nullopt_t) noexcept {}

    inline bool valid() const noexcept {
        return ptr != nullptr;
    }

    // non const so opt_ref<bool> works
    inline operator bool() noexcept {
        return valid();
    }

    inline operator T&() const noexcept {
        assert(valid() && "Dereferencing null opt_ref");
        return *ptr;
    }

    inline T& operator*() noexcept {
        assert(valid() && "Dereferencing null opt_ref");
        return *ptr;
    }

    inline T* operator->() noexcept {
        assert(valid() && "Dereferencing null opt_ref");
        return ptr;
    }
};