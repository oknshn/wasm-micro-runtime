import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path
import math

HERE = Path(__file__).resolve().parent
CSV_PATH = HERE / "benchmark_results.csv"
OUT_DIR = HERE / "deltas"


def load_data():
    df = pd.read_csv(CSV_PATH)
    # Keep only rows with ticks
    df = df.dropna(subset=["ticks"])
    df["ticks"] = df["ticks"].astype(float)
    return df


def compute_group_delta(df, platform, kernel):
    g = df[(df.platform == platform) & (df.kernel == kernel)].copy()
    if g.empty:
        return None

    opt = g[g.opt_level == "opt"][ ["N", "ticks"] ].rename(columns={"ticks": "opt_ticks"})
    zeph = g[g.opt_level == "zephyr"][ ["N", "ticks"] ].rename(columns={"ticks": "zephyr_ticks"})

    if opt.empty or zeph.empty:
        return None

    merged = pd.merge(opt, zeph, on="N")
    if merged.empty:
        return None

    merged["delta_abs"] = merged["zephyr_ticks"] - merged["opt_ticks"]
    merged["delta_rel_pct"] = merged["delta_abs"] / merged["zephyr_ticks"] * 100.0

    # Regression of delta_abs vs log2(N)
    x = np.log2(merged["N"].astype(float).values)
    y = merged["delta_abs"].values

    if len(x) >= 2:
        # linear fit y = m*x + b
        (m, b), cov = np.polyfit(x, y, 1, cov=True)
        m_se = math.sqrt(cov[0, 0]) if cov is not None else float("nan")
        # R^2
        y_pred = m * x + b
        ss_res = np.sum((y - y_pred) ** 2)
        ss_tot = np.sum((y - np.mean(y)) ** 2)
        r2 = 1.0 - ss_res / ss_tot if ss_tot > 0 else float("nan")
    else:
        m = float("nan")
        m_se = float("nan")
        r2 = float("nan")

    stats = {
        "platform": platform,
        "kernel": kernel,
        "n_points": len(merged),
        "mean_delta": float(merged["delta_abs"].mean()),
        "std_delta": float(merged["delta_abs"].std(ddof=0)),
        "cv_delta": float(merged["delta_abs"].std(ddof=0) / merged["delta_abs"].mean()) if merged["delta_abs"].mean() != 0 else float("nan"),
        "mean_delta_rel_pct": float(merged["delta_rel_pct"].mean()),
        "slope": float(m),
        "slope_se": float(m_se),
        "r2": float(r2),
    }

    return merged, stats


def make_plots(merged, platform, kernel, outdir: Path):
    outdir.mkdir(parents=True, exist_ok=True)
    Ns = merged["N"].values

    plt.figure(figsize=(6, 4))
    plt.plot(Ns, merged["delta_abs"], marker="o")
    plt.xscale("log", base=2)
    plt.grid(True, which="both", ls=":", alpha=0.4)
    plt.xlabel("N")
    plt.ylabel("Zephyr - HW-opt ticks (absolute)")
    plt.title(f"{platform} / {kernel}: delta (zephyr - hw_opt)")
    fname = outdir / f"delta_abs_{platform}_{kernel}.png"
    plt.tight_layout()
    plt.savefig(fname, dpi=150)
    plt.close()

    plt.figure(figsize=(6, 4))
    plt.plot(Ns, merged["delta_rel_pct"], marker="o")
    plt.xscale("log", base=2)
    plt.grid(True, which="both", ls=":", alpha=0.4)
    plt.xlabel("N")
    plt.ylabel("Relative delta (%)")
    plt.title(f"{platform} / {kernel}: relative delta (%)")
    fname = outdir / f"delta_rel_{platform}_{kernel}.png"
    plt.tight_layout()
    plt.savefig(fname, dpi=150)
    plt.close()


def write_latex_table(stats_list, outpath: Path):
    outpath.parent.mkdir(parents=True, exist_ok=True)
    with open(outpath, "w") as f:
        f.write("\\begin{tabular}{l l r r r r r}\\n")
        f.write("\\hline\\n")
        f.write("Platform & Kernel & N & Mean (ticks) & Std & Slope & R^2 \\\\n")
        f.write("\\hline\\n")
        for s in stats_list:
            # represent mean std rounded
            f.write(f"{s['platform']} & {s['kernel']} & {s['n_points']} & {s['mean_delta']:.1f} & {s['std_delta']:.1f} & {s['slope']:.3f} & {s['r2']:.3f} \\\\n")
        f.write("\\hline\\n")
        f.write("\\end{tabular}\\n")


