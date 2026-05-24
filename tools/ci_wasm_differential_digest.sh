#!/usr/bin/env bash
# Called from .github/workflows/emscripten.yml: ensure wasm-objdump on PATH via
# wabt, then native vs wasm-scalar vs wasm-simd128 digest comparison.
#
# Expects EMSDK already set when run under setup-emsdk; extends PATH when needed.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if [[ -n "${EMSDK:-}" && -d "${EMSDK}/upstream/bin" ]]; then
  export PATH="${EMSDK}/upstream/bin:${PATH}"
fi

sudo apt-get update -qq
sudo apt-get install -y wabt >/dev/null

bash "$ROOT/tools/run_wasm_differential_test.sh"
