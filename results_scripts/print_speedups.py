#!/usr/bin/env python3

import argparse
import csv
from pathlib import Path

HERE = Path(__file__).resolve().parent
CSV_PATH = HERE / "benchmark_results.csv"


def collect_speedups(N, platform_filter=None):
    """Collect (platform, kernel, wasm, opt, ratio) for given N.

    - wasm = opt_level "none" (pure WASM)
    - opt  = opt_level "opt" (HW-optimized WASM)
    ratio = wasm / opt
    """

    data = {}  # (platform, kernel, opt_level) -> ticks
    platforms = set()

    with CSV_PATH.open(newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
                n_val = int(row["N"])
            except (KeyError, ValueError):
                continue
            if n_val != N:
                continue

            platform = row["platform"]
            if platform_filter and platform != platform_filter:
                continue

            opt_level = row["opt_level"]
            kernel = row["kernel"]
            try:
                ticks = int(row["ticks"])
            except (KeyError, ValueError):
                continue

            platforms.add(platform)
            data[(platform, kernel, opt_level)] = ticks

    # Build results
    results = []  # (platform, kernel, wasm, opt, ratio)
    for platform in sorted(platforms):
        kernels = sorted({k for (p, k, _) in data.keys() if p == platform})
        for kernel in kernels:
            wasm = data.get((platform, kernel, "none"))
            opt = data.get((platform, kernel, "opt"))
            if wasm is None or opt is None or opt == 0:
                ratio = None
            else:
                ratio = wasm / opt
            results.append((platform, kernel, wasm, opt, ratio))

    return results


def main():
    parser = argparse.ArgumentParser(description="Print WASM vs HW-Opt cycles and ratios from benchmark_results.csv")
    parser.add_argument("--N", type=int, default=1024, help="Problem size N to filter (default: 1024)")
    parser.add_argument("--platform", help="Optional platform name (e.g., max32690); if omitted, show all platforms")
    args = parser.parse_args()

    results = collect_speedups(args.N, args.platform)
    if not results:
        print(f"No entries found for N={args.N}" + (f" and platform={args.platform}" if args.platform else ""))
        return

    current_platform = None
    for platform, kernel, wasm, opt, ratio in results:
        if platform != current_platform:
            current_platform = platform
            print(f"\nPlatform: {platform}, N={args.N}")
        wasm_s = f"{wasm}" if wasm is not None else "-"
        opt_s = f"{opt}" if opt is not None else "-"
        ratio_s = f"{ratio:.3f}" if ratio is not None else "-"
        print(f"  {kernel:13s}  wasm={wasm_s:8s}  opt={opt_s:8s}  ratio={ratio_s}")


if __name__ == "__main__":
    main()
