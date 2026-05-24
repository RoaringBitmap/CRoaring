# WebAssembly SIMD coverage and parity matrix

CRoaring uses SIMD on hot paths differently per architecture. This doc is the **parity matrix** kept alongside Wasm SIMD ports.

## Neon vs Wasm128 rule of thumb

**Every `#elif defined(CROARING_USENEON)` in `src/**/*.c` keeps a sibling `#elif defined(CROARING_WASM_SIMD)` in the same preprocessor chain** when both appear in a file — unless deliberately documented as omitted below.

CI enforces pairwise counts and ordering with [`tools/check_wasm_simd_neon_pairing.sh`](../tools/check_wasm_simd_neon_pairing.sh) (Wasm line must strictly precede the paired Neon line in each ordinal pair).

**Note:** Containers that only gained Wasm SIMD (`array.c`, `run.c`, `convert.c`) do not duplicate Neon SIMD today — those files use scalar Neon; the parity script applies to files like `bitset.c` that contain both Neon and Wasm branches.

### Deliberate scalar Neon for paired Wasm

 [`src/containers/bitset.c`](../src/containers/bitset.c): `bitset_container_to_uint32_array` adds `#elif defined(CROARING_WASM_SIMD)` for `bitset_extract_setbits_wasm_simd`; the paired `#elif defined(CROARING_USENEON)` calls scalar `bitset_extract_setbits` (no AArch64 SIMD table decode port yet).

## SIMD surface by area

| Translation unit | x86 SIMD (AVX / AVX-512) | ARM NEON SIMD | Wasm SIMD (`-msimd128`) | Notes |
|------------------|---------------------------|----------------|--------------------------|-------|
| [`src/containers/bitset.c`](../src/containers/bitset.c) | AVX2 / AVX-512 bitwise + extract thresholds | SIMD popcount paths | `#elif defined(CROARING_WASM_SIMD)` bitwise + `wasm_i8x16_popcnt` reductions | SIMD `to_uint32_array` when cardinality ≥ 8192 via `bitset_extract_setbits_wasm_simd`; Neon remains scalar extract. |
| [`src/array_util.c`](../src/array_util.c) | SSE PCMPESTRM-style intersect / widen | Scalar | `intersect_vector16*`, XOR/union merges, **`memequals`** 16‑byte loop, **`wasm_array_container_to_uint32_array`** (zero‑extend + add base) | `shuffle_mask16` shared guard: `CROARING_IS_X64 \|\| CROARING_WASM_SIMD`. pcmpequal_bitmask rebuilt with `wasm_v128_any_true` + lane spill (no variable `extract_lane`). |
| [`src/bitset_util.c`](../src/bitset_util.c) | AVX2 extract / decode tables | Scalar | **`bitset_extract_setbits_wasm_simd`** / **`bitset_extract_setbits_wasm_uint16`** (+ table init under same guard as x64) | v128 loads from `vecDecodeTable` rows. |
| [`src/containers/run.c`](../src/containers/run.c) | AVX-512 helpers when enabled | Scalar | Run cardinality accumulate; run → `uint32` batch export | `wasm_u32x4_shr` + `wasm_i32x4_add` patterns. |
| [`src/containers/array.c`](../src/containers/array.c) | AVX2 array intersect / widen | Scalar | XOR, intersect (+ grow slack), cardinality, inplace intersect, **`array_container_to_uint32_array`** | Intersection slack uses `16 / sizeof(uint16_t)` lanes (no `__m128i` under Wasm). |
| [`src/containers/convert.c`](../src/containers/convert.c) | x86 `array_container_from_bitset` SIMD | Scalar | **`array_container_from_bitset`** uses `bitset_extract_setbits_wasm_uint16` | |
| [`include/roaring/portability.h`](../include/roaring/portability.h) | x86 intrinsic includes | `#include <arm_neon.h>` when `CROARING_USENEON` | `#include <wasm_simd128.h>` when SIMD128 defines **`CROARING_WASM_SIMD`** | Enables feature macros only. |

