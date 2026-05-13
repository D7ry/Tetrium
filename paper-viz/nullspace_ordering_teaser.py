#!/usr/bin/env python3
"""
Generate a 1 x 4 Seaborn-style figure comparing null-space test ordering.

Panels:
  A. All candidate null-space fingerprints
  B. Random ordering with model-predicted observer compatibility
  C. Genetic-prior ordering with compatibility and measured thresholds
  D. Trichromatic genetic-prior CDF

Usage:
    python paper-viz/nullspace_ordering_teaser.py
"""

from __future__ import annotations

import argparse
import csv
import itertools
import sys
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import seaborn as sns
from matplotlib.colors import ListedColormap

REPO_ROOT = Path(__file__).resolve().parents[1]
TETRIUM_ROOT = REPO_ROOT / "extern" / "TetriumColor"
sys.path.insert(0, str(TETRIUM_ROOT))

from TetriumColor.Observer.ObserverGenotypes import ObserverGenotypes  # noqa: E402
from TetriumColor.Plotting.PlotStyle import (  # noqa: E402
    ALL_PEAKS,
    DOUBLE_COL,
    PAPER_BLUE,
    PAPER_DIMENSION_COLORS,
    PAPER_LIGHT_GRAY,
    PAPER_NEUTRAL,
    PAPER_RED,
    PAPER_YELLOW,
    apply_style,
)

OUTPUT_STEM = "nullspace_ordering_teaser"
LABEL_FONT_SIZE = 6
TOP_N = 10
CDF_DISPLAY_N = 15
RANDOM_SEED = 7
SUMMARY_THRESHOLDS = REPO_ROOT / "data" / "quest_mocs_subject_summary" / "quest_mocs_compare_grid.csv"
THRESHOLD_CRITERION = 0.625
OMITTED_THRESHOLD_SUBJECTS = {"will-5-8"}
COMPATIBILITY_ROWS = 3

OBSERVER_ROWS = [
    ("Obs. 1", (420.0, 530.0, 559.0)),
    ("Obs. 2", (420.0, 533.0, 559.0)),
]


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


def _all_candidate_tests() -> list[tuple[float, float, float]]:
    return [tuple(combo) for combo in itertools.combinations([float(p) for p in ALL_PEAKS], 3)]


def _fingerprint_for_test(test: tuple[float, ...]) -> np.ndarray:
    nulled = set(test)
    return np.array([0 if float(peak) in nulled else 1 for peak in ALL_PEAKS], dtype=int)


def _matrix_for_tests(tests: list[tuple[float, ...]]) -> np.ndarray:
    return np.vstack([_fingerprint_for_test(test) for test in tests])


def _test_label(test: tuple[float, ...]) -> str:
    ml = [peak for peak in test if peak != 420.0]
    return "(" + ",".join(_fmt_peak(p) for p in ml) + ")"


def _is_failed(test: tuple[float, ...], observer: tuple[float, ...]) -> bool:
    return set(observer).issubset(set(test))


def _trichromat_prior() -> list[tuple[tuple[float, ...], float]]:
    genotypes = ObserverGenotypes(dimensions=[1, 2, 3, 4, 5])
    items = []
    for genotype, prob in genotypes.get_pdf("both").items():
        if len(genotype) == 2:
            test = tuple(sorted((420.0, *map(float, genotype))))
            items.append((test, float(prob)))
    total = sum(prob for _test, prob in items)
    return [(test, prob / total) for test, prob in items if total > 0]


def _measured_threshold_order(
    prior: list[tuple[tuple[float, ...], float]],
    thresholds: dict[tuple[float, ...], float],
) -> list[tuple[float, ...]]:
    prior_rank = {test: idx for idx, (test, _prob) in enumerate(prior)}
    measured = [test for test, _prob in prior if test in thresholds]
    measured.sort(key=lambda test: (thresholds[test], prior_rank[test]))
    if len(measured) >= TOP_N:
        return measured[:TOP_N]
    fallback = [test for test, _prob in prior if test not in set(measured)]
    return (measured + fallback)[:TOP_N]


