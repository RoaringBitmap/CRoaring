#!/usr/bin/env bash
# WASM differential test: same harness + amalgamated roaring.c built natively and
# with emcc (scalar wasm + -msimd128 wasm). Compares deterministic stdout digests.
#
# Requirements: bash, cc (or $CC), emcc (or $EMCC), node (or $NODE), wasm-objdump
#   (from WABT or $EMSDK/upstream/bin when using Emscripten).
#
# Optional env:
#   WASM_DIFF_REQUIRE_SIMD_OPS=1 — fail if the -msimd128 linked .wasm has zero
#     SIMD opcodes (enable once CROARING_IS_WASM intrinsics are merged; default 0).
#   CC, EMCC, NODE — compiler paths.
#
# Local dev (macOS/Linux): from repo root,
#   bash tools/run_wasm_differential_test.sh

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CC="${CC:-cc}"
EMCC="${EMCC:-emcc}"
NODE="${NODE:-node}"

if ! command -v "$EMCC" >/dev/null 2>&1; then
  echo "run_wasm_differential_test.sh: emcc not found (set EMCC or install Emscripten)." >&2
  exit 2
fi
if ! command -v "$NODE" >/dev/null 2>&1; then
  echo "run_wasm_differential_test.sh: node not found (set NODE)." >&2
  exit 2
fi

WORK="$(mktemp -d "${TMPDIR:-/tmp}/croaring-wasm-diff.XXXXXX")"
cleanup() { rm -rf "$WORK"; }
trap cleanup EXIT

AMALG="$WORK/amalg"
mkdir -p "$AMALG"
bash "$ROOT/amalgamation.sh" "$AMALG"

HARNESS="$ROOT/tests/wasm_diff_harness.c"
ROARING_C="$AMALG/roaring.c"
NATIVE_OUT="$WORK/wasm_diff_harness.native"
SCALAR_JS="$WORK/wasm_diff_scalar.js"
SIMD_JS="$WORK/wasm_diff_simd.js"
NATIVE_TXT="$WORK/native.txt"
SCALAR_TXT="$WORK/wasm_scalar.txt"
SIMD_TXT="$WORK/wasm_simd.txt"

resolve_wasm_objdump() {
  if command -v wasm-objdump >/dev/null 2>&1; then
    command -v wasm-objdump
    return
  fi
  if [[ -n "${EMSDK:-}" && -x "${EMSDK}/upstream/bin/wasm-objdump" ]]; then
    echo "${EMSDK}/upstream/bin/wasm-objdump"
    return
  fi
  echo ""
}

WASM_OBJDUMP="$(resolve_wasm_objdump)"

echo "== Native ($CC) =="
"$CC" -std=c11 -O2 -Wall -Wextra -DCROARING_AMALGAMATED=1 -I"$AMALG" \
  "$ROARING_C" "$HARNESS" -o "$NATIVE_OUT"
"$NATIVE_OUT" >"$NATIVE_TXT"

echo "== WebAssembly scalar (emcc, no -msimd128) =="
"$EMCC" -std=c11 -O2 -DCROARING_AMALGAMATED=1 -I"$AMALG" \
  "$ROARING_C" "$HARNESS" \
  -sALLOW_MEMORY_GROWTH=1 \
  -o "$SCALAR_JS"
# emcc -o foo.js writes foo.wasm next to foo.js
scalar_wasm_file="${SCALAR_JS%.js}.wasm"
if [[ ! -f "$scalar_wasm_file" ]]; then
  echo "run_wasm_differential_test.sh: expected $scalar_wasm_file" >&2
  exit 3
fi
"$NODE" "$SCALAR_JS" >"$SCALAR_TXT"

echo "== WebAssembly SIMD (emcc -msimd128) =="
# Preprocessor: __wasm_simd128__ must be defined with -msimd128
cat >"$WORK/simd_preproc.c" <<'EOF'
#if !defined(__wasm_simd128__)
#error "expected __wasm_simd128__ when compiling with -msimd128"
#endif
int main(void) { return 0; }
EOF
"$EMCC" -msimd128 -c "$WORK/simd_preproc.c" -o "$WORK/simd_preproc.o"

"$EMCC" -std=c11 -O2 -msimd128 -DCROARING_AMALGAMATED=1 -I"$AMALG" \
  "$ROARING_C" "$HARNESS" \
  -sALLOW_MEMORY_GROWTH=1 \
  -o "$SIMD_JS"
simd_wasm_file="${SIMD_JS%.js}.wasm"
if [[ ! -f "$simd_wasm_file" ]]; then
  echo "run_wasm_differential_test.sh: expected $simd_wasm_file" >&2
  exit 3
fi
"$NODE" "$SIMD_JS" >"$SIMD_TXT"

count_simd_ops() {
  local wasm="$1"
  if [[ -z "$WASM_OBJDUMP" ]]; then
    echo "0"
    return
  fi
  local n
  n=$("$WASM_OBJDUMP" -d "$wasm" 2>/dev/null | grep -oE 'v128\.[a-z0-9._]+|[fi][0-9]+x[0-9]+\.[a-z0-9._]+' | wc -l | tr -d ' ')
  echo "${n:-0}"
}

scalar_ops="$(count_simd_ops "$scalar_wasm_file")"
simd_ops="$(count_simd_ops "$simd_wasm_file")"
echo "wasm-objdump SIMD opcode matches (scalar wasm): $scalar_ops"
echo "wasm-objdump SIMD opcode matches ( simd wasm): $simd_ops"
if [[ -z "$WASM_OBJDUMP" ]]; then
  echo "warning: wasm-objdump not found; skipping SIMD opcode counts (install WABT or use Emscripten's upstream/bin)." >&2
fi

if [[ "${WASM_DIFF_REQUIRE_SIMD_OPS:-0}" == "1" ]]; then
  if [[ -z "$WASM_OBJDUMP" ]]; then
    echo "WASM_DIFF_REQUIRE_SIMD_OPS=1 but wasm-objdump not available." >&2
    exit 4
  fi
  if [[ "$simd_ops" -eq 0 ]]; then
    echo "SIMD wasm linked module has zero SIMD opcodes; expected intrinsics after WASM SIMD backend." >&2
    exit 4
  fi
fi

compare_digests() {
  local a="$1"
  local b="$2"
  local label="$3"
  if cmp -s "$a" "$b"; then
    return 0
  fi
  echo "run_wasm_differential_test.sh: digest mismatch: $label" >&2
  echo "diff -u (first 80 lines):" >&2
  diff -u "$a" "$b" | head -n 80 >&2 || true
  exit 1
}

echo "== Compare digests =="
compare_digests "$NATIVE_TXT" "$SCALAR_TXT" "native vs wasm scalar"
compare_digests "$NATIVE_TXT" "$SIMD_TXT" "native vs wasm -msimd128"
compare_digests "$SCALAR_TXT" "$SIMD_TXT" "wasm scalar vs wasm -msimd128"
echo "OK: native, wasm scalar, and wasm -msimd128 digests match."
