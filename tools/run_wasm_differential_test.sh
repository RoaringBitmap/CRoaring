#!/usr/bin/env bash
# WASM differential test: same harness + amalgamated roaring.c built natively and
# with emcc (scalar wasm + -msimd128 wasm). Compares deterministic stdout digests.
#
# Requirements: bash, cc (or $CC), emcc (or $EMCC), node (or $NODE), wasm-objdump
#   (from WABT or $EMSDK/upstream/bin when using Emscripten).
#
# Optional env:
#   WASM_DIFF_SKIP_SIMD_ARTIFACT_GUARD=1 — skip wasm-objdump bytecode uplift checks below.
#   WASM_DIFF_SKIP_LLVM_WASM_INTRINSIC_GUARD=1 — skip IR scan for @llvm.wasm.* intrinsics.
#
# Mandatory (unless skipped per env):
#
# wasm-objdump artifact proof (still heuristic on opcodes): linked + roaring-only wasm objects
#   must show SIMD uplift for -msimd128 vs scalar, so digest legs aren’t bitwise-identical stubs.
#
# LLVM IR intrinsic proof (explicit hand-written SIMD builtins vs generic vectors):
#   Emit amalgamation roaring.ll with -emit-llvm -S -O2 -msimd128 and require ≥1 distinct
#   @llvm.wasm.* name. Clang maps headers like wasm_simd128.h builtins (e.g. wasm_i8x16_swizzle)
#   to those intrinsics across -O2; generic insertelement SIMD widening typically does not.
#
# Local dev (macOS/Linux): from repo root,
#   bash tools/run_wasm_differential_test.sh
#
# Docker (no local Emscripten/WABT): bash tools/run-wasm-diff-docker.sh

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

verify_wasm_simd_artifacts() {
  local scalar_link="$1"
  local simd_link="$2"
  local roaring_scalar_o="$3"
  local roaring_simd_o="$4"

  if [[ "${WASM_DIFF_SKIP_SIMD_ARTIFACT_GUARD:-0}" != "0" ]]; then
    echo "== SIMD artifact proof: skipped (WASM_DIFF_SKIP_SIMD_ARTIFACT_GUARD) ==" >&2
    return 0
  fi
  if [[ -z "$WASM_OBJDUMP" ]]; then
    echo "run_wasm_differential_test.sh: wasm-objdump required for SIMD artifact guard (install WABT or use Emscripten upstream/bin)." >&2
    exit 4
  fi

  local lk_s lk_d rl_s rl_d
  lk_s=$(count_simd_ops "$scalar_link")
  lk_d=$(count_simd_ops "$simd_link")
  rl_s=$(count_simd_ops "$roaring_scalar_o")
  rl_d=$(count_simd_ops "$roaring_simd_o")

  echo "SIMD-shaped opcode hits (wasm-objdump heuristic): linked.scalar=$lk_s linked.simd=$lk_d roaring.c-only.scalar=$rl_s roaring.c-only.simd=$rl_d"

  if [[ "$lk_d" -le "$lk_s" ]]; then
    echo "run_wasm_differential_test.sh: linked -msimd128 module does not expose more SIMD opcodes than scalar ($lk_d vs $lk_s); SIMD test leg may equal scalar codegen." >&2
    exit 5
  fi
  if [[ "$rl_d" -le "$rl_s" ]]; then
    echo "run_wasm_differential_test.sh: amalgamated roaring.c with -msimd128 shows no SIMD opcode uplift versus plain wasm object ($rl_d vs $rl_s)." >&2
    exit 5
  fi
  if [[ "$rl_s" -ne 0 ]]; then
    echo "run_wasm_differential_test.sh: plain wasm roaring.c object unexpectedly has SIMD-shaped opcodes (count=$rl_s); expected baseline 0." >&2
    exit 5
  fi
  echo "OK: SIMD artifacts differ from scalar (linked wasm + roaring.c wasm objects)."
}

count_distinct_llvm_wasm_intrinsic_names() {
  local ll="$1"
  if [[ ! -f "$ll" ]]; then
    echo "0"
    return 0
  fi
  grep -oE '@llvm\.wasm[a-zA-Z0-9._]*' "$ll" | sort -u | wc -l | tr -d ' '
}

verify_llvm_wasm_intrinsics_in_roaring_ir() {
  if [[ "${WASM_DIFF_SKIP_LLVM_WASM_INTRINSIC_GUARD:-0}" != "0" ]]; then
    echo "== LLVM @llvm.wasm.* intrinsic guard: skipped (WASM_DIFF_SKIP_LLVM_WASM_INTRINSIC_GUARD) ==" >&2
    return 0
  fi

  echo "== LLVM IR: @llvm.wasm.* intrinsics in amalgamation roaring.ll (-O2 -msimd128) =="
  local ll="$WORK/ro.ll"
  "$EMCC" -std=c11 -O2 -msimd128 -emit-llvm -S -DCROARING_AMALGAMATED=1 -I"$AMALG" "$ROARING_C" -o "$ll"

  local n
  n="$(count_distinct_llvm_wasm_intrinsic_names "$ll")"
  echo "distinct @llvm.wasm.* names in roaring IR: ${n:-0}"
  grep -oE '@llvm\.wasm[a-zA-Z0-9._]*' "$ll" 2>/dev/null | sort -u | head -n 32 || true

  if [[ "${n:-0}" -eq 0 ]]; then
    echo "run_wasm_differential_test.sh: amalgamation IR contains no @llvm.wasm.* intrinsics at O2." >&2
    echo "  Implement wasm SIMD with builtins that Clang lowers to @llvm.wasm.* (wasm_simd128.h / builtins)." >&2
    echo "  Generic SIMD vector IR alone is not sufficient for this gate." >&2
    exit 6
  fi
  echo "OK: roaring amalgamation uses explicit Wasm LLVM intrinsics (@llvm.wasm.*)."
}

echo "== Native ($CC) =="
"$CC" -std=c11 -O2 -Wall -Wextra -DCROARING_AMALGAMATED=1 -I"$AMALG" \
  "$ROARING_C" "$HARNESS" -o "$NATIVE_OUT"
"$NATIVE_OUT" >"$NATIVE_TXT"

echo "== WebAssembly scalar (emcc, no -msimd128) =="
"$EMCC" -std=c11 -O2 -DCROARING_AMALGAMATED=1 -I"$AMALG" \
  "$ROARING_C" "$HARNESS" \
  -sALLOW_MEMORY_GROWTH=1 \
  -o "$SCALAR_JS"
scalar_wasm_file="${SCALAR_JS%.js}.wasm"
if [[ ! -f "$scalar_wasm_file" ]]; then
  echo "run_wasm_differential_test.sh: expected $scalar_wasm_file" >&2
  exit 3
fi
"$NODE" "$SCALAR_JS" >"$SCALAR_TXT"

echo "== WebAssembly SIMD (emcc -msimd128) =="
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

echo "== Roaring.c wasm objects (isolate library SIMD uplift) =="
"$EMCC" -std=c11 -O2 -c -DCROARING_AMALGAMATED=1 -I"$AMALG" "$ROARING_C" -o "$WORK/roaring_lib.scalar.o"
"$EMCC" -std=c11 -O2 -msimd128 -c -DCROARING_AMALGAMATED=1 -I"$AMALG" "$ROARING_C" -o "$WORK/roaring_lib.simd.o"
verify_wasm_simd_artifacts "$scalar_wasm_file" "$simd_wasm_file" "$WORK/roaring_lib.scalar.o" "$WORK/roaring_lib.simd.o"
verify_llvm_wasm_intrinsics_in_roaring_ir

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