def _threshold_test_from_row(row: dict[str, str]) -> tuple[float, ...] | None:
    genotype_key = row.get("genotype_key", "")
    if not genotype_key:
        genotype_key = row.get("genotype", "")
    if not genotype_key:
        return None
    genotype = tuple(
        sorted(
            float(part)
            for part in genotype_key.split(",")
            if part and abs(float(part) - 547.0) > 1e-6
        )
    )
    if len(genotype) != 2:
        return None
    return tuple(sorted((420.0, *genotype)))


def _load_measured_thresholds() -> dict[tuple[float, ...], float]:
    if not SUMMARY_THRESHOLDS.exists():
        return {}
    thresholds_by_test = defaultdict(list)
    with SUMMARY_THRESHOLDS.open(newline="") as f:
        for row in csv.DictReader(f):
            if row.get("subject") in OMITTED_THRESHOLD_SUBJECTS:
                continue
            if row.get("method") != "Quest":
                continue
            if row.get("threshold_criterion"):
                try:
                    if not np.isclose(float(row["threshold_criterion"]), THRESHOLD_CRITERION):
                        continue
                except ValueError:
                    continue
            test = _threshold_test_from_row(row)
            if test is None:
                continue
            try:
                threshold = float(row["threshold"])
            except (TypeError, ValueError):
                continue
            if np.isfinite(threshold):
                thresholds_by_test[test].append(threshold)
    return {
        test: float(np.median(values))
        for test, values in thresholds_by_test.items()
        if values
    }


def _draw_xo_grid(ax, matrix: np.ndarray, row_labels: list[str] | None = None) -> None:
    n_rows, n_cols = matrix.shape
    ax.set_xlim(-0.5, n_cols - 0.5)
    ax.set_ylim(n_rows - 0.5, -1.45)

    for row in range(n_rows):
        for col in range(n_cols):
            nonzero = matrix[row, col] == 1
            ax.add_patch(
                plt.Rectangle(
                    (col - 0.5, row - 0.5),
                    1,
                    1,
                    facecolor=PAPER_LIGHT_GRAY if nonzero else "#f9d6d5",
                    edgecolor="#d6d6d6" if nonzero else "#d98984",
                    linewidth=0.35,
                )
            )
            ax.text(
                col,
                row,
                "o" if nonzero else "x",
                ha="center",
                va="center",
                color="#777777" if nonzero else "#9c1f1f",
                fontsize=LABEL_FONT_SIZE,
                fontfamily="monospace",
                fontweight="bold" if not nonzero else "normal",
            )

    for col, label in enumerate(_cone_labels()):
        ax.text(col, -1.12, label, ha="center", va="center", fontsize=LABEL_FONT_SIZE, rotation=45)
    if row_labels is not None:
        for row, label in enumerate(row_labels):
            ax.text(-0.82, row, label, ha="right", va="center", fontsize=LABEL_FONT_SIZE)
    ax.set_axis_off()


def _draw_barcode_panel(ax, tests: list[tuple[float, ...]]) -> None:
    n_tests = len(tests)
    n_cones = len(ALL_PEAKS)
    total_rows = n_cones + COMPATIBILITY_ROWS
    matrix = _matrix_for_tests(tests).T
    cmap = ListedColormap(["#f9d6d5", PAPER_LIGHT_GRAY])

    sns.heatmap(
        matrix,
        ax=ax,
        cmap=cmap,
        cbar=False,
        linewidths=0.35,
        linecolor="#d6d6d6",
        xticklabels=False,
        yticklabels=_cone_labels(),
        vmin=0,
        vmax=1,
    )
    ax.set_xlim(0, n_tests)
    ax.set_ylim(total_rows, 0)

    pass_color = PAPER_DIMENSION_COLORS[2]
    fail_color = PAPER_RED
    for row_offset, (observer_label, observer) in enumerate(OBSERVER_ROWS):
        y = n_cones + row_offset
        ax.text(-0.25, y + 0.5, observer_label, ha="right", va="center", fontsize=LABEL_FONT_SIZE)
        for col, test in enumerate(tests):
            fail = _is_failed(test, observer)
            ax.add_patch(
                plt.Rectangle(
                    (col, y),
                    1,
                    1,
                    facecolor=fail_color if fail else pass_color,
                    edgecolor="white",
                    linewidth=0.35,
                    alpha=0.82 if fail else 0.58,
                )
            )
            ax.text(
                col + 0.5,
                y + 0.5,
                "x" if fail else "o",
                ha="center",
                va="center",
                fontsize=LABEL_FONT_SIZE,
                color="white" if fail else "#1f1f1f",
                fontfamily="monospace",
                fontweight="bold" if fail else "normal",
            )

    for spine in ax.spines.values():
        spine.set_visible(False)
    ax.tick_params(left=False, bottom=False, labelleft=True, labelbottom=False, labelsize=LABEL_FONT_SIZE, pad=1.5)
    ax.set_yticklabels(_cone_labels(), rotation=0, va="center")


