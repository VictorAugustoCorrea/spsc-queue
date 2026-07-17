#include "SPSC_queue.h"

#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <x86intrin.h>

struct Message {
    uint64_t send_tsc;
    uint64_t seq;
    double price;
    uint32_t symbol_id;
};

static_assert(std::is_trivially_copyable_v<Message>);

constexpr size_t KCapacity  = 1 << 16;
constexpr uint64_t KNumMsgs = 5'000'000;

static uint64_t rdtsc() {
    _mm_lfence();
    return __rdtsc();
}

int main() {
    SPSC_Queue<Message, KCapacity> queue;
    std::atomic start { false };
    std::atomic done  { false };

    std::vector<uint64_t> latencies_cycles;
    latencies_cycles.reserve(KNumMsgs);

    std::thread producer([&] {
        while (!start.load(std::memory_order_acquire)) { /* spin until the signal */ }

        for (uint64_t i = 0; i < KNumMsgs; ++i) {
            Message m {
                rdtsc(),
                i,
                100.25 + static_cast<double>(i % 100) * 0.01,
                static_cast<uint32_t>(i % 500)
            };
            while (!queue.try_push(m)) { }
        }
    });

    std::thread consumer([&] {
        while (!start.load(std::memory_order_acquire)) { /* spin */ }

        Message m = {};
        uint64_t expected_seq = 0;
        bool corrupted = false;

        for (uint64_t i = 0; i < KNumMsgs; ++i) {
            while (!queue.try_pop(m)) { }

            if (m.seq != expected_seq)
                corrupted = true;

            ++expected_seq;
            const uint64_t recv = rdtsc();
            latencies_cycles.push_back(recv - m.send_tsc);
        }
        if (corrupted)
            std::printf("Error: Out-of-order sequence! \n");
        else
            std::printf("Correctness: OK (%llu messages in exact order, none lost)\n",
                static_cast<unsigned long long>(KNumMsgs));
        done.store(true, std::memory_order_release);
    });

    const auto wall_start = std::chrono::steady_clock::now();
    start.store(true, std::memory_order_release);

    producer.join();
    consumer.join();

    const auto wall_end = std::chrono::steady_clock::now();

    const double secs  = std::chrono::duration<double>(wall_end - wall_start).count();
    const double msgs_per_sec = KNumMsgs/ secs;

    std::sort(latencies_cycles.begin(), latencies_cycles.end());
    const uint64_t c0 = rdtsc();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const uint64_t c1 = rdtsc();
    const double cycles_per_ns = static_cast<double>(c1 - c0) / 100.0 * 1'000'000.0;

    auto pct = [&] (const double p) {
        auto idx = static_cast<size_t>(p * static_cast<double>(latencies_cycles.size()));
        idx = std::min(idx, latencies_cycles.size() - 1);
        return latencies_cycles[idx];
    };

    std::printf("Messages: %llu\n", static_cast<unsigned long long>(KNumMsgs));
    std::printf("Time: %.4f s\n", secs);
    std::printf("Throughput: %.2f millions msgs/s\n", msgs_per_sec / 1e6);
    std::printf("Freq: %.3f GHz\n", cycles_per_ns);
    std::printf("Latency p50: %.1lu\n", pct(0.50));
    std::printf("Latency p90: %.1lu\n", pct(0.90));
    std::printf("Latency p99: %.1lu\n", pct(0.99));
    std::printf("Latency p99.9: %.1lu\n", pct(0.999));
    std::printf("Max Latency: %.1lu\n", pct(1.0));

    return 0;
}