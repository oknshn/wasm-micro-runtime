import pandas as pd
import matplotlib.pyplot as plt
from pathlib import Path
import math
from matplotlib.patches import Patch

HERE = Path(__file__).resolve().parent
CSV_PATH = HERE / "benchmark_results.csv"


PLATFORM_LABELS = {
    "max32690": "MAX32690EVKIT",
    "riscv32": "ESP32-C3 DevKitM",
    "qemu_cortex_a53": "QEMU",
}


OPT_LABELS = {
    "none": "WASM",
    "opt": "WASM Hardware-Optimized",
    "zephyr": "Zephyr",
}


def pretty_platform(name: str) -> str:
    return PLATFORM_LABELS.get(name, name)


def pretty_opt(opt_level: str) -> str:
    return OPT_LABELS.get(opt_level, opt_level)


def synthesize_riscv(df: pd.DataFrame) -> pd.DataFrame:
    """Legacy helper (no-op now that real RISC-V data is provided).

    Kept for compatibility; just returns the DataFrame unchanged.
    """

    return df


def load_data():
    df = pd.read_csv(CSV_PATH)
    # Ensure consistent ordering
    df["opt_level"] = pd.Categorical(df["opt_level"], ["none", "opt", "zephyr"])

    # RISC-V data is now real; no synthetic filling
    return df


def plot_kernel(df, kernel: str, outdir: Path):
    kdf = df[df["kernel"] == kernel].copy()
    if kdf.empty:
        return

    outdir.mkdir(parents=True, exist_ok=True)

    # One figure per platform, all execution modes (WASM, optimized, Zephyr)
    for platform in sorted(kdf["platform"].unique()):
        pdf = kdf[kdf["platform"] == platform]
        if pdf.empty:
            continue

        plt.figure(figsize=(6, 4))
        for opt_level, g in pdf.groupby("opt_level"):
            # Drop rows without ticks (e.g. RISC-V placeholders)
            g = g.dropna(subset=["ticks"])
            if g.empty:
                continue
            label = OPT_LABELS.get(opt_level, opt_level)
            plt.plot(g["N"], g["ticks"], marker="o", label=label)

        plt.title(f"{kernel} - {platform}")
        plt.xlabel("N")
        plt.ylabel("Elapsed ticks")
        plt.xscale("log", base=2)
        plt.yscale("log")
        plt.grid(True, which="both", ls=":", alpha=0.4)
        plt.legend()

        fname = outdir / f"{kernel}_{platform}.png"
        plt.tight_layout()
        plt.savefig(fname, dpi=150)
        plt.close()


def plot_kernel_bars(df, kernel: str, outdir: Path):
    """Plot grouped bar charts: ticks vs N, grouped by platform, per opt level."""

    kdf = df[df["kernel"] == kernel].copy()
    kdf = kdf.dropna(subset=["ticks"])
    if kdf.empty:
        return

    outdir.mkdir(parents=True, exist_ok=True)

    for opt_level, g in kdf.groupby("opt_level"):
        if g.empty:
            continue

        # Pivot so index = N, columns = platform, values = ticks
        piv = g.pivot_table(index="N", columns="platform", values="ticks")
        piv = piv.sort_index()

        ax = piv.plot(kind="bar", figsize=(7, 4))
        ax.set_title(f"{kernel} - {OPT_LABELS.get(opt_level, opt_level)}")
        ax.set_xlabel("N")
        ax.set_ylabel("Elapsed ticks")
        ax.set_yscale("log")
        ax.grid(True, which="both", ls=":", alpha=0.4)
        plt.tight_layout()

        fname = outdir / f"{kernel}_bars_opt-{opt_level}.png"
        plt.savefig(fname, dpi=150)
        plt.close()


def plot_opt_comparison_bars(df, kernel: str, outdir: Path):
    """For each platform, compare none vs opt for a kernel across N.

    X-axis: N, bars: opt_level (none, opt), one figure per platform.
    """

    kdf = df[df["kernel"] == kernel].copy()
    kdf = kdf.dropna(subset=["ticks"])
    if kdf.empty:
        return

    outdir.mkdir(parents=True, exist_ok=True)

    for platform, g in kdf.groupby("platform"):
        if g.empty:
            continue

        piv = g.pivot_table(index="N", columns="opt_level", values="ticks")
        piv = piv.sort_index()

        # Use human-friendly labels for opt levels
        piv = piv.rename(columns=OPT_LABELS)

        ax = piv.plot(kind="bar", figsize=(7, 4))
        ax.set_title(
            f"{kernel} - {pretty_platform(platform)}: WASM vs WASM Hardware-Optimized vs Zephyr"
        )
        ax.set_xlabel("N")
        ax.set_ylabel("Elapsed ticks")
        ax.set_yscale("log")
        ax.grid(True, which="both", ls=":", alpha=0.4)
        plt.tight_layout()

        fname = outdir / f"{kernel}_opt-compare_{platform}.png"
        plt.savefig(fname, dpi=150)
        plt.close()


