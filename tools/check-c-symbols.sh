#!/usr/bin/env bash
# Check that a C++ library defines every global function in the C library.
# Requires GNU nm. Exits 1 for missing symbols and 2 for invalid input.
set -euo pipefail

if [[ $# != 2 ]]; then
    echo "Usage: $0 <C library> <C++ library>" >&2
    exit 2
fi

scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT

function_symbols() {
    local options=(-g --defined-only --format=posix)
    case "$1" in
        *.so|*.so.*) options+=(-D) ;;
    esac
    # Compare names, allowing strong, weak and indirect function definitions.
    # Data symbols and C++ implementation details need not match the C build.
    nm "${options[@]}" "$1" |
        awk '$2 ~ /^[TWiI]$/ { print $1 }' | LC_ALL=C sort -u
}

if ! function_symbols "$1" > "$scratch/c" ||
   ! function_symbols "$2" > "$scratch/cpp"; then
    echo "Could not read library symbols." >&2
    exit 2
fi
if [[ ! -s "$scratch/c" ]]; then
    echo "No global C functions found in $1" >&2
    exit 2
fi
LC_ALL=C comm -23 "$scratch/c" "$scratch/cpp" > "$scratch/missing"
if [[ -s "$scratch/missing" ]]; then
    echo "C functions missing from the C++ library:" >&2
    cat "$scratch/missing" >&2
    exit 1
fi
echo "All $(wc -l < "$scratch/c" | tr -d ' ') C functions are defined in the C++ library."
