#!/usr/bin/env bash
# Called from .github/workflows/emscripten.yml: ensure wasm-objdump on PATH via
# wabt, then native vs wasm-scalar vs wasm-simd128 digest comparison.
#
# Expects EMSDK already set when run under setup-emsdk; extends PATH when needed.
#
# SIMD artifact / LLVM guards in tools/run_wasm_differential_test.sh are toolchain-
# heuristic; do not set WASM_DIFF_SKIP_SIMD_ARTIFACT_GUARD or
# WASM_DIFF_SKIP_LLVM_WASM_INTRINSIC_GUARD in CI (they are rejected when GITHUB_ACTIONS is set).

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if [[ -n "${EMSDK:-}" && -d "${EMSDK}/upstream/bin" ]]; then
  export PATH="${EMSDK}/upstream/bin:${PATH}"
fi

sudo apt-get update -qq
sudo apt-get install -y wabt >/dev/null

bash "$ROOT/tools/run_wasm_differential_test.sh"
