#!/usr/bin/env python3
import re
import csv
from pathlib import Path

HERE = Path(__file__).resolve().parent

# Mapping from platform to its result log files.
#
# We now support three execution modes per platform:
#   - "none"   -> WASM            (Results_*_Wasm)
#   - "opt"    -> WASM optimized (Results_*_Wasm_Opt)
#   - "zephyr" -> native Zephyr   (Results_*_Zepyr)
#
# The file names below must match the Results_* files present in this
# directory.
PLATFORM_LOGS = {
    "qemu_cortex_a53": {
        "none": "Results_Qemu_Wasm",
        "opt": "Results_Qemu_Wasm_Opt",
        "zephyr": "Results_Qemu_Zepyr",
    },
    "max32690": {
        "none": "Results_Max32_Wasm",
        "opt": "Results_Max32_Wasm_Opt",
        "zephyr": "Results_Max32_Zepyr",
    },
    "riscv32": {
        "none": "Results_Riscv_Wasm",
        "opt": "Results_Riscv_Wasm_Opt",
        "zephyr": "Results_Riscv_Zepyr",
    },
}

# CSV column order must match benchmark_results.csv
CSV_HEADER = [
    "platform",
    "opt_level",
    "N",
    "kernel",
    "ticks",
    "res",
    "m",
    "k",
    "n",
    "H",
    "W",
    "kH",
    "kW",
    "len",
]

# Regexes for parsing the logs
RE_CASE = re.compile(r"^=== Benchmark case: N=(\d+) ===")
# e.g. "[vec_add][scalar] Elapsed ticks: 181 (m=2,k=2,n=2)"
# or   "[vec_add] Elapsed ticks: 22 (len=4)"
RE_LINE = re.compile(
    r"^\[(?P<kernel>[^\]]+)\]"  # [kernel]
    r"(?:\[(?P<tag>[^\]]+)\])?"  # optional [scalar]/[opt]
    r"\s+Elapsed ticks: (?P<ticks>\d+)"  # ticks
    r"(?:\s*\((?P<meta>[^)]*)\))?"      # optional "(m=...,k=...,...)"
)


def parse_meta(meta_str):
    """Parse the tail "(m=...,k=...,...)" section into a dict.

    Only keys relevant to benchmark_results.csv are extracted.
    """

    result = {"res": None, "m": None, "k": None, "n": None,
              "H": None, "W": None, "kH": None, "kW": None,
              "len": None}
    if not meta_str:
        return result

    for part in meta_str.split(","):
        part = part.strip()
        if "=" not in part:
            continue
        key, value = part.split("=", 1)
        key = key.strip()
        value = value.strip()
        if key in {"m", "k", "n", "H", "W", "kH", "kW", "len"}:
            try:
                result[key] = int(value)
            except ValueError:
                # Leave as None if not an int
                pass
        elif key == "res":
            try:
                result["res"] = float(value)
            except ValueError:
                pass

    return result


def parse_log_file(path: Path, platform: str, default_opt: str):
    """Parse a single Results_* log file and return list of rows.

    default_opt is the opt_level implied by the file name (e.g. "opt" for
    Results_Riscv_Opt). Within the file, lines that explicitly contain
    [scalar] or [opt] override this default (useful for MAX logs that
    embed both scalar and opt in one file).
    """

    rows = []
    if not path.is_file():
        return rows

    current_N = None
    with path.open("r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue

            m_case = RE_CASE.match(line)
            if m_case:
                current_N = int(m_case.group(1))
                continue

            m_line = RE_LINE.match(line)
            if not m_line or current_N is None:
                # Ignore unrelated lines (boot banners, etc.)
                continue

            kernel = m_line.group("kernel").strip()
            tag = m_line.group("tag")
            ticks = int(m_line.group("ticks"))
            meta = parse_meta(m_line.group("meta"))

            # Determine opt_level: explicit tag wins, otherwise file default.
            if tag == "scalar":
                opt_level = "none"
            elif tag == "opt":
                opt_level = "opt"
            else:
                opt_level = default_opt

            row = {
                "platform": platform,
                "opt_level": opt_level,
                "N": current_N,
                "kernel": kernel,
                "ticks": ticks,
                "res": meta["res"],
                "m": meta["m"],
                "k": meta["k"],
                "n": meta["n"],
                "H": meta["H"],
                "W": meta["W"],
                "kH": meta["kH"],
                "kW": meta["kW"],
                "len": meta["len"],
            }
            rows.append(row)

    return rows


def main():
    all_rows = []

    for platform, logs in PLATFORM_LOGS.items():
        for opt_level, fname in logs.items():
            path = HERE / fname
            rows = parse_log_file(path, platform=platform, default_opt=opt_level)
            all_rows.extend(rows)

    # Sort rows for stable, readable CSV
    all_rows.sort(key=lambda r: (r["platform"], r["opt_level"], r["N"], r["kernel"]))

    out_path = HERE / "benchmark_results.csv"
    with out_path.open("w", newline="") as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=CSV_HEADER)
        writer.writeheader()
        for r in all_rows:
            # Convert None to empty strings for CSV compatibility
            row = {k: ("" if r[k] is None else r[k]) for k in CSV_HEADER}
            writer.writerow(row)

    print(f"Wrote {len(all_rows)} rows to {out_path}")


if __name__ == "__main__":
    main()