## Incremental Wasm SIMD milestones (M1–M5)

| Milestone | Scope | Harness / verification |
|-----------|-------|-------------------------|
| **M1 — memequals parity** | `memequals` wasm 16‑byte SIMD loop in [`array_util.c`](../src/array_util.c); equality paths exercised via **`portable_roundtrip`** + `roaring_bitmap_equals` digests in [`tests/wasm_diff_harness.c`](../tests/wasm_diff_harness.c) | ✅ `tools/run_wasm_differential_test.sh` |
| **M2 — sorted uint16 vector merge** | `intersect_vector16*`, XOR/union `store_unique*` helpers (`wasm_i8x16_narrow_i16x8` bitmask, lane spill for equal‑any bitmask) | Same harness workload (dense array / bitset merges) |
| **M3 — array widen / export** | `wasm_array_container_to_uint32_array`; `array_container_to_uint32_array` wiring | ✅ `export_*_u32_fold` digests |
| **M4 — bitset_util decode** | `lengthTable` / `vecDecodeTable*` wasm init + `bitset_extract_setbits_wasm_*` | Bitset-heavy ranges + export digests |
| **M5 — container glue** | `array.c`, `run.c`, `convert.c`, `bitset_container_to_uint32_array` simd thresholds | Combined harness |

**Doc-matrix (this file):** keep the **SIMD surface** table aligned with `#elif defined(CROARING_WASM_SIMD)` regions when adding ports.

## Verification already in-repo

| Check | Purpose |
|-------|---------|
| [`tools/run_wasm_differential_test.sh`](../tools/run_wasm_differential_test.sh) | Same deterministic workload: native gcc, wasm scalar, wasm `-msimd128`; stdout digests must match. Harness covers serialize round-trip, cardinality, merge ops, **`roaring_bitmap_to_uint32_array`** fold (`export_*_u32_fold`). |
| Linked vs scalar SIMD opcode drift | Ensures SIMD build is not bitwise-identical stubs. |
| Amalgamation LLVM IR probe | Wasm builtins leave `@llvm.ctpop.v16i8` and/or `@llvm.wasm.*` traces (Clang-dependent). |
| [`tools/check_wasm_simd_neon_pairing.sh`](../tools/check_wasm_simd_neon_pairing.sh) | NEON / Wasm `#elif` pairing where both exist. |

## Optional local checks

| Script | Purpose |
|--------|---------|
| [`tools/wasm_simd_perf_smoke.sh`](../tools/wasm_simd_perf_smoke.sh) (+ [`tools/_wasm_perf_bench_helper.py`](../tools/_wasm_perf_bench_helper.py)) | Repeated `node` wasm scalar vs `-msimd128` harness timings. Needs `python3` and [`emcc`](https://emscripten.org/). Example: `docker run ... bash tools/wasm_simd_perf_smoke.sh` with [`tools/run-wasm-diff-docker.sh`](../tools/run-wasm-diff-docker.sh)-style mounts. |

## When extending Wasm SIMD further

1. Add or extend `#elif defined(CROARING_WASM_SIMD)` in the relevant `src/**/*.c`.
2. If the file already has Neon `#elif` chains, satisfy [`tools/check_wasm_simd_neon_pairing.sh`](../tools/check_wasm_simd_neon_pairing.sh) (or document a deliberate scalar Neon sibling like `bitset_container_to_uint32_array`).
3. Update this parity matrix row and rationale if behavior differs across x86 / Neon / Wasm.
4. Extend [`tests/wasm_diff_harness.c`](../tests/wasm_diff_harness.c) so the amalgamation differential test invokes the new code path.
5. Re-run wasm differential + SIMD artifact/IR gates (`bash tools/run_wasm_differential_test.sh` or Docker wrapper).
