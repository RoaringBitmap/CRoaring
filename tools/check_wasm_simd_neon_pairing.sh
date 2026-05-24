#!/usr/bin/env bash
# Cheap structural heuristic for src/**/*.c that use Wasm SIMD128 and/or Neon SIMD.
#
# Rules (does NOT parse preprocessor nesting; cannot prove semantic parity):
#
# - Files that only have #elif CROARING_USENEON (no CROARING_WASM_SIMD lines): PASS.
#   Extra Neon-only optimizations are allowed without dummy Wasm branches.
# - Files that only have #elif CROARING_WASM_SIMD (no NEON): PASS.
# - Files that declare BOTH: fail if Wasm #elif blocks outnumber NEON (each Wasm path
#   should have Neon consideration downstream). Accept extra Neon-only branches.
#
# Greedy pairing: each Wasm line ascends paired with some unused NEON strictly below
# the next Wasm (smallest NEON strictly after this Wasm consumes that branch). This
# enforces Wasm-before-NEON order for overlapping paths while allowing trailing Neon-only.

# Passing here does not imply NEON/WASM algorithmic equivalence — use review plus
# tools/run_wasm_differential_test.sh digest parity for semantics.
#
# Usage: bash tools/check_wasm_simd_neon_pairing.sh
# Exit 0 OK, 1 on violation.

set -euo pipefail

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
