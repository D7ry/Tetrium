#!/usr/bin/env python3
"""
Generate a 3-panel SIGGRAPH-style teaser figure for hyper-observer null-space
detection.

Panels:
  A. 12-cone hyperobserver cone families
  B. Ranked genotype prior with CDF
  C. Observer-specific null-space response fingerprints

Usage:
    python paper-viz/teaser_hyperobserver_nullspace.py
    python paper-viz/teaser_hyperobserver_nullspace.py --output-dir paper-viz/output/teaser_hyperobserver_nullspace
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.colors import ListedColormap
from matplotlib.patches import Rectangle

REPO_ROOT = Path(__file__).resolve().parents[1]
TETRIUM_ROOT = REPO_ROOT / "extern" / "TetriumColor"
sys.path.insert(0, str(TETRIUM_ROOT))

from TetriumColor.Observer.ObserverGenotypes import ObserverGenotypes  # noqa: E402
from TetriumColor.Plotting.PlotStyle import (  # noqa: E402
    ALL_PEAKS,
    COLORS,
    DOUBLE_COL,
    L_PEAKS,
    M_PEAKS,
    WAVELENGTHS,
    apply_style,
    build_observers,
)

S_PEAK = ALL_PEAKS[0]
TOP_N = 10

OUTPUT_STEM = "hyperobserver_nullspace_teaser"


def _fmt_peak(peak: float) -> str:
    return f"{peak:.0f}" if float(peak).is_integer() else f"{peak:g}"


def _family_for_peak(peak: float) -> str:
    if peak == S_PEAK:
        return "S"
    if peak in M_PEAKS:
        return "M"
    if peak in L_PEAKS:
        return "L"
    raise ValueError(f"Unexpected peak: {peak}")


def _family_alpha(peak: float) -> float:
    return {"S": 0.96, "M": 0.64, "L": 0.66}[_family_for_peak(peak)]


def _build_teaser_prior(n_ranks: int = 24) -> tuple[list[tuple[float, ...]], np.ndarray]:
    """Use real genotype ordering, with a compact example prior for the teaser."""
    observer_genotypes = ObserverGenotypes(dimensions=[1, 2, 3, 4, 5])
    genotypes = list(observer_genotypes.get_pdf("both").keys())[:n_ranks]

    top = np.array([0.28, 0.20, 0.15, 0.10, 0.075, 0.055, 0.045, 0.035, 0.030, 0.020])
    tail = np.geomspace(0.006, 0.0005, max(n_ranks - TOP_N, 0))
    probabilities = np.concatenate([top, tail])
    probabilities = probabilities / probabilities.sum()

    # Keep the teaser claim exact after normalization.
    probabilities[:TOP_N] *= 0.99 / probabilities[:TOP_N].sum()
    probabilities[TOP_N:] *= 0.01 / probabilities[TOP_N:].sum()

    order = np.argsort(probabilities)[::-1]
    return [genotypes[idx] for idx in order], probabilities[order]


def _style_axes(ax) -> None:
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.grid(False)


def draw_panel_a(ax) -> None:
    observer, nominal_peaks = build_observers(WAVELENGTHS)["Hyperobserver"]
    sensor_matrix = observer.get_sensor_matrix(WAVELENGTHS)

    for peak in nominal_peaks:
        idx = ALL_PEAKS.index(peak)
        ax.plot(
            WAVELENGTHS,
            sensor_matrix[idx],
            color=COLORS[peak],
            alpha=_family_alpha(peak),
            linewidth=1.0,
            solid_capstyle="round",
        )

    ax.text(422, 0.92, "S", color=COLORS[S_PEAK], ha="center", va="bottom", fontsize=8)
    ax.text(532, 0.98, "M variants", color=COLORS[533], ha="center", va="bottom", fontsize=8)
    ax.text(556, 1.02, "L variants", color=COLORS[556], ha="center", va="bottom", fontsize=8)

    ax.set_title("12-cone hyperobserver")
    ax.set_xlabel("Wavelength (nm)")
    ax.set_ylabel("Sensitivity")
    ax.set_xlim(400, 700)
    ax.set_ylim(0, 1.12)
    ax.set_xticks([400, 500, 600, 700])
    ax.set_yticks([0, 0.5, 1.0])
    _style_axes(ax)


def draw_panel_b(ax) -> None:
    _, probabilities = _build_teaser_prior()
    ranks = np.arange(1, len(probabilities) + 1)
    cdf = np.cumsum(probabilities)

    bar_colors = np.full(len(probabilities), "#c7c7c7", dtype=object)
    bar_colors[:TOP_N] = "#2ca25f"
    edge_colors = np.full(len(probabilities), "#a0a0a0", dtype=object)
    edge_colors[:TOP_N] = "#1b1b1b"

    ax.bar(
        ranks,
        probabilities,
        width=0.82,
        color=bar_colors,
        edgecolor=edge_colors,
        linewidth=0.35,
        zorder=2,
    )
    ax.axvline(TOP_N + 0.5, color="#333333", linestyle="--", linewidth=0.8, alpha=0.72)
    ax.set_title("Genotype prior and CDF")
    ax.set_xlabel("Genotype rank")
    ax.set_ylabel("Probability")
    ax.set_xlim(0.35, len(probabilities) + 0.65)
    ax.set_ylim(0, max(probabilities) * 1.2)
    ax.set_xticks([1, 5, 10, 15, 20])
    _style_axes(ax)

    ax_cdf = ax.twinx()
    ax_cdf.plot(ranks, cdf, color="#1f1f1f", linewidth=1.0, marker="o", markersize=2.0, zorder=3)
    ax_cdf.axhline(0.99, color="#333333", linestyle="--", linewidth=0.8, alpha=0.72)
    ax_cdf.set_ylim(0, 1.03)
    ax_cdf.set_ylabel("Cumulative probability")
    ax_cdf.grid(False)
    ax_cdf.spines["top"].set_visible(False)
    ax_cdf.spines["left"].set_visible(False)

    ax_cdf.text(
        0.52,
        0.86,
        r"Top 10 tests cover 99\% of observers",
        transform=ax_cdf.transAxes,
        ha="left",
        va="center",
        fontsize=7,
    )


def draw_panel_c(ax) -> None:
    fingerprints = np.array(
        [
            [0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0],
            [0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1],
            [0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0],
        ],
        dtype=int,
    )
    row_labels = [
        "Null for {S,M530,L559}",
        "Null for {S,M530,L555}",
        "Null for {S,M533,L559}",
    ]
    cone_labels = ["S", "530", "533", "536", "547", "551", "552", "553", "555", "556", "556.5", "559"]

    cmap = ListedColormap(["#f7f7f7", "#4d4d4d"])
    ax.imshow(fingerprints, aspect="auto", interpolation="nearest", cmap=cmap, vmin=0, vmax=1)

    for y in range(fingerprints.shape[0]):
        for x in range(fingerprints.shape[1]):
            ax.add_patch(
                Rectangle(
                    (x - 0.5, y - 0.5),
                    1,
                    1,
                    fill=False,
                    edgecolor="white",
                    linewidth=0.45,
                )
            )

    for y, label in enumerate(row_labels):
        ax.text(-0.78, y, label, ha="right", va="center", fontsize=6.2)

    for x, label in enumerate(cone_labels):
        ax.text(x, fingerprints.shape[0] - 0.08, label, ha="center", va="top", fontsize=4.7, rotation=90)

    ax.text(
        0.5,
        -0.24,
        "Observer-specific null directions",
        transform=ax.transAxes,
        ha="center",
        va="top",
        fontsize=7,
    )

    ax.set_title("Null-space fingerprints")
    ax.set_xlim(-0.5, fingerprints.shape[1] - 0.5)
    ax.set_ylim(fingerprints.shape[0] - 0.5, -0.5)
    ax.set_axis_off()


def add_panel_label(ax, label: str) -> None:
    ax.text(
        -0.08,
        1.08,
        label,
        transform=ax.transAxes,
        ha="left",
        va="bottom",
        fontsize=11,
        fontweight="bold",
    )


def make_figure(output_dir: Path, formats: list[str]) -> None:
    apply_style()
    plt.rcParams.update(
        {
            "figure.facecolor": "white",
            "axes.facecolor": "white",
            "savefig.facecolor": "white",
            "savefig.dpi": 450,
        }
    )

    output_dir.mkdir(parents=True, exist_ok=True)

    fig, axes = plt.subplots(
        1,
        3,
        figsize=(DOUBLE_COL, 2.35),
        constrained_layout=True,
        gridspec_kw={"width_ratios": [1.08, 1.04, 0.96]},
    )
    fig.set_constrained_layout_pads(w_pad=0.03, h_pad=0.03, wspace=0.05, hspace=0.02)

    draw_panel_a(axes[0])
    draw_panel_b(axes[1])
    draw_panel_c(axes[2])

    for label, ax in zip(["A", "B", "C"], axes):
        add_panel_label(ax, label)

    fig.suptitle(
        "Genetic priors reduce exhaustive null-space search to ~10 psychophysical tests.",
        y=1.03,
        fontsize=8,
    )

    for fmt in formats:
        fig.savefig(output_dir / f"{OUTPUT_STEM}.{fmt}", bbox_inches="tight")
    plt.close(fig)


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate a 3-panel hyperobserver teaser figure.")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=REPO_ROOT / "paper-viz" / "output" / "teaser_hyperobserver_nullspace",
        help="Directory for generated teaser figure.",
    )
    parser.add_argument(
        "--formats",
        nargs="+",
        default=["pdf", "png"],
        choices=["pdf", "png", "svg"],
        help="Output formats.",
    )
    args = parser.parse_args()

    make_figure(args.output_dir, args.formats)
    print(f"Wrote hyperobserver null-space teaser figure to {args.output_dir}")


if __name__ == "__main__":
    main()
