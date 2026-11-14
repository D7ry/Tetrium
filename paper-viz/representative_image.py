#!/usr/bin/env python3
"""
Generate a standalone representative image for the hyper-observer null-space
workflow.

The layout follows the existing teaser's left-to-right story, but uses larger
schematic elements and replaces the final fingerprint plot with two viewers
responding differently to the same stimulus.

Usage:
    python paper-viz/representative_image.py
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Circle, FancyArrowPatch, Polygon, Rectangle

REPO_ROOT = Path(__file__).resolve().parents[1]
TETRIUM_ROOT = REPO_ROOT / "extern" / "TetriumColor"
sys.path.insert(0, str(TETRIUM_ROOT))

from TetriumColor.Observer.ObserverGenotypes import ObserverGenotypes  # noqa: E402
from TetriumColor.Plotting.PlotStyle import (  # noqa: E402
    ALL_PEAKS,
    COLORS,
    L_PEAKS,
    M_PEAKS,
    WAVELENGTHS,
    apply_style,
    build_observers,
)

OUTPUT_STEM = "representative_image"
TOP_N = 10
WIDTH_IN = 10.0
HEIGHT_IN = 20.0 / 3.0
DPI = 300

INK = "#202124"
MUTED = "#6f7378"
GRID = "#d9dde2"
PALE = "#f4f6f8"
GREEN = "#2f9e73"
BLUE = "#3167b1"
RED = "#c74d4d"
YELLOW = "#e0a526"


def _style_panel(ax) -> None:
    ax.set_facecolor("white")
    for spine in ax.spines.values():
        spine.set_visible(False)
    ax.tick_params(left=False, bottom=False, labelleft=False, labelbottom=False)


def _family_alpha(peak: float) -> float:
    if peak == ALL_PEAKS[0]:
        return 0.95
    if peak in M_PEAKS:
        return 0.58
    if peak in L_PEAKS:
        return 0.62
    return 0.7


def _build_teaser_prior(n_ranks: int = 24) -> np.ndarray:
    ObserverGenotypes(dimensions=[1, 2, 3, 4, 5])
    top = np.array([0.28, 0.20, 0.15, 0.10, 0.075, 0.055, 0.045, 0.035, 0.030, 0.020])
    tail = np.geomspace(0.006, 0.0005, max(n_ranks - TOP_N, 0))
    probabilities = np.concatenate([top, tail])
    probabilities = probabilities / probabilities.sum()
    probabilities[:TOP_N] *= 0.99 / probabilities[:TOP_N].sum()
    probabilities[TOP_N:] *= 0.01 / probabilities[TOP_N:].sum()
    return probabilities


def _panel_title(ax, number: str, title: str, subtitle: str) -> None:
    ax.text(
        0.0,
        1.04,
        number,
        transform=ax.transAxes,
        ha="left",
        va="bottom",
        fontsize=9.0,
        fontweight="bold",
        color=INK,
    )
    ax.text(
        0.13,
        1.04,
        title,
        transform=ax.transAxes,
        ha="left",
        va="bottom",
        fontsize=8.0,
        fontweight="bold",
        color=INK,
    )
    ax.text(
        0.13,
        0.94,
        subtitle,
        transform=ax.transAxes,
        ha="left",
        va="bottom",
        fontsize=6.0,
        color=MUTED,
    )


def draw_hyperobserver(ax) -> None:
    _style_panel(ax)
    observer, nominal_peaks = build_observers(WAVELENGTHS)["Hyperobserver"]
    sensor_matrix = observer.get_sensor_matrix(WAVELENGTHS)

    for peak in nominal_peaks:
        idx = ALL_PEAKS.index(peak)
        ax.plot(
            WAVELENGTHS,
            sensor_matrix[idx],
            color=COLORS[peak],
            alpha=_family_alpha(peak),
            linewidth=1.35,
            solid_capstyle="round",
        )

    ax.set_xlim(395, 705)
    ax.set_ylim(-0.02, 1.13)
    ax.set_xlabel(r"$\lambda$", fontsize=12, color=INK, labelpad=8)
    ax.set_xticks([420, 530, 560, 700])
    ax.set_xticklabels(["420", "530", "560", "700"], fontsize=8.0, color=MUTED)
    ax.set_yticks([])
    ax.tick_params(bottom=True, labelbottom=True, length=3.5, width=0.7, color=GRID)
    ax.spines["bottom"].set_visible(True)
    ax.spines["bottom"].set_color(GRID)
    ax.text(420, 0.95, "420", fontsize=8.4, color=COLORS[ALL_PEAKS[0]], ha="center", va="bottom")
    for peak in M_PEAKS:
        idx = ALL_PEAKS.index(peak)
        y = sensor_matrix[idx].max()
        ax.text(peak, y + 0.035, f"{int(peak)}", fontsize=7.2, color=COLORS[peak], ha="center", va="bottom")
    for peak in L_PEAKS:
        idx = ALL_PEAKS.index(peak)
        y = sensor_matrix[idx].max()
        label = f"{peak:g}"
        ax.text(
            peak,
            y + 0.045 + 0.03 * ((L_PEAKS.index(peak) % 3) - 1),
            label,
            fontsize=6.0,
            color=COLORS[peak],
            ha="center",
            va="bottom",
            rotation=35,
        )
    _panel_title(ax, "1", "Hyper-observer", "candidate cone peaks")


def draw_ordering(ax) -> None:
    _style_panel(ax)
    probs = _build_teaser_prior()
    ranks = np.arange(1, len(probs) + 1)
    cdf = np.cumsum(probs)
    colors = np.array(["#cfd5dc"] * len(probs), dtype=object)
    colors[:TOP_N] = GREEN

    ax.bar(ranks, probs, color=colors, edgecolor="white", linewidth=0.35, width=0.82)
    ax.axvline(TOP_N + 0.5, color=INK, lw=0.8, ls="--", alpha=0.75)
    ax.set_xlim(0.3, len(probs) + 0.7)
    ax.set_ylim(0, probs.max() * 1.34)
    ax.set_xticks([1, 5, 10, 20])
    ax.set_xticklabels(["1", "5", "10", "20"], fontsize=6.2, color=MUTED)
    ax.tick_params(bottom=True, labelbottom=True, length=2.5, width=0.5, color=GRID)
    ax.spines["bottom"].set_visible(True)
    ax.spines["bottom"].set_color(GRID)

    ax2 = ax.twinx()
    ax2.plot(ranks, cdf, color=INK, lw=1.25, marker="o", markersize=2.0)
    ax2.axhline(0.99, color=BLUE, lw=0.9, ls=":")
    ax2.set_ylim(0, 1.04)
    ax2.set_yticks([])
    for spine in ax2.spines.values():
        spine.set_visible(False)

    ax.text(11.0, probs.max() * 1.08, "99% in first 10", fontsize=7.2, color=BLUE, fontweight="bold")
    ax.text(12.0, probs.max() * 0.93, "trichromat prior", fontsize=5.9, color=MUTED)
    _panel_title(ax, "2", "Efficient ordering", "rank likely observers")


def draw_display(ax) -> None:
    _style_panel(ax)
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)

    base_x = np.array([0.18, 0.38, 0.60, 0.80])
    heights = [0.36, 0.48, 0.42, 0.34]
    colors = ["#5d75d9", "#35a853", "#f0b53a", "#d94f45"]
    labels = ["P1", "P2", "P3", "P4"]
    for x, h, color, label in zip(base_x, heights, colors, labels):
        ax.add_patch(Rectangle((x - 0.032, 0.48), 0.064, h, facecolor=color, edgecolor="white", lw=0.7))
        ax.text(x, 0.42, label, ha="center", va="center", fontsize=6.4, color=MUTED)

    ax.plot([0.1, 0.9], [0.48, 0.48], color=GRID, lw=0.9)
    ax.text(0.50, 0.93, "four primaries", ha="center", fontsize=7.4, color=INK)

    left = Circle((0.39, 0.22), 0.14, facecolor="#76bdd1", edgecolor=INK, lw=0.55, alpha=0.88)
    right = Circle((0.61, 0.22), 0.14, facecolor="#d3a75f", edgecolor=INK, lw=0.55, alpha=0.88)
    ax.add_patch(left)
    ax.add_patch(right)
    ax.text(0.50, 0.22, "=", ha="center", va="center", fontsize=13, fontweight="bold", color=INK)
    ax.text(0.50, 0.04, "metamer pair", ha="center", fontsize=6.3, color=MUTED)
    _panel_title(ax, "3", "Custom display", "targeted stimuli")


def _draw_viewer(ax, cx: float, cy: float, color: str, label: str, response: str) -> None:
    ax.add_patch(Circle((cx, cy + 0.14), 0.072, facecolor="#f3d1bd", edgecolor=INK, lw=0.7))
    ax.add_patch(Polygon(
        [(cx - 0.12, cy - 0.13), (cx + 0.12, cy - 0.13), (cx + 0.078, cy + 0.065), (cx - 0.078, cy + 0.065)],
        closed=True,
        facecolor=color,
        edgecolor=INK,
        lw=0.7,
    ))
    ax.add_patch(Circle((cx - 0.023, cy + 0.153), 0.008, facecolor=INK, edgecolor=INK))
    ax.add_patch(Circle((cx + 0.023, cy + 0.153), 0.008, facecolor=INK, edgecolor=INK))
    ax.text(cx, cy - 0.21, label, ha="center", va="center", fontsize=11.0, color=INK, fontweight="bold")
    ax.text(cx, cy - 0.30, response, ha="center", va="center", fontsize=8.4, color=MUTED)


def draw_classification(ax) -> None:
    _style_panel(ax)
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)

    ax.add_patch(Circle((0.50, 0.68), 0.15, facecolor="#76bdd1", edgecolor=INK, lw=0.8, alpha=0.88))
    ax.add_patch(Circle((0.50, 0.68), 0.082, facecolor="#d3a75f", edgecolor="white", lw=0.75, alpha=0.9))
    ax.text(0.50, 0.91, "same stimulus", ha="center", fontsize=15.5, color=INK)

    ax.add_patch(FancyArrowPatch((0.43, 0.55), (0.27, 0.41), arrowstyle="-|>", mutation_scale=14, lw=1.3, color=MUTED))
    ax.add_patch(FancyArrowPatch((0.57, 0.55), (0.73, 0.41), arrowstyle="-|>", mutation_scale=14, lw=1.3, color=MUTED))
    _draw_viewer(ax, 0.27, 0.22, "#6ea7d1", "match", "compatible")
    _draw_viewer(ax, 0.73, 0.22, "#d07066", "different", "separated")

    _panel_title(ax, "2", "Targeted test", "different responses")


def make_figure(output_dir: Path) -> Path:
    apply_style()
    plt.rcParams.update(
        {
            "figure.facecolor": "white",
            "axes.facecolor": "white",
            "savefig.facecolor": "white",
            "savefig.dpi": DPI,
            "text.usetex": False,
            "font.family": "DejaVu Sans",
            "font.size": 6.8,
            "axes.linewidth": 0.6,
        }
    )

    output_dir.mkdir(parents=True, exist_ok=True)
    output_path = output_dir / f"{OUTPUT_STEM}.jpg"

    fig, axes = plt.subplots(
        1,
        2,
        figsize=(WIDTH_IN, HEIGHT_IN),
        dpi=DPI,
        gridspec_kw={"width_ratios": [1.05, 0.95]},
    )
    fig.subplots_adjust(left=0.045, right=0.965, top=0.77, bottom=0.13, wspace=0.18)
    fig.text(
        0.035,
        0.955,
        "Personalized color tests",
        ha="left",
        va="top",
        fontsize=19.0,
        fontweight="bold",
        color=INK,
    )
    fig.text(
        0.035,
        0.895,
        "Model many possible cone sensitivities, then use one stimulus to separate observer classes.",
        ha="left",
        va="top",
        fontsize=10.5,
        color=MUTED,
    )

    draw_hyperobserver(axes[0])
    draw_classification(axes[1])

    left_box = axes[0].get_position()
    right_box = axes[1].get_position()
    fig.add_artist(FancyArrowPatch(
        (left_box.x1 + 0.012, left_box.y0 + left_box.height * 0.52),
        (right_box.x0 - 0.012, right_box.y0 + right_box.height * 0.52),
        transform=fig.transFigure,
        arrowstyle="-|>",
        mutation_scale=16,
        lw=1.2,
        color="#9aa0a6",
    ))

    fig.savefig(output_path, format="jpg", dpi=DPI, pil_kwargs={"quality": 95})
    plt.close(fig)
    return output_path


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate the representative image JPEG.")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=REPO_ROOT / "paper-viz" / "output" / "representative_image",
        help="Directory for generated representative image.",
    )
    args = parser.parse_args()

    output_path = make_figure(args.output_dir)
    print(f"Wrote representative image to {output_path}")


if __name__ == "__main__":
    main()
