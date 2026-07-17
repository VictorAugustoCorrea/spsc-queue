#pragma once

#ifndef LEARN001_SPSC_QUEUE_H
#define LEARN001_SPSC_QUEUE_H

#include <atomic>
#include <cstddef>
#include <type_traits>

inline constexpr std::size_t CACHE_LINE_SIZE = 64;

template <typename T, size_t Capacity>
class SPSC_Queue {
    static_assert((Capacity & Capacity - 1) == 0, "Capacity must be a power of 2");
    static_assert(Capacity >= 2, "Capacity must be >= 2");
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable(POD) to perform in HFT");
public:
    SPSC_Queue() = default;
    SPSC_Queue(const SPSC_Queue& ) = delete;
    SPSC_Queue(const SPSC_Queue&&) = delete;
    SPSC_Queue& operator = (const SPSC_Queue& ) = delete;
    SPSC_Queue& operator = (const SPSC_Queue&&) = delete;

    bool try_push(const T& item) noexcept {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = increment(tail);

        if (next_tail == cached_head_) {
            cached_head_ = head_.load(std::memory_order_acquire);
            if (next_tail == cached_head_) {
                return false;
            }
        }

        buffer_[tail] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    bool try_pop(T& item) noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);

        if (head == cached_tail_) {
            cached_tail_ = tail_.load(std::memory_order_acquire);
            if (head == cached_tail_) {
                return false;
            }
        }

        item = buffer_[head];
        head_.store(increment(head), std::memory_order_release);
        return true;
    }

    [[nodiscard]]
    size_t size() const noexcept {
        const size_t t = tail_.load(std::memory_order_acquire);
        const size_t h = head_.load(std::memory_order_acquire);
        return t - h & mask_;
    }

    [[nodiscard]]
    bool empty() const noexcept { return size() == 0; }
    static constexpr size_t capacity() noexcept { return Capacity; }

private:
    static constexpr size_t mask_ =  Capacity - 1;
    static constexpr size_t increment(const size_t idx) noexcept {
        return idx + 1 & mask_;
    }

    alignas(CACHE_LINE_SIZE) std::atomic<size_t> head_{0};
    alignas(CACHE_LINE_SIZE) size_t cached_tail_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail_{0};
    alignas(CACHE_LINE_SIZE) std:: size_t cached_head_{0};
    alignas(CACHE_LINE_SIZE) T buffer_[Capacity];

};

#endif //LEARN001_SPSC_QUEUE_H
