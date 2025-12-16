import pandas as pd
from pathlib import Path
import argparse

HERE = Path(__file__).resolve().parent
CSV_PATH = HERE / "benchmark_results.csv"


def fmt_int(n):
    return f"{int(n):,}".replace(",", "\\,")


def generate_table(df, platform, N):
    # If N is provided, produce one-row-per-kernel for that N. If N is None
    # and all_ns=True, caller should build rows for all N values per kernel.
    raise RuntimeError("Use generate_table_oneN or generate_table_allNs")


def generate_table_oneN(df, platform, N):
    # Filter and pivot for a single N
    g = df[(df["platform"] == platform) & (df["N"] == N)].copy()
    if g.empty:
        raise SystemExit("No data for given platform/N")

    piv = g.pivot_table(index="kernel", columns="opt_level", values="ticks")
    piv = piv.fillna(0)

    # Order kernels in a stable, human-friendly way if possible
    preferred_order = [
        "vec_abs",
        "vec_add",
        "vec_mul",
        "dot_product",
        "mat_mul",
        "conv2d_small",
        "softmax",
        "rmsnorm",
    ]
    kernels = [k for k in preferred_order if k in piv.index] + [
        k for k in piv.index if k not in preferred_order
    ]

    rows = []
    for kernel in kernels:
        wasm = float(piv.loc[kernel, "none"]) if "none" in piv.columns else 0.0
        opt = float(piv.loc[kernel, "opt"]) if "opt" in piv.columns else 0.0
        speedup = (wasm / opt) if (opt and wasm) else 0.0
        rows.append((kernel, wasm, opt, speedup))

    # Build LaTeX
    lines = []
    lines.append("\\begin{table}[tb]")
    lines.append("    \\centering")
    lines.append(f"    \\caption{{Execution cycles and speedups on {platform} for $N={N}$}}")
    lines.append(f"    \\label{{tab:{platform}_results_N{N}}}")
    lines.append("    \\scriptsize")
    lines.append("    \\begin{tabular}{lrrr}")
    lines.append("        \\hline")
    lines.append("        \\textbf{Kernel} & \\textbf{WASM} & \\textbf{WASM Hw-Opt} & \\textbf{Speedup} \\\\")
    lines.append("        \\hline")

    for kernel, wasm, opt, speedup in rows:
        kernel_tex = f"\\texttt{{{kernel}}}"
        wasm_s = fmt_int(wasm)
        opt_s = fmt_int(opt)
        speed_s = f"{speedup:.2f}\\times" if speedup else "-"
            lines.append(f"        {kernel_tex} & {wasm_s} & {opt_s} & {speed_s} \\\\")

    lines.append("    \\end{tabular}")
    lines.append("\\end{table}")

    return "\n".join(lines)


def generate_table_allNs(df, platform):
    # Build a table with rows (kernel, N, WASM, HW-opt, Zephyr, Speedup)
    g = df[df["platform"] == platform].copy()
    if g.empty:
        raise SystemExit("No data for given platform")

    Ns = sorted(g["N"].unique())
    kernels = sorted(g["kernel"].unique())

    lines = []
    lines.append("\\begin{table}[tb]")
    lines.append("    \\centering")
    lines.append(f"    \\caption{{Execution cycles and speedups on {platform} for all N}}")
    lines.append(f"    \\label{{tab:{platform}_results_allN}}")
    lines.append("    \\scriptsize")
    lines.append("    \\begin{tabular}{l r r r r r}")
    lines.append("        \\hline")
    lines.append("        \\textbf{Kernel} & \\textbf{N} & \\textbf{WASM} & \\textbf{WASM Hw-Opt} & \\textbf{Zephyr} & \\textbf{Speedup} \\\\")
    lines.append("        \\hline")

    for kernel in kernels:
        for N in Ns:
            row = g[(g["kernel"] == kernel) & (g["N"] == N)]
            if row.empty:
                wasm = opt = zeph = 0.0
            else:
                wasm_rows = row[row["opt_level"] == "none"]["ticks"].values
                opt_rows = row[row["opt_level"] == "opt"]["ticks"].values
                zeph_rows = row[row["opt_level"] == "zephyr"]["ticks"].values
                wasm = float(wasm_rows[0]) if len(wasm_rows) else 0.0
                opt = float(opt_rows[0]) if len(opt_rows) else 0.0
                zeph = float(zeph_rows[0]) if len(zeph_rows) else 0.0

            speedup = (wasm / opt) if (opt and wasm) else 0.0
            kernel_tex = f"\\texttt{{{kernel}}}"
            wasm_s = fmt_int(wasm) if wasm else "0"
            opt_s = fmt_int(opt) if opt else "0"
            zeph_s = fmt_int(zeph) if zeph else "0"
            speed_s = f"{speedup:.2f}\\times" if speedup else "-"
            lines.append(f"        {kernel_tex} & {N} & {wasm_s} & {opt_s} & {zeph_s} & {speed_s} \\\")
    lines.append("    \\end{tabular}")
    lines.append("\\end{table}")

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--platform", required=False, help="Platform name; omit to generate for all platforms")
    parser.add_argument("--N", type=int, required=False, default=1024, help="Problem size N (default: 1024)")
    parser.add_argument("--all", action="store_true", help="Generate table for all N values for the platform")
    parser.add_argument("--out", default=None)
    args = parser.parse_args()

    df = pd.read_csv(CSV_PATH)
    out_files = []

    if args.all:
        # If platform provided, generate for that platform; otherwise do for all
        platforms = [args.platform] if args.platform else sorted(df["platform"].unique())
        for p in platforms:
            tex = generate_table_allNs(df, p)
            if args.out:
                outpath = Path(args.out)
            else:
                outdir = HERE / "tables"
                outdir.mkdir(parents=True, exist_ok=True)
                outpath = outdir / f"{p}_allN.tex"
            outpath.write_text(tex)
            out_files.append(outpath)
            print(f"Wrote LaTeX table to {outpath}")
    else:
        # One-N tables (default N=1024)
        Nval = args.N
        platforms = [args.platform] if args.platform else sorted(df["platform"].unique())
        for p in platforms:
            tex = generate_table_oneN(df, p, Nval)
            if args.out and args.platform:
                outpath = Path(args.out)
            else:
                outdir = HERE / "tables"
                outdir.mkdir(parents=True, exist_ok=True)
                outpath = outdir / f"{p}_N{Nval}.tex"
            outpath.write_text(tex)
            out_files.append(outpath)
            print(f"Wrote LaTeX table to {outpath}")

    # If run without --out and single platform, also print to stdout for convenience
    if len(out_files) == 1 and not args.out:
        print(out_files[0].read_text())


if __name__ == "__main__":
    main()
