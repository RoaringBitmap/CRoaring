# WebAssembly SIMD coverage and parity matrix

CRoaring currently uses SIMD for hot paths differently on each architecture. This doc is the **parity matrix** maintained alongside code changes.

## Neon vs Wasm128 rule of thumb

**Any `#elif defined(CROARING_USENEON)` SIMD implementation in `src/**/*.c` should have a matching `#elif defined(CROARING_WASM_SIMD)` branch in the same preprocessor chain**, unless documented here as deliberately omitted (hardware-only, not expressible on `wasm_simd128.h`, etc.).

CI checks this pairing with [`tools/check_wasm_simd_neon_pairing.sh`](../tools/check_wasm_simd_neon_pairing.sh).

## SIMD surface by area

| Translation unit | x86 SIMD (AVX / AVX-512) | ARM NEON SIMD | Wasm SIMD (`-msimd128`) | Notes |
|------------------|---------------------------|----------------|-------------------------|-------|
| [`src/containers/bitset.c`](../src/containers/bitset.c) | `#if CROARING_IS_X64`: Harley–Seal-style paths, bitwise macros | `#elif defined(CROARING_USENEON)` | `#elif defined(CROARING_WASM_SIMD)` | Bitset cardinality and `bitset_container_{or,and,xor,andnot,...}` variants. Wasm uses v128 + `wasm_i8x16_popcnt` / lane-wise popcount reductions. |
| [`src/array_util.c`](../src/array_util.c) | Many guarded regions | Scalar / compiler autovec | Scalar | Largest x86 SIMD area; NEON does not duplicate these paths today. Wasm parity with **x86** here is future work (v128 rewires). |
| [`src/bitset_util.c`](../src/bitset_util.c) | x86-only hotspots | Scalar | Scalar | Same as above. |
| [`src/containers/run.c`](../src/containers/run.c) | AVX-512 when enabled | Scalar | Scalar | Run-container helpers. |
| [`src/containers/array.c`](../src/containers/array.c) | Mixed x86 guards | Scalar | Scalar | Array container internals. |
| [`src/containers/convert.c`](../src/containers/convert.c) | x86 paths | Scalar | Scalar | Conversion / upgrade paths. |
| [`include/roaring/portability.h`](../include/roaring/portability.h) | x86 intrinsic includes | `#include <arm_neon.h>` when `CROARING_USENEON` | `#include <wasm_simd128.h>` when `__wasm_simd128__` defines `CROARING_WASM_SIMD` | Enables feature macros only. |

**Summary.** Wasm SIMD matches **Neon’s** intentional SIMD footprint (currently **only `bitset.c`**). Matching **Intel** SIMD across `array_util` / `bitset_util` etc. requires additional ports and broader tests.

## Verification already in-repo

| Check | Purpose |
|-------|---------|
| [`tools/run_wasm_differential_test.sh`](../tools/run_wasm_differential_test.sh) | Same deterministic workload: native gcc, wasm scalar, wasm `-msimd128`; digests must match. |
| Linked vs scalar SIMD opcode drift | Ensures SIMD build is not bitwise-identical stubs. |
| Amalgamation LLVM IR probe | Wasm builtins leave `@llvm.ctpop.v16i8` or legacy `@llvm.wasm.*` traces (Clang-dependent). |

## Optional local checks

| Script | Purpose |
|--------|---------|
| [`tools/check_wasm_simd_neon_pairing.sh`](../tools/check_wasm_simd_neon_pairing.sh) | Fails when Neon `#elif` branches outnumber Wasm SIMD `#elif` branches or pairwise ordering breaks. |
| [`tools/wasm_simd_perf_smoke.sh`](../tools/wasm_simd_perf_smoke.sh) (+ [`tools/_wasm_perf_bench_helper.py`](../tools/_wasm_perf_bench_helper.py)) | Repeated `node` runs of wasm scalar vs `-msimd128` harness; prints wall times and ratio (default informative). Env: `WASM_SIMD_PERF_ROUNDS`; optional gates `WASM_SIMD_PERF_REQUIRE_FASTER` or `WASM_SIMD_PERF_MAX_SLOWUP` for local strict checks. Needs `python3` and [`emcc`](https://emscripten.org/). Example with Docker Emscripten: `docker run --rm ... croaring/wasm-diff:local bash tools/wasm_simd_perf_smoke.sh` from a volume-mounted repo root (`-w`/`-v`; see [`tools/run-wasm-diff-docker.sh`](../tools/run-wasm-diff-docker.sh)). |

## When extending Wasm SIMD beyond `bitset.c`

1. Implement the new `#elif defined(CROARING_WASM_SIMD)` region.
2. Add this row/table entry and rationale if x86 differs from Neon.
3. Extend [`tests/wasm_diff_harness.c`](../tests/wasm_diff_harness.c) so the amalgamation differential test actually invokes the new routines (coverage + digest lock-in).
4. Re-run wasm differential + SIMD artifact/IR gates.
