#pragma once

#include <common/intrusive_ptr.h>
#include <mutex>

namespace NCommon {

////////////////////////////////////////////////////////////////////////////////

template <typename T>
class TAtomicIntrusivePtr {
public:
    TAtomicIntrusivePtr() = default;

    explicit TAtomicIntrusivePtr(const TIntrusivePtr<T>& ptr) : ptr_(ptr) {}

    ~TAtomicIntrusivePtr() = default;

    TAtomicIntrusivePtr(const TAtomicIntrusivePtr&) = delete;
    TAtomicIntrusivePtr& operator=(const TAtomicIntrusivePtr&) = delete;
    TAtomicIntrusivePtr(TAtomicIntrusivePtr&&) = delete;
    TAtomicIntrusivePtr& operator=(TAtomicIntrusivePtr&&) = delete;

    TIntrusivePtr<T> Acquire() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return ptr_;
    }

    TIntrusivePtr<T> Store(const TIntrusivePtr<T>& newPtr) {
        std::lock_guard<std::mutex> lock(mutex_);
        TIntrusivePtr<T> oldPtr = ptr_;
        ptr_ = newPtr;
        return oldPtr;
    }

private:
    TIntrusivePtr<T> ptr_;
    mutable std::mutex mutex_;
};

////////////////////////////////////////////////////////////////////////////////

} // namespace NCommon