def write_per_group_latex(merged: pd.DataFrame, platform: str, kernel: str, outdir: Path, stats: dict):
    """Write a LaTeX table listing N, zephyr, hw-opt, delta_abs, delta_rel_pct for this group."""
    outdir.mkdir(parents=True, exist_ok=True)
    fname = outdir / f"delta_table_{platform}_{kernel}.tex"
    with open(fname, "w") as f:
        f.write("\\begin{table}[tb]\n")
        f.write("    \\centering\n")
        f.write(f"    \\caption{{Delta (Zephyr - HW-Optimized) for {platform} / {kernel}}}\n")
        f.write(f"    \\label{{tab:delta_{platform}_{kernel}}}\n")
        f.write("    \\scriptsize\n")
        # Columns: N | Zephyr | HW-Opt | Delta
        f.write("    \\begin{tabular}{r r r r}\n")
        f.write("        \\hline\n")
        f.write("        \\textbf{N} & \\textbf{Zephyr} & \\textbf{HW-Opt} & \\textbf{Delta} \\\\\\n+")
        f.write("        \\hline\n")

        # Sort by N
        merged_sorted = merged.sort_values("N")
        for _, row in merged_sorted.iterrows():
            N = int(row["N"])
            zeph = int(row["zephyr_ticks"]) if not np.isnan(row["zephyr_ticks"]) else 0
            opt = int(row["opt_ticks"]) if not np.isnan(row["opt_ticks"]) else 0
            dabs = int(row["delta_abs"]) if not np.isnan(row["delta_abs"]) else 0
            f.write(f"        {N} & {zeph:,} & {opt:,} & {dabs:,}\\\\\n")

        f.write("        \\hline\n")
        # Add regression summary as a note below the table
        f.write("    \\end{tabular}\n")
        f.write(f"    \\vspace{{2mm}}\\\\\\n")
        f.write(f"    Slope vs log2(N): {stats.get('slope', float('nan')):.3f} \\ (R^2={stats.get('r2', float('nan')):.3f})\\n")
        f.write("\\end{table}\n")

    return fname


def write_combined_platform_table(combined: pd.DataFrame, platform: str, outdir: Path):
    """Write a single LaTeX longtable for all kernels/N on a given platform.

    The combined table has columns: Kernel | N | Zephyr | HW-Opt | Delta
    Uses the longtable environment which can span pages in LaTeX.
    """
    outdir.mkdir(parents=True, exist_ok=True)
    fname = outdir / f"combined_{platform}.tex"
    with open(fname, "w") as f:
        f.write("% Requires: \\usepackage{longtable} in preamble\n")
        f.write("\\begin{center}\n")
        f.write(f"\\begin{{longtable}}{{l r r r r}}\n")
        f.write(f"\\caption{{Delta (Zephyr - HW-Optimized) across kernels and sizes for {platform}}}\\\\\n")
        f.write(f"\\label{{tab:combined_{platform}}}\\\\\n")
        f.write("\\hline\\n")
        f.write("Kernel & N & Zephyr & HW-Opt & Delta \\\\ \n")
        f.write("\\hline\\n")
        f.write("\\endfirsthead\n")
        f.write("\\hline\\n")
        f.write("Kernel & N & Zephyr & HW-Opt & Delta \\\\ \n")
        f.write("\\hline\\n")
        f.write("\\endhead\n")

        # Rows already sorted by kernel, N
        for _, row in combined.iterrows():
            kernel = row["kernel"]
            N = int(row["N"])
            zeph = int(row["zephyr_ticks"]) if not np.isnan(row["zephyr_ticks"]) else 0
            opt = int(row["opt_ticks"]) if not np.isnan(row["opt_ticks"]) else 0
            dabs = int(row["delta_abs"]) if not np.isnan(row["delta_abs"]) else 0
            f.write(f"{kernel} & {N} & {zeph:,} & {opt:,} & {dabs:,} \\\\ \n")

        f.write("\\hline\\n")
        f.write("\\end{longtable}\n")
        f.write("\\end{center}\n")

    return fname


def main():
    df = load_data()
    platforms = sorted(df["platform"].unique())
    kernels = sorted(df["kernel"].unique())

    out_csv = OUT_DIR / "deltas_summary.csv"
    out_tex = OUT_DIR / "deltas_table.tex"
    out_plots = OUT_DIR / "plots"

    rows = []
    stats_list = []
    for platform in platforms:
        for kernel in kernels:
            result = compute_group_delta(df, platform, kernel)
            if result is None:
                continue
            merged, stats = result
            rows.append(merged.assign(platform=platform, kernel=kernel))
            stats_list.append(stats)
            make_plots(merged, platform, kernel, out_plots)
            # write per-group LaTeX table
            tbl = write_per_group_latex(merged, platform, kernel, OUT_DIR / "tables", stats)
            print(f"Wrote LaTeX table for {platform}/{kernel} -> {tbl}")

    # After processing all groups, produce combined platform tables
    if rows:
        all_merged = pd.concat(rows, ignore_index=True)
        all_merged.to_csv(OUT_DIR / "deltas_per_point.csv", index=False)

        # Write combined table per platform
        for platform in platforms:
            p_rows = all_merged[all_merged["platform"] == platform]
            if p_rows.empty:
                continue
            combined = p_rows.sort_values(["kernel", "N"])
            combined_fname = write_combined_platform_table(combined, platform, OUT_DIR / "tables")
            print(f"Wrote combined table for {platform} -> {combined_fname}")

    if rows:
        all_merged = pd.concat(rows, ignore_index=True)
        all_merged.to_csv(OUT_DIR / "deltas_per_point.csv", index=False)

    pd.DataFrame(stats_list).to_csv(out_csv, index=False)
    write_latex_table(stats_list, out_tex)

    print(f"Wrote CSV summary to {out_csv}")
    print(f"Wrote LaTeX table to {out_tex}")
    print(f"Wrote plots to {out_plots}")


if __name__ == "__main__":
    main()
