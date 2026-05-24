#!/usr/bin/env bash
# Optional wall-clock smoke: wasm scalar vs wasm -msimd128 running tests/wasm_diff_harness.c
# (same amalgamation build pipeline as tools/run_wasm_differential_test.sh).
# Use it to quantify SIMD benefit for paths like intersect_vector16 (correctness stays
# on the differential test; this script is only throughput signal, noise-prone).
#
# Requirements: bash, python3 (>= 3.6), emcc, amalgamation.sh artifacts.
#
# Env:
#   WASM_SIMD_PERF_ROUNDS (default 25) iterations per wasm build.
#   WASM_SIMD_PERF_REQUIRE_FASTER — if set nonempty, exit 1 when SIMD wall time >= scalar wall time.
#   WASM_SIMD_PERF_MAX_SLOWUP — optional float; exit 1 if simd_sec > scalar_sec * factor (noise-prone).
#   TEMP_DIR / TMPDIR passed through mktemp.
#
# Typical (informative):  bash tools/wasm_simd_perf_smoke.sh
# From repo root (requires Docker Emscripten image matching run-wasm-diff-docker):
#   docker run --rm --platform linux/amd64 -v \"$PWD:$PWD\":Z -w \"$PWD\" croaring/wasm-diff:local bash tools/wasm_simd_perf_smoke.sh
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/croaring-wasm-perf.XXXXXX")"
cleanup() { rm -rf "$WORK"; }
trap cleanup EXIT

ROUND="${WASM_SIMD_PERF_ROUNDS:-25}"
PY="${PYTHON3:-python3}"

if ! command -v "$PY" >/dev/null 2>&1; then
  echo "wasm_simd_perf_smoke.sh: need python3 (set PYTHON3)." >&2
  exit 2
fi

AMALG="$WORK/amalg"
mkdir -p "$AMALG"
bash "$ROOT/amalgamation.sh" "$AMALG" >/dev/null

HARNESS="$ROOT/tests/wasm_diff_harness.c"
ROARING_C="$AMALG/roaring.c"
SCALAR_JS="$WORK/wasm_perf_scalar.js"
SIMD_JS="$WORK/wasm_perf_simd.js"

"${EMCC:-emcc}" -std=c11 -O2 -DCROARING_AMALGAMATED=1 -I"$AMALG" "$ROARING_C" "$HARNESS" \
  -sALLOW_MEMORY_GROWTH=1 -o "$SCALAR_JS"

"${EMCC:-emcc}" -std=c11 -O2 -msimd128 -DCROARING_AMALGAMATED=1 -I"$AMALG" "$ROARING_C" "$HARNESS" \
  -sALLOW_MEMORY_GROWTH=1 -o "$SIMD_JS"

"${NODE:-node}" --version >/dev/null

"$PY" "$ROOT/tools/_wasm_perf_bench_helper.py" \
  "${NODE:-node}" "$ROUND" "$SCALAR_JS" "$SIMD_JS" \
  "${WASM_SIMD_PERF_REQUIRE_FASTER:-}" \
  "${WASM_SIMD_PERF_MAX_SLOWUP:-}"
