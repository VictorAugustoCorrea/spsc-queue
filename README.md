# spsc_queue

Lock-free single-producer/single-consumer ring buffer for low-latency pipelines (market data → strategy → execution). No locks, no allocation after init, no syscalls on the hot path.

`C++20` · `header-only` · `MIT`

## Usage

```cpp
#include "spsc_queue.hpp"

// payload must be trivially copyable
struct Tick { uint64_t ts; double price; uint32_t symbol_id; };

// capacity: power of 2
SPSCQueue<Tick, 1 << 16> queue;

// producer thread
queue.try_push(tick);

// consumer thread
Tick t;
if (queue.try_pop(t)) { /* ... */ }
```

## Build

```bash
make
./benchmark
```

## Memory layout

```
offset  0   ------------------------------ 64 bytes ------------------------------
        |  head_          atomic<size_t>   written by: consumer
        ------------------------------------------------------------------------
        |  cached_tail_   size_t           written by: consumer (local only)
        ------------------------------------------------------------------------
        |  tail_          atomic<size_t>   written by: producer
        ------------------------------------------------------------------------
        |  cached_head_   size_t           written by: producer (local only)
        ------------------------------------------------------------------------
        |  buffer_[N]     T                read/written by both, disjoint idx
        ------------------------------------------------------------------------
```

Each field sits in its own `alignas(64)` cache line — producer and consumer never invalidate each other's line.

## Design decisions

1. **Fixed-size ring buffer** — no allocation on the hot path; contiguous layout favors the hardware prefetcher.
2. **Power-of-2 capacity** — `% Capacity` becomes `& (Capacity - 1)`.
3. **T is trivially copyable** — push/pop reduce to a byte copy, no ctor/dtor/exception.
4. **Non-blocking API** — `try_push`/`try_pop` never call into the kernel.
5. **Local cache of the remote index** — avoids reading the other thread's cache line on every call.
6. **acquire/release memory order** — sufficient ordering, without `seq_cst`'s full barrier.
7. **alignas(64) against false sharing** — the single largest win in the design, see layout above.
8. **-O3 -march=native** — aggressive inlining, native instructions for the target CPU.

## Benchmark

5,000,000 messages, default build. Tens-of-nanoseconds latency requires dedicated physical cores per thread — see checklist below.

| metric      | result                |
|-------------|------------------------|
| correctness | exact order, 0 losses |
| throughput  | ~9.0M msgs/s          |
| capacity    | 65536 slots           |
| payload     | 24 bytes              |

## Production checklist

- Pin each thread to a dedicated physical core (`pthread_setaffinity_np`)
- `isolcpus` / `nohz_full` in the kernel
- Hyperthreading disabled on the cores in use
- Huge pages for the buffer
- Zero syscalls between data arrival and decision
- Cache warm-up before market open

## Files

| file             | description                                        |
|------------------|-----------------------------------------------------|
| `spsc_queue.hpp` | the queue — header-only, ~150 lines                |
| `benchmark.cpp`  | throughput, p50–p99.9 latency, correctness check   |
| `Makefile`       | build flags                                        |

## License

MIT
