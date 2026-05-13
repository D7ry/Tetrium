#!/usr/bin/env python3
"""
Generate a compact null-space fingerprint chart.

Columns are hyperobserver cone labels. Rows are null-space tests. Each cell is:
  x = nulled cone response
  o = non-zero

Usage:
    python paper-viz/null_fingerprint_chart.py
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

REPO_ROOT = Path(__file__).resolve().parents[1]
TETRIUM_ROOT = REPO_ROOT / "extern" / "TetriumColor"
sys.path.insert(0, str(TETRIUM_ROOT))

from TetriumColor.Plotting.PlotStyle import (  # noqa: E402
    ALL_PEAKS,
    DOUBLE_COL,
    PAPER_LIGHT_GRAY,
    PAPER_NEUTRAL,
    PAPER_RED,
    PAPER_YELLOW,
    apply_style,
)

OUTPUT_STEM = "null_fingerprint_chart"
QUEST_COLOR = PAPER_YELLOW
NEUTRAL_COLOR = PAPER_NEUTRAL
LABEL_FONT_SIZE = 6
CENSORED_COLOR = PAPER_RED
SUMMARY_CSV = REPO_ROOT / "data" / "quest_mocs_subject_summary" / "quest_mocs_compare_grid.csv"
THRESHOLD_CRITERION = 0.625
THRESHOLD_SUBJECT = "chris-5-7"

FINGERPRINTS = np.array(
    [
        [0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0],
        [0, 0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1],
        [0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0],
    ],
    dtype=int,
)

ROW_LABELS = [
    "(530, 559)",
    "(530, 555)",
    "(533, 559)",
]

def _fmt_peak(peak: float) -> str:
    return f"{peak:.0f}" if float(peak).is_integer() else f"{peak:g}"


def _label_key(label: str) -> str:
    return ",".join(part.strip() for part in label.strip("()").split(",") if part.strip())


def _load_threshold_data(summary_csv: Path, subject: str) -> tuple[np.ndarray, np.ndarray]:
    if not summary_csv.exists():
        raise FileNotFoundError(
            f"{summary_csv} does not exist. Run summarize_quest_mocs_subjects.py first."
        )
    df = pd.read_csv(summary_csv)
    df = df[(df["method"] == "Quest") & (df["subject"] == subject)].copy()
    if "threshold_criterion" in df.columns:
        criteria = pd.to_numeric(df["threshold_criterion"], errors="coerce")
        df = df[np.isclose(criteria, THRESHOLD_CRITERION)].copy()
    if df.empty:
        raise ValueError(f"No Quest rows at criterion {THRESHOLD_CRITERION:g} for {subject} in {summary_csv}.")

    df["threshold_value"] = pd.to_numeric(df["threshold"], errors="coerce")
    df["threshold_raw_value"] = pd.to_numeric(df["threshold_raw"], errors="coerce")
    df["is_censored"] = df["threshold_censored"].astype(str).str.lower().eq("true")
    by_label = {str(row.genotype_label): row for row in df.itertuples(index=False)}

    thresholds = []
    censored = []
    for label in ROW_LABELS:
        key = _label_key(label)
        row = by_label.get(key)
        if row is None:
            thresholds.append(np.nan)
            censored.append(False)
            continue
        thresholds.append(float(row.threshold_value))
        raw_censored = np.isfinite(row.threshold_raw_value) and float(row.threshold_raw_value) > 1.0
        censored.append(bool(row.is_censored) or raw_censored)
    return np.asarray(thresholds, dtype=float), np.asarray(censored, dtype=bool)


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


def draw_chart(ax) -> None:
    n_rows, n_cols = FINGERPRINTS.shape
    ax.set_xlim(-0.5, n_cols - 0.5)
    ax.set_ylim(n_rows - 0.5, -1.58)

    for row in range(n_rows):
        for col in range(n_cols):
            detectable = FINGERPRINTS[row, col] == 1
            face = "#f9d6d5" if not detectable else PAPER_LIGHT_GRAY
            edge = "#d98984" if not detectable else "#d6d6d6"
            ax.add_patch(
                plt.Rectangle(
                    (col - 0.5, row - 0.5),
                    1,
                    1,
                    facecolor=face,
                    edgecolor=edge,
                    linewidth=0.45,
                )
            )
            ax.text(
                col,
                row,
                "o" if detectable else "x",
                ha="center",
                va="center",
                color="#777777" if detectable else "#9c1f1f",
                fontsize=LABEL_FONT_SIZE,
                fontfamily="monospace",
                fontweight="bold" if not detectable else "normal",
            )

    for col, label in enumerate(_cone_labels()):
        ax.text(col, -1.28, label, ha="center", va="center", fontsize=LABEL_FONT_SIZE, rotation=45)

    for row, label in enumerate(ROW_LABELS):
        ax.text(-0.82, row, label, ha="right", va="center", fontsize=LABEL_FONT_SIZE)

    ax.set_axis_off()


def draw_response_panel(ax) -> None:
    x = np.arange(len(ROW_LABELS))
    highlight_idx = int(np.nanargmax(CHRIS_QUEST_THRESHOLDS_RAW))

    ax.plot(
        x,
        CHRIS_QUEST_THRESHOLDS,
        color=QUEST_COLOR,
        linewidth=0.85,
        alpha=0.65,
        zorder=1,
    )
    ax.scatter(
        x,
        CHRIS_QUEST_THRESHOLDS,
        s=18,
        color=QUEST_COLOR,
        edgecolor=QUEST_COLOR,
        linewidth=0.4,
        alpha=0.28,
        zorder=2,
    )
    ax.scatter(
        [x[highlight_idx]],
        [CHRIS_QUEST_THRESHOLDS[highlight_idx]],
        s=42,
        marker="^",
        color=CENSORED_COLOR,
        edgecolor=CENSORED_COLOR,
        linewidth=0.5,
        zorder=3,
    )

    ax.set_ylabel("Detection\nthreshold")
    ax.set_xticks(x)
    ax.set_xticklabels(ROW_LABELS)
    ax.set_xlim(-0.35, len(ROW_LABELS) - 0.65)
    ax.set_ylim(0, 1.16)
    ax.set_yticks([0, 0.5, 1.0])
    ax.grid(axis="y", alpha=0.25, linewidth=0.45)
    ax.grid(axis="x", alpha=0.16, linewidth=0.35)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.spines["left"].set_linewidth(0.55)
    ax.spines["bottom"].set_linewidth(0.55)
    ax.tick_params(axis="both", labelsize=LABEL_FONT_SIZE, width=0.55, length=2.2, pad=1.5)


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
    fig, axes = plt.subplots(
        2,
        1,
        figsize=(DOUBLE_COL / 3, 1.75),
        constrained_layout=True,
        gridspec_kw={"height_ratios": [0.95, 0.88]},
    )
    fig.set_constrained_layout_pads(hspace=0.11)
    draw_chart(axes[0])
    draw_response_panel(axes[1])
    fig.text(
        0.5,
        0.012,
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
    parser = argparse.ArgumentParser(description="Generate a null-space fingerprint chart.")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=REPO_ROOT / "paper-viz" / "output" / "null_fingerprint_chart",
        help="Directory for generated chart.",
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
    print(f"Wrote null-space fingerprint chart to {args.output_dir}")


if __name__ == "__main__":
    main()