def _draw_candidate_panel(ax, tests: list[tuple[float, ...]]) -> None:
    matrix = _matrix_for_tests(tests).T
    n_cones = len(ALL_PEAKS)
    total_rows = n_cones + COMPATIBILITY_ROWS
    cmap = ListedColormap(["#f9d6d5", PAPER_LIGHT_GRAY])
    sns.heatmap(
        matrix,
        ax=ax,
        cmap=cmap,
        cbar=False,
        xticklabels=False,
        yticklabels=_cone_labels(),
        linewidths=0,
        vmin=0,
        vmax=1,
    )
    ax.set_ylim(total_rows, 0)
    ax.set_title("A. Candidate Subsets", pad=3)
    bracket_y = len(ALL_PEAKS) + 0.28
    ax.plot([0, len(tests)], [bracket_y, bracket_y], color=PAPER_NEUTRAL, linewidth=0.6, clip_on=False)
    ax.plot([0, 0], [bracket_y - 0.22, bracket_y], color=PAPER_NEUTRAL, linewidth=0.6, clip_on=False)
    ax.plot([len(tests), len(tests)], [bracket_y - 0.22, bracket_y], color=PAPER_NEUTRAL, linewidth=0.6, clip_on=False)
    ax.text(
        len(tests) * 0.5,
        len(ALL_PEAKS) + 0.92,
        r"$\binom{12}{3}=220$ candidate subsets",
        ha="center",
        va="center",
        fontsize=LABEL_FONT_SIZE,
    )
    ax.set_xlabel("")
    ax.tick_params(axis="y", labelsize=LABEL_FONT_SIZE, length=0, pad=1.5, labelrotation=0)
    ax.set_yticklabels(_cone_labels(), rotation=0, va="center")
    sns.despine(ax=ax, left=True, bottom=True)