def plot_platform_summary(df, platform: str, outdir: Path):
    """For a given platform, show all kernels for each size.

    One figure per platform, with subplots by N. In each subplot,
    X-axis = kernel, bars = opt_level (none vs opt), Y = ticks (log).
    """

    pdf = df[df["platform"] == platform].copy()
    pdf = pdf.dropna(subset=["ticks"])
    if pdf.empty:
        return

    outdir.mkdir(parents=True, exist_ok=True)

    Ns = sorted(pdf["N"].unique())
    num_N = len(Ns)
    if num_N == 0:
        return

    ncols = min(3, num_N)
    nrows = math.ceil(num_N / ncols)

    fig, axes = plt.subplots(nrows=nrows, ncols=ncols, figsize=(4 * ncols, 3 * nrows), squeeze=False)

    for i, Nval in enumerate(Ns):
        r = i // ncols
        c = i % ncols
        ax = axes[r][c]

        g = pdf[pdf["N"] == Nval]
        if g.empty:
            ax.axis("off")
            continue

        piv = g.pivot_table(index="kernel", columns="opt_level", values="ticks")
        piv = piv.sort_index()

        # Human-friendly legend labels for opt levels
        piv = piv.rename(columns=OPT_LABELS)

        piv.plot(kind="bar", ax=ax)
        ax.set_title(f"N={Nval}")
        ax.set_xlabel("Kernel")
        ax.set_ylabel("Elapsed ticks")
        ax.set_yscale("log")
        ax.grid(True, which="both", ls=":", alpha=0.4)
        ax.tick_params(axis="x", rotation=45)

    # Hide any unused subplots
    total_axes = nrows * ncols
    for j in range(num_N, total_axes):
        r = j // ncols
        c = j % ncols
        axes[r][c].axis("off")

    fig.suptitle(
        f"{pretty_platform(platform)}: all kernels per size (WASM, WASM Hardware-Optimized, Zephyr)"
    )
    plt.tight_layout(rect=[0, 0.03, 1, 0.95])

    fname = outdir / f"platform_summary_{platform}.png"
    fig.savefig(fname, dpi=150)
    plt.close(fig)


def plot_platform_overview(df, platform: str, outdir: Path):
    """Single graph per platform: all kernels, N from min..max.

    - X-axis: N (log scale)
    - Y-axis: ticks (log scale)
    - Color encodes kernel
    - Line style encodes opt_level (solid=none, dashed=opt)
    """

    pdf = df[df["platform"] == platform].copy()
    pdf = pdf.dropna(subset=["ticks"])
    if pdf.empty:
        return

    outdir.mkdir(parents=True, exist_ok=True)

    plt.figure(figsize=(8, 5))
    ax = plt.gca()

    kernels = sorted(pdf["kernel"].unique())
    colors = plt.rcParams["axes.prop_cycle"].by_key().get("color", ["C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7", "C8", "C9"])

    for idx, kernel in enumerate(kernels):
        kdf = pdf[pdf["kernel"] == kernel]
        if kdf.empty:
            continue

        color = colors[idx % len(colors)]

        # Render each execution mode with different line style
        for opt_level, style, marker in [
            ("none", "-", "o"),
            ("opt", "--", "s"),
            ("zephyr", ":", "^"),
        ]:
            mode_df = (
                kdf[kdf["opt_level"] == opt_level]
                .dropna(subset=["ticks"])
                .sort_values("N")
            )
            if mode_df.empty:
                continue
            ax.plot(
                mode_df["N"],
                mode_df["ticks"],
                marker=marker,
                linestyle=style,
                color=color,
                label=f"{kernel} ({OPT_LABELS.get(opt_level, opt_level)})",
            )

        ax.set_title(f"{platform}: all kernels (WASM, optimized, Zephyr)")
    ax.set_xlabel("N")
    ax.set_ylabel("Elapsed ticks")
    ax.set_xscale("log", base=2)
    ax.set_yscale("log")
    ax.grid(True, which="both", ls=":", alpha=0.4)
    ax.legend(fontsize="small", ncol=2)

    plt.tight_layout()

    fname = outdir / f"platform_overview_{platform}.png"
    plt.savefig(fname, dpi=150)
    plt.close()


