#!/usr/bin/env python3
"""
Plot null-space equivalence classes for Quest subjects.

The figure uses the subject order from the multi-subject Quest summary command
and classifies each subject by the candidate observers whose Quest null-space
test was censored above the available gamut.

Usage:
    python paper-viz/subject_equivalence_classes.py
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

from TetriumColor.Observer.ObserverGenotypes import ObserverGenotypes  # noqa: E402
from TetriumColor.Plotting.PlotStyle import (  # noqa: E402
    PAPER_LIGHT_GRAY,
    PAPER_RED,
    SINGLE_COL,
    apply_style,
)

OUTPUT_STEM = "subject_equivalence_classes"
SUMMARY_CSV = REPO_ROOT / "data" / "quest_mocs_subject_summary" / "quest_mocs_compare_grid.csv"
THRESHOLD_CRITERION = 0.625
TOP_N = 10
LABEL_FONT_SIZE = 6
TITLE_FONT_SIZE = 8

SUBJECT_ORDER = [
    "jingyi-5-8",
    "lauren-5-6",
    "jess-5-5",
    "hannah-5-7",
    "james-5-7",
    "chris-5-7",
    "atsu-5-8-3",
    "ben-5-8-2",
]


def _fmt_peak(peak: float | str) -> str:
    peak = float(peak)
    return f"{peak:.0f}" if peak.is_integer() else f"{peak:g}"


def _test_label(genotype_label: str) -> str:
    peaks = [_fmt_peak(part) for part in str(genotype_label).split(",") if part]
    return "(" + ",".join(peaks) + ")"


def _trichromat_prior_order(top_n: int = TOP_N) -> list[str]:
    genotypes = ObserverGenotypes(dimensions=[1, 2, 3, 4, 5])
    items = []
    for genotype, prob in genotypes.get_pdf("both").items():
        if len(genotype) == 2:
            key = ",".join(_fmt_peak(peak) for peak in genotype)
            items.append((key, float(prob)))
    total = sum(prob for _key, prob in items)
    items = [(key, prob / total) for key, prob in items if total > 0]
    items.sort(key=lambda item: item[1], reverse=True)
    return [key for key, _prob in items[:top_n]]


def _load_quest_summary(summary_csv: Path) -> pd.DataFrame:
    if not summary_csv.exists():
        raise FileNotFoundError(
            f"{summary_csv} does not exist. Run summarize_quest_mocs_subjects.py first."
        )

    df = pd.read_csv(summary_csv)
    df = df[(df["method"] == "Quest") & (df["subject"].isin(SUBJECT_ORDER))].copy()
    if "threshold_criterion" in df.columns:
        criteria = pd.to_numeric(df["threshold_criterion"], errors="coerce")
        df = df[np.isclose(criteria, THRESHOLD_CRITERION)].copy()
    if df.empty:
        raise ValueError(
            f"No Quest rows at criterion {THRESHOLD_CRITERION:g} found for requested subjects in {summary_csv}."
        )

    subject_rank = {subject: idx for idx, subject in enumerate(SUBJECT_ORDER)}
    df["subject_rank"] = df["subject"].map(subject_rank)
    df["threshold_value"] = pd.to_numeric(df["threshold"], errors="coerce")
    df["threshold_raw_value"] = pd.to_numeric(df["threshold_raw"], errors="coerce")
    df["is_censored"] = df["threshold_censored"].astype(str).str.lower().eq("true")
    return df.sort_values(["subject_rank", "genotype_label"])


def _build_subject_matrix(
    df: pd.DataFrame,
    test_order: list[str],
) -> tuple[np.ndarray, np.ndarray, list[str]]:
    subjects = [subject for subject in SUBJECT_ORDER if subject in set(df["subject"])]
    thresholds = np.full((len(subjects), len(test_order)), np.nan, dtype=float)
    censored = np.zeros((len(subjects), len(test_order)), dtype=bool)
    subject_index = {subject: idx for idx, subject in enumerate(subjects)}
    test_index = {test: idx for idx, test in enumerate(test_order)}

    for row in df.itertuples(index=False):
        if row.genotype_label not in test_index:
            continue
        r = subject_index[row.subject]
        c = test_index[row.genotype_label]
        thresholds[r, c] = float(row.threshold_value)
        censored[r, c] = bool(row.is_censored) or float(row.threshold_raw_value) > 1.0

    return thresholds, censored, subjects


def _classification_sets(mask: np.ndarray, test_order: list[str]) -> list[list[str]]:
    return [[_test_label(test_order[col]) for col in np.flatnonzero(row)] for row in mask]


def _wrap_classification(labels: list[str]) -> str:
    if not labels:
        return r"$\{\}$"
    if len(labels) <= 2:
        body = ", ".join(labels)
    else:
        body = ", ".join(labels[:2]) + rf", +{len(labels) - 2}"
    return rf"$\{{{body}\}}$"


def _draw_threshold_grid(
    ax: plt.Axes,
    thresholds: np.ndarray,
    censored: np.ndarray,
    test_order: list[str],
    subjects: list[str],
) -> None:
    n_subjects, n_tests = thresholds.shape
    ax.set_xlim(-0.5, n_tests - 0.5)
    ax.set_ylim(n_subjects - 0.5, -0.5)

    for row in range(n_subjects):
        for col in range(n_tests):
            value = thresholds[row, col]
            if censored[row, col]:
                color = PAPER_RED
                alpha = 0.92
            elif np.isfinite(value):
                clipped = float(np.clip(value, 0.0, 1.0))
                color = plt.cm.YlOrBr(0.08 + 0.50 * clipped)
                alpha = 0.50 + 0.38 * clipped
            else:
                color = PAPER_LIGHT_GRAY
                alpha = 1.0
            ax.add_patch(
                plt.Rectangle(
                    (col - 0.5, row - 0.5),
                    1,
                    1,
                    facecolor=color,
                    edgecolor="white",
                    linewidth=0.45,
                    alpha=alpha,
                )
            )
            if censored[row, col]:
                ax.plot(
                    col,
                    row,
                    marker="^",
                    markersize=4.6,
                    markerfacecolor=PAPER_RED,
                    markeredgecolor="white",
                    markeredgewidth=0.35,
                    linestyle="None",
                    clip_on=False,
                )
            if (not censored[row, col]) and np.isfinite(value):
                ax.text(
                    col,
                    row,
                    f"{value:.2f}",
                    ha="center",
                    va="center",
                    fontsize=LABEL_FONT_SIZE,
                    color="#2c2c2c",
                )

    ax.set_xticks(range(n_tests))
    ax.set_xticklabels([_test_label(test) for test in test_order], rotation=45, ha="left")
    ax.xaxis.tick_top()
    ax.tick_params(axis="x", length=0, pad=1.5, labelsize=5.5)
    ax.set_yticks(range(n_subjects))
    ax.set_yticklabels([str(SUBJECT_ORDER.index(subject) + 1) for subject in subjects])
    ax.tick_params(axis="y", length=0, labelsize=LABEL_FONT_SIZE, pad=2)
    ax.text(
        -0.5,
        -0.5,
        "Subject ID",
        ha="center",
        va="bottom",
        fontsize=LABEL_FONT_SIZE,
        rotation=45,
        clip_on=False,
    )
    ax.set_title("A. Measured Null Threshold Vectors", loc="left", pad=8, fontsize=TITLE_FONT_SIZE)
    for spine in ax.spines.values():
        spine.set_visible(False)


def _draw_class_size_axis(
    ax: plt.Axes,
    class_sets: list[list[str]],
) -> None:
    n_subjects = len(class_sets)
    ax.set_xlim(0.0, 1.0)
    ax.set_ylim(n_subjects - 0.5, -0.5)
    ax.text(0.5, -1.12, r"$|S^*|$", ha="center", va="center", fontsize=LABEL_FONT_SIZE, clip_on=False)
    for idx, labels in enumerate(class_sets):
        ax.text(
            0.5,
            idx,
            str(len(labels)),
            ha="center",
            va="center",
            fontsize=LABEL_FONT_SIZE,
        )
    ax.set_axis_off()


def _draw_example_axis(
    ax: plt.Axes,
    class_sets: list[list[str]],
) -> None:
    classifications = [
        ("Broad", "S1, S3, S4, S5"),
        ("Narrow", "S2, S6"),
        ("Unclassifiable", "S7"),
        ("Protanomalous", "S8")
        ,
    ]
    ax.set_xlim(0.0, 1.0)
    ax.set_ylim(0.0, 1.0)
    ax.text(0.0, 0.86, "B. Null-Threshold Vector Classes ", ha="left", va="center", fontsize=TITLE_FONT_SIZE)
    y_positions = [0.58, 0.38, 0.18, -0.02]
    for (label, subjects), y in zip(classifications, y_positions):
        ax.text(
            0.0,
            y,
            rf"{label}: {subjects}",
            ha="left",
            va="center",
            fontsize=LABEL_FONT_SIZE,
        )
    ax.set_axis_off()


def make_figure(summary_csv: Path, output_dir: Path, formats: list[str]) -> None:
    apply_style()
    plt.rcParams.update(
        {
            "figure.facecolor": "white",
            "axes.facecolor": "white",
            "savefig.facecolor": "white",
            "savefig.dpi": 450,
            "font.size": LABEL_FONT_SIZE,
            "axes.labelsize": LABEL_FONT_SIZE,
            "axes.titlesize": TITLE_FONT_SIZE,
            "xtick.labelsize": LABEL_FONT_SIZE,
            "ytick.labelsize": LABEL_FONT_SIZE,
        }
    )

    output_dir.mkdir(parents=True, exist_ok=True)
    df = _load_quest_summary(summary_csv)
    test_order = _trichromat_prior_order(TOP_N)
    thresholds, censored, subjects = _build_subject_matrix(df, test_order)
    class_sets = _classification_sets(censored, test_order)

    fig = plt.figure(
        figsize=(SINGLE_COL, 3.55),
        constrained_layout=True,
    )
    gs = fig.add_gridspec(
        2,
        2,
        width_ratios=[1.0, 0.095],
        height_ratios=[1.0, 0.22],
    )
    grid_ax = fig.add_subplot(gs[0, 0])
    size_ax = fig.add_subplot(gs[0, 1], sharey=grid_ax)
    example_ax = fig.add_subplot(gs[1, :])
    fig.set_constrained_layout_pads(w_pad=0.01, h_pad=0.01, wspace=0.01, hspace=0.015)

    _draw_threshold_grid(grid_ax, thresholds, censored, test_order, subjects)
    _draw_class_size_axis(size_ax, class_sets)
    _draw_example_axis(example_ax, class_sets)

    for fmt in formats:
        fig.savefig(output_dir / f"{OUTPUT_STEM}.{fmt}", bbox_inches="tight")
    plt.close(fig)


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate subject equivalence-class figure.")
    parser.add_argument(
        "--summary-csv",
        type=Path,
        default=SUMMARY_CSV,
        help="CSV emitted by summarize_quest_mocs_subjects.py.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=REPO_ROOT / "paper-viz" / "output" / OUTPUT_STEM,
        help="Directory for generated figure.",
    )
    parser.add_argument(
        "--formats",
        nargs="+",
        default=["pdf", "png"],
        choices=["pdf", "png", "svg"],
        help="Output formats.",
    )
    args = parser.parse_args()

    make_figure(args.summary_csv, args.output_dir, args.formats)
    print(f"Wrote subject equivalence-class figure to {args.output_dir}")


if __name__ == "__main__":
    main()
