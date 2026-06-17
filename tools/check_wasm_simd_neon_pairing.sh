#!/usr/bin/env bash
#
# NEON vs WebAssembly SIMD128 — structural preprocessor pairing heuristic
#
# Many portable SIMD forks use chains like:
#   #if CROARING_IS_X64 ... #elif defined(CROARING_WASM_SIMD) ... #elif defined(CROARING_USENEON)
#
# Goal: discourage "orphaned" blocks where Wasm gets a specialized #elif but Neon has
# fewer corresponding #elif branches in the same file (risk of drifting ARM-only regressions).
# This script does NOT parse #if nesting; it only compares line-number lists of directives.
#
# Rules (cheap grep over src/**/*.c; cannot prove semantic parity):
#
# - Files that only use #elif CROARING_USENEON (no CROARING_WASM_SIMD lines): PASS.
#   Neon-only optimizations are OK without dummy Wasm branches.
# - Files that only use #elif CROARING_WASM_SIMD (no NEON): PASS.
#   Wasm-only SIMD paths need no NEON twin in that file (e.g. array-only kernels).
# - Files that use BOTH directives: PASS only if NEON #elif count >= Wasm #elif count, and each
#   Wasm line can be paired with a strictly later NEON line (greedy: enforce Wasm-before-NEON order
#   for overlapping SIMD paths while allowing trailing Neon-only code).
#
# CI: `.github/workflows/emscripten.yml` runs this script. Behavioral parity is
# covered separately by `tools/run_wasm_differential_test.sh` (digest vs native/gcc).
#
# Usage:
#   bash tools/check_wasm_simd_neon_pairing.sh [--help|-h]
# Exit 0 OK, 1 on violation.

set -euo pipefail

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    cat <<'EOF'
Structural NEON vs WebAssembly SIMD pairing check for src/**/*.c.

Counts #elif defined(CROARING_USENEON) vs #elif defined(CROARING_WASM_SIMD).
Flags files where both macros appear but Neon has fewer #elif branches than Wasm,
or a Wasm branch has no Neon #elif strictly below it in source order — helps catch
orphaned Wasm-only blocks when ARM Neon should mirror the fork.

Does not parse #if nesting. Does not imply algorithmic NEON≡WASM parity; use review
plus tools/run_wasm_differential_test.sh for behavior.

Runs in CI: .github/workflows/emscripten.yml.

Usage:
  bash tools/check_wasm_simd_neon_pairing.sh
EOF
    exit 0
fi

if [[ -n "${1:-}" ]]; then
    echo "check_wasm_simd_neon_pairing.sh: unexpected argument \"$1\"; use --help" >&2
    exit 1
fi

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FAIL=0

declare -a FOUND=()
while IFS= read -r -d '' f; do
  FOUND+=("$f")
done < <(
  find "$ROOT/src" -name '*.c' -type f -print0 \
    | while IFS= read -r -d '' f; do
        grep -qE '#elif defined\(CROARING_USENEON\)|#elif defined\(CROARING_WASM_SIMD\)' "$f" && printf '%s\0' "$f"
      done
)

uniq_seen=''
declare -a uniq_files=()
for f in "${FOUND[@]}"; do
  case "|$uniq_seen|" in *"|$f|"*) continue ;; esac
  uniq_files+=("$f")
  uniq_seen="${uniq_seen}|$f"
done

check_file_pairing() {
  local file="$1"
  local -a neon_a wasm_a
  neon_a=()
  wasm_a=()
  local line

  local has_neon=false
  local has_wasm=false
  if grep -q '#elif defined(CROARING_USENEON)' "$file"; then has_neon=true; fi
  if grep -q '#elif defined(CROARING_WASM_SIMD)' "$file"; then has_wasm=true; fi

  [[ "$has_neon" == false ]] && [[ "$has_wasm" == false ]] && return 0

  while IFS= read -r line; do
    neon_a+=("$line")
  done < <(grep -n '#elif defined(CROARING_USENEON)' "$file" | cut -d: -f1 | sort -n || true)
  while IFS= read -r line; do
    wasm_a+=("$line")
  done < <(grep -n '#elif defined(CROARING_WASM_SIMD)' "$file" | cut -d: -f1 | sort -n || true)

  local nneon=${#neon_a[@]}
  local nwasm=${#wasm_a[@]}

  [[ "$nwasm" -eq 0 ]] && return 0
  [[ "$nneon" -eq 0 ]] && return 0

  if (( nwasm > nneon )); then
    echo "check_wasm_simd_neon_pairing.sh: in $file, #elif WASM_SIMD (${nwasm}) exceeds #elif NEON (${nneon}). Add Neon branches where Wasm paths exist." >&2
    FAIL=1
    return 0
  fi

  local j i
  j=0
  for ((i = 0; i < nwasm; i++)); do
    while (( j < nneon && neon_a[j] <= wasm_a[i] )); do
      ((++j))
    done
    if (( j >= nneon )); then
      echo "check_wasm_simd_neon_pairing.sh: in $file, #elif WASM_SIMD at line ${wasm_a[i]} has no strictly later #elif NEON to pair with." >&2
      FAIL=1
      return 0
    fi
    ((++j))
  done
}

if [[ ${#uniq_files[@]} -eq 0 ]]; then
  echo "check_wasm_simd_neon_pairing.sh: no #elif CROARING_USENEON or CROARING_WASM_SIMD under src/**/*.c — expected SIMD paths (e.g. bitset)." >&2
  exit 1
fi

for f in "${uniq_files[@]}"; do
  check_file_pairing "$f"
done

if [[ "$FAIL" -ne 0 ]]; then
  exit 1
fi

echo "OK: structural NEON / WASM SIMD heuristic passed (${#uniq_files[@]} file(s))."
