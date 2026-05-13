#!/usr/bin/env python3
"""
Plot hyperobserver cone spectral sensitivities.

Outputs separate S, M, and L cone-family plots, plus one combined plot.

Usage:
    python paper-viz/plot_hyperobserver.py
    python paper-viz/plot_hyperobserver.py --output-dir paper-viz/output/hyperobserver_cones
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import matplotlib.pyplot as plt

REPO_ROOT = Path(__file__).resolve().parents[1]
TETRIUM_ROOT = REPO_ROOT / "extern" / "TetriumColor"
sys.path.insert(0, str(TETRIUM_ROOT))

from TetriumColor.Observer.Observer import Cone, Observer  # noqa: E402
from TetriumColor.Plotting.PlotStyle import (  # noqa: E402
    ALL_PEAKS,
    DOUBLE_COL,
    L_PEAKS,
    M_PEAKS,
    PAPER_CONE_FAMILY_COLORS,
    WAVELENGTHS,
    apply_style,
    build_observers,
)

S_PEAKS = [ALL_PEAKS[0]]
FIG_WIDTH = DOUBLE_COL / 3
FAMILY_HEIGHT_RATIO = 0.95
COMBINED_HEIGHT_RATIO = 0.8
SHORT_FAMILY_HEIGHT_RATIO = 0.85
SHORT_COMBINED_HEIGHT_RATIO = 0.6
STOCKMAN_SHARPE_PEAKS = {"S": 420, "M": 530, "L": 559}
STOCKMAN_SHARPE_LINESTYLES = {"S": ":", "M": "--", "L": "-."}

FAMILY_COLORS = PAPER_CONE_FAMILY_COLORS


def _peak_label(peak: float) -> str:
    return f"{peak:.0f} nm" if peak == int(peak) else f"{peak:g} nm"


def _family_for_peak(peak: float) -> str:
    if peak in S_PEAKS:
        return "S"
    if peak in M_PEAKS:
        return "M"
    if peak in L_PEAKS:
        return "L"
    raise ValueError(f"Unexpected hyperobserver cone peak: {peak}")


def _color_for_peak(peak: float) -> str:
    family = _family_for_peak(peak)
    family_peaks = {"S": S_PEAKS, "M": M_PEAKS, "L": L_PEAKS}[family]
    idx = family_peaks.index(peak)
    return FAMILY_COLORS[family][idx]


def _build_stockman_sharpe_observer() -> Observer:
    return Observer(
        [
            Cone.s_cone(WAVELENGTHS, template=None),
            Cone.m_cone(WAVELENGTHS, template=None),
            Cone.l_cone(WAVELENGTHS, template=None),
        ],
        illuminant="raw",
    )


def _plot_cones(
    ax,
    wavelengths,
    sensor_matrix,
    peaks,
    title: str,
    stockman_sharpe_matrix,
    stockman_sharpe_families,
    split_legend: bool = False,
    compact_legend: bool = False,
) -> None:
    variant_lines = []
    for peak in peaks:
        idx = ALL_PEAKS.index(peak)
        line, = ax.plot(
            wavelengths,
            sensor_matrix[idx],
            color=_color_for_peak(peak),
            label=_peak_label(peak),
            linewidth=1.25,
            alpha=0.82,
        )
        variant_lines.append(line)

    stockman_sharpe_lines = []
    for family in stockman_sharpe_families:
        ss_idx = ["S", "M", "L"].index(family)
        line, = ax.plot(
            wavelengths,
            stockman_sharpe_matrix[ss_idx],
            color="black",
            linestyle=STOCKMAN_SHARPE_LINESTYLES[family],
            linewidth=1.0,
            label=f"SS-{family}",
        )
        stockman_sharpe_lines.append(line)

    ax.set_xlabel("Wavelength (nm)")
    ax.set_ylabel("Spectral sensitivity")
    ax.set_xlim(wavelengths[0], wavelengths[-1])
    ax.set_ylim(bottom=0)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    if split_legend:
        if compact_legend:
            ax.legend(
                handles=variant_lines + stockman_sharpe_lines,
                frameon=False,
                loc="upper center",
                bbox_to_anchor=(0.4, -0.5),
                bbox_transform=ax.transAxes,
                ncol=5,
                columnspacing=0.45,
                handlelength=1.1,
                borderaxespad=0.0,
                alignment="center",
            )
            return
        variant_legend = ax.legend(
            handles=variant_lines,
            frameon=False,
            loc="upper center",
            bbox_to_anchor=(0.5, -0.30),
            bbox_transform=ax.transAxes,
            ncol=4,
            columnspacing=0.7,
            handlelength=1.3,
            borderaxespad=0.0,
            alignment="center",
        )
        ax.add_artist(variant_legend)
        ax.legend(
            handles=stockman_sharpe_lines,
            frameon=False,
            loc="upper center",
            bbox_to_anchor=(0.5, -0.68),
            bbox_transform=ax.transAxes,
            ncol=3,
            columnspacing=0.7,
            handlelength=1.4,
            borderaxespad=0.0,
            alignment="center",
        )
    else:
        ax.legend(
            frameon=False,
            loc="upper center",
            bbox_to_anchor=(0.5, -0.30),
            bbox_transform=ax.transAxes,
            ncol=2,
            columnspacing=0.8,
            handlelength=1.3,
            borderaxespad=0.0,
            alignment="center",
        )


def _save_figure(fig, output_dir: Path, stem: str, formats: list[str]) -> None:
    for fmt in formats:
        fig.savefig(output_dir / f"{stem}.{fmt}", facecolor="white")
    plt.close(fig)


def make_plots(output_dir: Path, formats: list[str], shorter: bool = False) -> None:
    apply_style()
    plt.rcParams.update(
        {
            "font.size": 6,
            "axes.labelsize": 6,
            "axes.titlesize": 6,
            "xtick.labelsize": 6,
            "ytick.labelsize": 6,
            "legend.fontsize": 6,
        }
    )

    output_dir.mkdir(parents=True, exist_ok=True)
    observer, nominal_peaks = build_observers(WAVELENGTHS)["Hyperobserver"]
    sensor_matrix = observer.get_sensor_matrix(WAVELENGTHS)
    stockman_sharpe = _build_stockman_sharpe_observer()
    stockman_sharpe_matrix = stockman_sharpe.get_sensor_matrix(WAVELENGTHS)

    family_specs = [
        ("s", "S Cone", S_PEAKS),
        ("m", "M Cone Variants", M_PEAKS),
        ("l", "L Cone Variants", L_PEAKS),
    ]
    family_height_ratio = SHORT_FAMILY_HEIGHT_RATIO if shorter else FAMILY_HEIGHT_RATIO
    combined_height_ratio = SHORT_COMBINED_HEIGHT_RATIO if shorter else COMBINED_HEIGHT_RATIO

    for stem, title, peaks in family_specs:
        fig, ax = plt.subplots(figsize=(FIG_WIDTH, FIG_WIDTH * family_height_ratio))
        family = stem.upper()
        _plot_cones(
            ax,
            WAVELENGTHS,
            sensor_matrix,
            peaks,
            title,
            stockman_sharpe_matrix,
            [family],
        )
        fig.subplots_adjust(left=0.22, right=0.96, bottom=0.46, top=0.97)
        _save_figure(fig, output_dir, f"hyperobserver_{stem}_cones", formats)

    fig, ax = plt.subplots(figsize=(FIG_WIDTH, FIG_WIDTH * combined_height_ratio))
    _plot_cones(
        ax,
        WAVELENGTHS,
        sensor_matrix,
        nominal_peaks,
        "Hyperobserver Cone Sensitivities",
        stockman_sharpe_matrix,
        ["S", "M", "L"],
        split_legend=True,
        compact_legend=shorter,
    )
    combined_bottom = 0.50 if shorter else 0.44
    fig.subplots_adjust(left=0.22, right=0.96, bottom=combined_bottom, top=0.97)
    _save_figure(fig, output_dir, "hyperobserver_all_cones", formats)


def main() -> None:
    parser = argparse.ArgumentParser(description="Plot hyperobserver cone spectral sensitivities.")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=REPO_ROOT / "paper-viz" / "output" / "hyperobserver_cones",
        help="Directory for generated plots.",
    )
    parser.add_argument(
        "--formats",
        nargs="+",
        default=["png", "pdf"],
        choices=["png", "pdf", "svg"],
        help="Output formats.",
    )
    parser.add_argument(
        "--shorter",
        action="store_true",
        help="Use shorter figure heights while preserving the default layout otherwise.",
    )
    args = parser.parse_args()

    make_plots(args.output_dir, args.formats, shorter=args.shorter)
    print(f"Wrote hyperobserver cone sensitivity plots to {args.output_dir}")


if __name__ == "__main__":
    main()
