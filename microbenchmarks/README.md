# Microbenchmarks

This directory contains Google Benchmark programs focused on low-level performance
of CRoaring operations.

## Binaries

- `bench`: dataset-backed and synthetic microbenchmarks (contains, unions, intersections, etc.)
- `synthetic_bench`: additional synthetic workloads

## Build

From the repository root:

```bash
cmake -B build -D ENABLE_ROARING_MICROBENCHMARKS=ON
cmake --build build
```

## Run

Default run:

```bash
./build/microbenchmarks/bench
```

Run with a specific realdata directory:

```bash
./build/microbenchmarks/bench benchmarks/realdata/wikileaks-noquotes
```

## Focusing on `contains()` cache behavior

To run only the cold/warm `contains()` microbenchmarks:

```bash
./build/microbenchmarks/bench --benchmark_filter="ContainsCold|ContainsWarm"
```

This filter is useful because it isolates one specific question:
how much of `contains()` latency is due to memory locality and cache residency.

- `ContainsCold*`: probes many different bitmaps once each (cache-unfriendly pattern).
- `ContainsWarm*`: probes the same bitmap repeatedly before moving on (cache-friendly pattern).

Both families normalize results using an `ns/query` counter, making direct comparison straightforward.
A large gap between `ContainsCold*` and `ContainsWarm*` typically indicates that memory/cache effects dominate.
A small gap suggests the compute path itself is the primary cost.

You will also see `Low`, `Mod`, and `High` variants, corresponding to different synthetic densities.
Comparing those variants helps characterize how density interacts with cache effects.

## Practical workflow

1. Run the filtered command above.
2. Compare `ContainsCold*` vs `ContainsWarm*` first.
3. Then compare `Low`, `Mod`, `High` within each family.
4. If needed, repeat with additional benchmark options (for example `--benchmark_repetitions=10`).
