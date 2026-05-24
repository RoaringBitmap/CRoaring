#!/usr/bin/env bash
# Cheap structural heuristic: in each src/**/*.c file that uses Neon, require
# the same count of '#elif defined(CROARING_USENEON)' and
# '#elif defined(CROARING_WASM_SIMD)', and pair them by ascending line numbers
# (each WASM line must be strictly above its paired NEON line).
#
# This does NOT parse preprocessor nesting. It cannot prove two branches belong
# to the same #if/#elif chain or implement the same algorithm. Passing here does
# not imply semantic NEON/WASM parity — use code review plus
# tools/run_wasm_differential_test.sh digest parity for that.
#
# Example: bitset_container_to_uint32_array (bitset.c) may use WASM SIMD fast
# path while the NEON #elif deliberately calls the scalar extractor; the script
# can still pass whenever counts and ordering match.
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
        grep -q '#elif defined(CROARING_USENEON)' "$f" && printf '%s\0' "$f"
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
  while IFS= read -r line; do
    neon_a+=("$line")
  done < <(grep -n '#elif defined(CROARING_USENEON)' "$file" | cut -d: -f1 | sort -n || true)
  while IFS= read -r line; do
    wasm_a+=("$line")
  done < <(grep -n '#elif defined(CROARING_WASM_SIMD)' "$file" | cut -d: -f1 | sort -n || true)

  local nneon=${#neon_a[@]}
  local nwasm=${#wasm_a[@]}

  if (( nneon == 0 )); then
    return 0
  fi
  if (( nwasm != nneon )); then
    echo "check_wasm_simd_neon_pairing.sh: mismatch in $file: #elif NEON count=${nneon} vs #elif WASM_SIMD count=${nwasm}. Add matching CROARING_WASM_SIMD chains." >&2
    FAIL=1
    return 0
  fi
  local i
  for ((i = 0; i < nneon; i++)); do
    if (( wasm_a[i] >= neon_a[i] )); then
      echo "check_wasm_simd_neon_pairing.sh: WASM SIMD must precede NEON at pair index $((i + 1)) in $file (wasm line ${wasm_a[i]}, neon line ${neon_a[i]})." >&2
      FAIL=1
    fi
  done
}

if [[ ${#uniq_files[@]} -eq 0 ]]; then
  echo "check_wasm_simd_neon_pairing.sh: no #elif NEON in src/**/*.c — expected at least bitset.c." >&2
  exit 1
fi

for f in "${uniq_files[@]}"; do
  check_file_pairing "$f"
done

if [[ "$FAIL" -ne 0 ]]; then
  exit 1
fi
echo "OK: structural NEON / WASM SIMD #elif pairing check passed (${#uniq_files[@]} file(s))."
