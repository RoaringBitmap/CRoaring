#!/usr/bin/env bash
# CI entry point for the wasm differential digest: ensure wasm-objdump on PATH
# via wabt, then native vs wasm-scalar vs wasm-simd128 digest comparison.
#
# NOTE: this PR adds the harness tooling but does not yet invoke this script from
# .github/workflows/emscripten.yml. The differential-digest CI step is enabled in
# the first wasm SIMD PR, because the SIMD-uplift guard in
# run_wasm_differential_test.sh requires wasm SIMD code to be present to assert
# against. Until then, run it locally (or via the `wasm_differential_test`
# CMake target).
#
# Expects EMSDK already set when run under setup-emsdk; extends PATH when needed.
#
# The SIMD artifact guard in tools/run_wasm_differential_test.sh is toolchain-
# heuristic; do not set WASM_DIFF_SKIP_SIMD_ARTIFACT_GUARD in CI (it is rejected
# when GITHUB_ACTIONS is set).

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if [[ -n "${EMSDK:-}" && -d "${EMSDK}/upstream/bin" ]]; then
  export PATH="${EMSDK}/upstream/bin:${PATH}"
fi

sudo apt-get update -qq
sudo apt-get install -y wabt >/dev/null

bash "$ROOT/tools/run_wasm_differential_test.sh"