def plot_platform_bar_overview(df, platform: str, outdir: Path):
    """Bar graph per platform: all functions, all sizes.

    - X-axis: N (4..1024)
        - Bars: per (kernel, opt_level) pair
            * Color encodes kernel
            * Dotted fill inside bar indicates HW-optimized vs plain fill for WASM
    """

    pdf = df[df["platform"] == platform].copy()
    pdf = pdf.dropna(subset=["ticks"])
    if pdf.empty:
        return

    outdir.mkdir(parents=True, exist_ok=True)

    Ns = sorted(pdf["N"].unique())
    kernels = sorted(pdf["kernel"].unique())
    opt_levels = ["none", "opt", "zephyr"]

    if not Ns or not kernels:
        return

    x_base = list(range(len(Ns)))
    total_series = len(kernels) * len(opt_levels)
    width = 0.8 / max(total_series, 1)

    colors = plt.rcParams["axes.prop_cycle"].by_key().get(
        "color", ["C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7", "C8", "C9"]
    )

    fig, ax = plt.subplots(figsize=(10, 5))

    series_index = 0
    for k_idx, kernel in enumerate(kernels):
        color = colors[k_idx % len(colors)]
        for opt_level in opt_levels:
            g = pdf[(pdf["kernel"] == kernel) & (pdf["opt_level"] == opt_level)]
            if g.empty:
                series_index += 1
                continue

            y_vals = []
            for Nval in Ns:
                rows = g[g["N"] == Nval]["ticks"]
                y_vals.append(float(rows.iloc[0]) if not rows.empty else float("nan"))

            offset = (series_index - (total_series - 1) / 2.0) * width
            x_positions = [xb + offset for xb in x_base]

            # Same base color for all; distinguish execution mode by
            # alpha/hatch pattern.
            if opt_level == "none":  # WASM
                alpha = 0.9
                hatch = ""
            elif opt_level == "opt":  # WASM Hardware-Optimized
                alpha = 0.5
                hatch = ".."
            else:  # Zephyr native
                alpha = 0.9
                hatch = "//"

            edgecolor = "black"

            # Actual bars do not carry legend labels; we build a custom legend
            bars = ax.bar(
                x_positions,
                y_vals,
                width,
                color=color,
                alpha=alpha,
                edgecolor=edgecolor,
                linewidth=0.8,
                 hatch=hatch,
                label="_nolegend_",
            )

            series_index += 1

    ax.set_xticks(x_base)
    ax.set_xticklabels([str(N) for N in Ns])
    ax.set_xlabel("N")
    ax.set_ylabel("Elapsed ticks")
    ax.set_yscale("log")
    ax.set_title(f"{pretty_platform(platform)}")
    ax.grid(True, which="both", ls=":", alpha=0.4)

    # Build a custom legend: colors for kernels, hatch style for opt levels
    kernel_handles = [
        Patch(facecolor=colors[k_idx % len(colors)], label=kernel)
        for k_idx, kernel in enumerate(kernels)
    ]
    style_handles = [
        Patch(facecolor="gray", alpha=0.9, edgecolor="black", hatch="", label=pretty_opt("none")),
        Patch(facecolor="gray", alpha=0.5, edgecolor="black", hatch="..", label=pretty_opt("opt")),
        Patch(facecolor="gray", alpha=0.9, edgecolor="black", hatch="//", label=pretty_opt("zephyr")),
    ]
    handles = kernel_handles + style_handles
    labels = [h.get_label() for h in handles]
    ax.legend(handles, labels, fontsize="small", ncol=2)

    plt.tight_layout()

    fname = outdir / f"platform_bar_overview_{platform}.png"
    fig.savefig(fname, dpi=150)
    plt.close(fig)


def main():
    df = load_data()
    outdir_optcmp = HERE / "plots_opt_compare"
    outdir_platform = HERE / "plots_platform_summary"
    outdir_plat_bar_overview = HERE / "plots_platform_bar_overview"
    # Per-kernel, per-platform opt vs non-opt comparison bars
    for kernel in sorted(df["kernel"].unique()):
        plot_opt_comparison_bars(df, kernel, outdir_optcmp)

    # Platform-level views: summary subplots + single bar overview
    for platform in sorted(df["platform"].unique()):
        plot_platform_summary(df, platform, outdir_platform)
        plot_platform_bar_overview(df, platform, outdir_plat_bar_overview)

    print(f"Saved WASM/optimized/Zephyr comparison bar plots under {outdir_optcmp}")
    print(f"Saved platform summaries under {outdir_platform}")
    print(f"Saved platform bar overviews under {outdir_plat_bar_overview}")


if __name__ == "__main__":
    main()
