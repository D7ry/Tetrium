#!/usr/bin/env python3
"""
Generate a cone-colored schematic vector with the trichromat components opaque.

Usage:
    python paper-viz/cone_null_vector_highlight.py
    python paper-viz/cone_null_vector_highlight.py --output-dir paper-viz/output/cone_null_vector_highlight
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle

REPO_ROOT = Path(__file__).resolve().parents[1]
TETRIUM_ROOT = REPO_ROOT / "extern" / "TetriumColor"
sys.path.insert(0, str(TETRIUM_ROOT))

from TetriumColor.Plotting.PlotStyle import (  # noqa: E402
    ALL_PEAKS,
    COLORS,
    DOUBLE_COL,
    apply_style,
)

OUTPUT_STEM = "cone_null_vector_highlight"
OPAQUE_PEAKS = {420, 530, 559}
INACTIVE_ALPHA = 0.12
ACTIVE_ALPHA = 0.96
CELL_EDGE = "#d6d6d6"
LABEL_FONT_SIZE = 8


def _fmt_peak(peak: float) -> str:
    return f"{peak:.0f}" if float(peak).is_integer() else f"{peak:g}"


def _cone_label(peak: float) -> str:
    if peak == 420:
        return "S"
    if peak < 540:
        return f"M{_fmt_peak(peak)}"
    return f"L{_fmt_peak(peak)}"


def _draw_vector(ax) -> None:
    n_entries = len(ALL_PEAKS)
    ax.set_xlim(-0.5, n_entries - 0.5)
    ax.set_ylim(-0.95, 0.95)

    for idx, peak in enumerate(ALL_PEAKS):
        is_active = peak in OPAQUE_PEAKS
        alpha = ACTIVE_ALPHA if is_active else INACTIVE_ALPHA
        ax.add_patch(
            Rectangle(
                (idx - 0.5, -0.5),
                1,
                1,
                facecolor=COLORS[peak],
                edgecolor=CELL_EDGE,
                linewidth=0.45,
                alpha=alpha,
            )
        )
        ax.text(
            idx,
            -0.55,
            _cone_label(peak),
            ha="center",
            va="top",
            fontsize=LABEL_FONT_SIZE,
            color="#202020" if is_active else "#8a8a8a",
            rotation=45,
        )

    ax.set_axis_off()


def make_plot(output_dir: Path, formats: list[str], dpi: int) -> None:
    if dpi < 300:
        raise ValueError("dpi must be at least 300")

    apply_style()
    plt.rcParams.update(
        {
            "figure.facecolor": "white",
            "axes.facecolor": "white",
            "savefig.facecolor": "white",
            "font.size": LABEL_FONT_SIZE,
            "axes.labelsize": LABEL_FONT_SIZE,
            "axes.titlesize": LABEL_FONT_SIZE,
            "xtick.labelsize": LABEL_FONT_SIZE,
            "ytick.labelsize": LABEL_FONT_SIZE,
            "axes.linewidth": 0.6,
            "savefig.dpi": dpi,
        }
    )

    output_dir.mkdir(parents=True, exist_ok=True)
    fig, ax = plt.subplots(figsize=(DOUBLE_COL * 0.72, DOUBLE_COL * 0.18))
    _draw_vector(ax)
    fig.subplots_adjust(left=0.02, right=0.99, bottom=0.30, top=0.90)

    for fmt in formats:
        fig.savefig(output_dir / f"{OUTPUT_STEM}.{fmt}", facecolor="white", dpi=dpi)
    plt.close(fig)


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate a cone-colored null-vector schematic.")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=REPO_ROOT / "paper-viz" / "output" / OUTPUT_STEM,
        help="Directory for generated figure.",
    )
    parser.add_argument(
        "--formats",
        nargs="+",
        default=["png", "pdf"],
        choices=["png", "pdf", "svg"],
        help="Output formats.",
    )
    parser.add_argument("--dpi", type=int, default=300, help="DPI for saved figures; defaults to 300.")
    args = parser.parse_args()

    if args.dpi < 300:
        parser.error("--dpi must be at least 300")

    make_plot(args.output_dir, args.formats, args.dpi)
    print(f"Wrote cone null-vector highlight to {args.output_dir}")


if __name__ == "__main__":
    main()
