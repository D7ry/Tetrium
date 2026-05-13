#!/usr/bin/env python3
"""Generate a tiny measured-null-fingerprint schematic fragment."""

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

from TetriumColor.Plotting.PlotStyle import PAPER_RED, SINGLE_COL, apply_style  # noqa: E402

OUTPUT_STEM = "nullprint_fragment_schematic"
LABEL_FONT_SIZE = 6
SUMMARY_CSV = REPO_ROOT / "data" / "quest_mocs_subject_summary" / "quest_mocs_compare_grid.csv"
SUBJECT_ORDER = [
    "jingyi-5-8",
    "lauren-5-6",
    "jess-5-5",
    "hannah-5-7",
    "james-5-7",
    "chris-5-7",
    "will-5-8",
    "atsu-5-8-3",
    "ben-5-8-2",
]
TOP_10_OBSERVERS = [
    "(530,559)",
    "(530,555)",
    "(533,559)",
    "(555,559)",
    "(533,555)",
    "(530,556)",
    "(530,556.5)",
    "(530,551)",
    "(530,552)",
    "(530,533)",
]


def _load_subject_nullprint(subject_id: int, summary_csv: Path) -> tuple[str, np.ndarray, np.ndarray]:
    if subject_id < 1 or subject_id > len(SUBJECT_ORDER):
        raise ValueError(f"--subject-id must be between 1 and {len(SUBJECT_ORDER)}.")
    subject_key = SUBJECT_ORDER[subject_id - 1]
    df = pd.read_csv(summary_csv)
    df = df[(df["method"] == "Quest") & (df["subject"] == subject_key)].copy()
    if df.empty:
        raise ValueError(f"No Quest rows found for Subject {subject_id} ({subject_key}) in {summary_csv}.")

    values = np.full((1, len(TOP_10_OBSERVERS)), np.nan, dtype=float)
    null_mask = np.zeros((1, len(TOP_10_OBSERVERS)), dtype=bool)
    df["threshold_value"] = pd.to_numeric(df["threshold"], errors="coerce")
    df["threshold_raw_value"] = pd.to_numeric(df["threshold_raw"], errors="coerce")
    df["is_censored"] = df["threshold_censored"].astype(str).str.lower().eq("true")
    by_label = {f"({row.genotype_label})": row for row in df.itertuples(index=False)}

    for col, label in enumerate(TOP_10_OBSERVERS):
        row = by_label.get(label)
        if row is None:
            continue
        values[0, col] = float(row.threshold_value)
        null_mask[0, col] = bool(row.is_censored) or float(row.threshold_raw_value) > 1.0
    return f"S{subject_id}", values, null_mask


def make_figure(output_dir: Path, formats: list[str], subject_id: int, summary_csv: Path) -> None:
    apply_style()
    plt.rcParams.update(
        {
            "figure.facecolor": "white",
            "axes.facecolor": "white",
            "savefig.facecolor": "white",
            "savefig.dpi": 450,
            "font.size": LABEL_FONT_SIZE,
            "axes.labelsize": LABEL_FONT_SIZE,
            "xtick.labelsize": LABEL_FONT_SIZE,
            "ytick.labelsize": LABEL_FONT_SIZE,
        }
    )

    subject_label, values, null_mask = _load_subject_nullprint(subject_id, summary_csv)

    fig, ax = plt.subplots(figsize=(SINGLE_COL, 0.86), constrained_layout=True)
    ax.set_xlim(-0.5, values.shape[1] - 0.5)
    ax.set_ylim(values.shape[0] - 0.5, -0.5)

    for row in range(values.shape[0]):
        for col in range(values.shape[1]):
            value = values[row, col]
            facecolor = PAPER_RED if null_mask[row, col] else plt.cm.YlOrBr(0.08 + 0.50 * value)
            ax.add_patch(
                plt.Rectangle(
                    (col - 0.5, row - 0.5),
                    1,
                    1,
                    facecolor=facecolor,
                    edgecolor="white",
                    linewidth=0.55,
                    alpha=0.92 if null_mask[row, col] else 0.52 + 0.35 * value,
                )
            )
            if null_mask[row, col]:
                ax.plot(
                    col,
                    row,
                    marker="^",
                    markersize=5.0,
                    markerfacecolor=PAPER_RED,
                    markeredgecolor="white",
                    markeredgewidth=0.45,
                    linestyle="None",
                )

    ax.set_xticks(range(values.shape[1]))
    ax.set_xticklabels(TOP_10_OBSERVERS, rotation=45, ha="left")
    ax.xaxis.tick_top()
    ax.set_yticks(range(values.shape[0]))
    ax.set_yticklabels([subject_label])
    ax.tick_params(axis="both", length=0, pad=1.5, labelsize=LABEL_FONT_SIZE)
    for spine in ax.spines.values():
        spine.set_visible(False)

    output_dir.mkdir(parents=True, exist_ok=True)
    for fmt in formats:
        fig.savefig(output_dir / f"{OUTPUT_STEM}.{fmt}", bbox_inches="tight")
    plt.close(fig)


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate a tiny nullprint fragment schematic.")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=REPO_ROOT / "paper-viz" / "output" / OUTPUT_STEM,
    )
    parser.add_argument("--formats", nargs="+", default=["pdf", "png"], choices=["pdf", "png", "svg"])
    parser.add_argument("--subject-id", type=int, default=6, help="1-indexed subject ID from the paper subject order.")
    parser.add_argument("--summary-csv", type=Path, default=SUMMARY_CSV, help="CSV emitted by summarize_quest_mocs_subjects.py.")
    args = parser.parse_args()
    make_figure(args.output_dir, args.formats, args.subject_id, args.summary_csv)
    print(f"Wrote nullprint fragment schematic to {args.output_dir}")


if __name__ == "__main__":
    main()
