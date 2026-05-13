#!/usr/bin/env python3
"""
Conceptual comparison between silent substitution and null-space detection.

The figure uses the same x/o fingerprint language as null_fingerprint_chart.py:
  x = nulled cone response
  o = non-zero

Usage:
    python paper-viz/silent_vs_nullspace_concept.py
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

REPO_ROOT = Path(__file__).resolve().parents[1]
TETRIUM_ROOT = REPO_ROOT / "extern" / "TetriumColor"
sys.path.insert(0, str(TETRIUM_ROOT))

from TetriumColor.Plotting.PlotStyle import (  # noqa: E402
    ALL_PEAKS,
    PAPER_BLUE,
    PAPER_LIGHT_GRAY,
    PAPER_NEUTRAL,
    PAPER_RED,
    SINGLE_COL,
    apply_style,
)

OUTPUT_STEM = "silent_vs_nullspace_concept"
LABEL_FONT_SIZE = 8

SILENT_SUBSTITUTION = np.array(
    [
        [0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
        [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1],
    ],
    dtype=int,
)
SILENT_LABELS = ["Isolate M530", "Isolate L559"]

NULL_SPACE = np.array(
    [
        [0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0],
        [0, 0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1],
        [0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0],
    ],
    dtype=int,
)
NULL_LABELS = ["Null (530,559)", "Null (530,555)", "Null (533,559)"]


def _fmt_peak(peak: float) -> str:
    return f"{peak:.0f}" if float(peak).is_integer() else f"{peak:g}"


def _cone_labels() -> list[str]:
    labels = []
    for peak in ALL_PEAKS:
        if peak == ALL_PEAKS[0]:
            labels.append("S")
        elif peak < 540:
            labels.append(f"M{_fmt_peak(peak)}")
        else:
            labels.append(f"L{_fmt_peak(peak)}")
    return labels


def _draw_cell(ax, col: int, row: int, nonzero: bool, color: str, emphasize: str) -> None:
    if emphasize == "o":
        face = "#e9f0f8" if nonzero else "#f9d6d5"
        edge = "#aebfd3" if nonzero else "#d98984"
        alpha = 0.98 if nonzero else 0.98
        text_color = color if nonzero else "#9c1f1f"
        text_alpha = 1.0
        weight = "bold" if nonzero else "normal"
    else:
        face = PAPER_LIGHT_GRAY if nonzero else "#f9d6d5"
        edge = "#d6d6d6" if nonzero else "#d98984"
        alpha = 0.30 if nonzero else 0.98
        text_color = "#777777" if nonzero else "#9c1f1f"
        text_alpha = 0.42 if nonzero else 1.0
        weight = "normal" if nonzero else "bold"

    ax.add_patch(
        plt.Rectangle(
            (col - 0.5, row - 0.5),
            1,
            1,
            facecolor=face,
            edgecolor=edge,
            linewidth=0.45,
            alpha=alpha,
        )
    )
    ax.text(
        col,
        row,
        "o" if nonzero else "x",
        ha="center",
        va="center",
        color=text_color,
        alpha=text_alpha,
        fontsize=LABEL_FONT_SIZE,
        fontfamily="monospace",
        fontweight=weight,
    )


def _draw_fingerprint(ax) -> None:
    matrix = np.vstack([SILENT_SUBSTITUTION, NULL_SPACE])
    row_labels = SILENT_LABELS + NULL_LABELS
    split_y = len(SILENT_LABELS) - 0.5
    n_rows, n_cols = matrix.shape
    ax.set_xlim(-0.5, n_cols - 0.5)
    ax.set_ylim(n_rows - 0.5, -1.72)

    for row in range(n_rows):
        for col in range(n_cols):
            nonzero = matrix[row, col] == 1
            if row < len(SILENT_LABELS):
                _draw_cell(ax, col, row, nonzero, PAPER_BLUE, "o")
            else:
                _draw_cell(ax, col, row, nonzero, PAPER_RED, "x")

    for col, label in enumerate(_cone_labels()):
        ax.text(col, -1.35, label, ha="center", va="center", fontsize=LABEL_FONT_SIZE, rotation=45)

    for row, label in enumerate(row_labels):
        ax.text(-0.78, row, label, ha="right", va="center", fontsize=LABEL_FONT_SIZE)

    ax.plot(
        [-3.5, n_cols - 0.5],
        [split_y, split_y],
        color="#777777",
        linewidth=1.05,
        linestyle="--",
        alpha=0.88,
        clip_on=False,
    )
    ax.set_axis_off()


def make_figure(output_dir: Path, formats: list[str]) -> None:
    apply_style()
    plt.rcParams.update(
        {
            "figure.facecolor": "white",
            "axes.facecolor": "white",
            "savefig.facecolor": "white",
            "savefig.dpi": 450,
            "font.size": LABEL_FONT_SIZE,
            "axes.labelsize": LABEL_FONT_SIZE,
            "axes.titlesize": LABEL_FONT_SIZE,
            "xtick.labelsize": LABEL_FONT_SIZE,
            "ytick.labelsize": LABEL_FONT_SIZE,
        }
    )

    output_dir.mkdir(parents=True, exist_ok=True)
    fig, ax = plt.subplots(figsize=(SINGLE_COL, 1.85), constrained_layout=True)
    _draw_fingerprint(ax)
    fig.text(
        0.5,
        -0.04,
        "x = nulled cone response    o = non-zero response",
        ha="center",
        va="center",
        fontsize=LABEL_FONT_SIZE,
        bbox={"facecolor": "white", "edgecolor": PAPER_NEUTRAL, "linewidth": 0.5, "pad": 2.0, "alpha": 0.78},
    )

    for fmt in formats:
        fig.savefig(output_dir / f"{OUTPUT_STEM}.{fmt}", bbox_inches="tight")
    plt.close(fig)


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate a silent-substitution vs null-space concept figure.")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=REPO_ROOT / "paper-viz" / "output" / "silent_vs_nullspace_concept",
        help="Directory for generated concept figure.",
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
    print(f"Wrote silent-substitution vs null-space concept figure to {args.output_dir}")


if __name__ == "__main__":
    main()