def _draw_cdf_panel(ax, prior: list[tuple[tuple[float, ...], float]]) -> None:
    probs = np.array([prob for _, prob in prior], dtype=float)
    ranks = np.arange(1, len(probs) + 1)
    cdf = np.cumsum(probs)
    top = min(CDF_DISPLAY_N, len(prior))
    y = np.arange(top)
    ax.barh(
        y,
        probs[:top],
        height=0.72,
        color=PAPER_DIMENSION_COLORS[2],
        edgecolor="black",
        linewidth=0.3,
        alpha=0.65,
        zorder=2,
    )
    ax_top = ax.twiny()
    ax_top.plot(cdf[:top], y, color=PAPER_NEUTRAL, lw=0.85, marker="o", markersize=2.0, alpha=0.88, zorder=3)
    top10_coverage = float(cdf[TOP_N - 1]) if len(cdf) >= TOP_N else float(cdf[-1])
    ax.axhline(TOP_N - 0.5, color=PAPER_RED, linestyle="--", linewidth=0.65, alpha=0.8)
    ax_top.axvline(0.99, color=PAPER_BLUE, linestyle=":", linewidth=0.65, alpha=0.8)

    labels = [_test_label(test) for test, _prob in prior[:top]]
    ax.set_yticks(y)
    ax.set_yticklabels(labels, fontsize=LABEL_FONT_SIZE)
    ax.invert_yaxis()
    ax.set_xlim(0, max(probs[:top]) * 1.22)
    ax_top.set_xlim(0, max(0.08, cdf[:top].max() * 1.08))
    ax.set_xlabel("Prob.")
    ax_top.set_xlabel("CDF")
    ax.set_ylabel("Ranked genotype")
    ax.set_title("D. Trichromatic CDF", pad=3)
    ax.text(
        0.04,
        0.04,
        "Top 10: 99.3% of trichromatic population",
        transform=ax.transAxes,
        ha="left",
        va="bottom",
        fontsize=LABEL_FONT_SIZE,
    )
    ax.grid(axis="x", alpha=0.25, linewidth=0.45)
    ax.grid(axis="y", visible=False)
    ax_top.grid(False)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax_top.spines["right"].set_visible(False)
    ax.tick_params(axis="both", labelsize=LABEL_FONT_SIZE, width=0.55, length=2.2, pad=1.5)
    ax_top.tick_params(axis="x", labelsize=LABEL_FONT_SIZE, width=0.55, length=2.2, pad=1.5)
    ax.set_box_aspect(1.497)
    ax_top.set_box_aspect(1.497)
    ax.set_anchor("C")
    ax_top.set_anchor("C")

def make_figure(output_dir: Path, formats: list[str]) -> None:
    apply_style()
    sns.set_style("ticks")
    plt.rcParams.update(
        {
            "figure.facecolor": "white",
            "axes.facecolor": "white",
            "savefig.facecolor": "white",
            "savefig.dpi": 450,
            "font.size": LABEL_FONT_SIZE,
            "axes.labelsize": LABEL_FONT_SIZE,
            "axes.titlesize": 8,
            "xtick.labelsize": LABEL_FONT_SIZE,
            "ytick.labelsize": LABEL_FONT_SIZE,
        }
    )

    output_dir.mkdir(parents=True, exist_ok=True)
    all_tests = _all_candidate_tests()
    rng = np.random.default_rng(RANDOM_SEED)
    random_tests = [all_tests[idx] for idx in rng.choice(len(all_tests), size=TOP_N, replace=False)]
    prior = _trichromat_prior()
    measured_thresholds = _load_measured_thresholds()
    prior_tests = _measured_threshold_order(prior, measured_thresholds)

    fig, axes = plt.subplots(
        1,
        4,
        figsize=(DOUBLE_COL, 2.45),
        constrained_layout=True,
        gridspec_kw={"width_ratios": [1.28, 0.90, 0.98, 0.84]},
    )
    fig.set_constrained_layout_pads(w_pad=0.015, h_pad=0.02, wspace=0.03, hspace=0.02)

    _draw_candidate_panel(axes[0], all_tests)
    axes[1].set_title("B. Random Ordering", pad=3)
    _draw_barcode_panel(axes[1], random_tests)
    axes[2].set_title("C. Measured Threshold Ordering", pad=3)
    _draw_barcode_panel(axes[2], prior_tests)
    _draw_cdf_panel(axes[3], prior)
    fig.text(
        0.525,
        0.03,
        # "Obs. 1 (530,559)    Obs. 2 (533,559)
        "o = non-zero response x = nulled cone responses",
        ha="center",
        va="center",
        fontsize=LABEL_FONT_SIZE,
        bbox={"facecolor": "white", "edgecolor": PAPER_NEUTRAL, "linewidth": 0.5, "pad": 2.0, "alpha": 0.78},
    )

    for fmt in formats:
        fig.savefig(output_dir / f"{OUTPUT_STEM}.{fmt}", bbox_inches="tight")
    plt.close(fig)


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate a null-space ordering teaser figure.")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=REPO_ROOT / "paper-viz" / "output" / "nullspace_ordering_teaser",
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

    make_figure(args.output_dir, args.formats)
    print(f"Wrote null-space ordering teaser to {args.output_dir}")


if __name__ == "__main__":
    main()
