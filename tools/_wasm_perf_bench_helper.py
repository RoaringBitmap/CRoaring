#!/usr/bin/env python3
"""
Called from wasm_simd_perf_smoke.sh — times repeated node executions of wasm harness JS.
"""

import subprocess
import sys
import time


def bench(node: str, rounds: int, js_path: str) -> float:
    t0 = time.perf_counter()
    for _ in range(rounds):
        subprocess.run([node, js_path], stdout=subprocess.DEVNULL, check=True)
    return time.perf_counter() - t0


def main(argv):  # noqa: ANN001
    if len(argv) < 7:
        print(
            "usage: _wasm_perf_bench_helper.py NODE ROUNDS scalar.js simd.js REQUIRE_FAST SLOWUP",
            file=sys.stderr,
        )
        return 2
    node = argv[1]
    rounds = int(argv[2])
    scalar_js = argv[3]
    simd_js = argv[4]
    require_faster = argv[5]
    slowup_cap = argv[6].strip()

    scalar_s = bench(node, rounds, scalar_js)
    simd_s = bench(node, rounds, simd_js)

    ratio = simd_s / scalar_s if scalar_s > 1e-9 else float("inf")
    print(
        "wasm SIMD perf smoke: rounds={}: scalar_sec={:.4f}, simd_sec={:.4f}, simd/scalar={:.4f}".format(
            rounds, scalar_s, simd_s, ratio
        )
    )

    ec = 0
    if require_faster.strip():
        if simd_s >= scalar_s:
            print(
                "wasm_simd_perf_smoke: SIMD not faster than scalar (WASM_SIMD_PERF_REQUIRE_FASTER).",
                file=sys.stderr,
            )
            ec = 1
    if slowup_cap and ec == 0:
        factor = float(slowup_cap)
        if factor > 1.0 + 1e-9 and simd_s > scalar_s * factor:
            print(
                "wasm_simd_perf_smoke: SIMD slower than {:.2f}x scalar (WASM_SIMD_PERF_MAX_SLOWUP).".format(
                    factor
                ),
                file=sys.stderr,
            )
            ec = 1
    return ec


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
