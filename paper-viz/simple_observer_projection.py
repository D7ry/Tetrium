#!/usr/bin/env python3
"""
Make a simple 2 x 4 paper figure:

Rows:
  1. Spectra targeted to LMS peaks (420, 530, 559)
  2. Spectra targeted to LMS peaks (420, 530, 533)

Columns:
  1. The two generated spectra
  2. Raw cone-excitation difference projected onto the target LMS observer
  3. Raw cone-excitation difference projected onto a matching 4-cone LMSQ observer
  4. Raw cone-excitation difference projected onto the full hyperobserver
"""

from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

REPO_ROOT = Path(__file__).resolve().parents[1]
TETRIUM_ROOT = REPO_ROOT / "extern" / "TetriumColor"
sys.path.insert(0, str(TETRIUM_ROOT))

from TetriumColor.ColorSpace import ColorSpace
from TetriumColor.Measurement import load_primaries_from_csv
from TetriumColor.Observer import Observer, Spectra
from TetriumColor.Observer.ObserverGenotypes import ObserverGenotypes
from TetriumColor.Plotting.PlotStyle import COLORS, DOUBLE_COL, apply_style

TARGETS = [(530, 559), (530, 533)]
LMS_PROJECTION_TARGET = (530, 559)
LMSQ_PROJECTION_TARGET = (530, 547, 559)
LMSQ_DISPLAY_ORDER = [420, 530, 547, 559]
HYPER_PEAKS = [420, 530, 533, 536, 547, 551, 552, 553, 555, 556, 556.5, 559]
M1_COLOR = "#2166ac"
M2_COLOR = "#b2182b"
DIFF_COLOR = "#2166ac"


def _fmt_peak(peak: float) -> str:
    return str(int(peak)) if float(peak).is_integer() else str(peak)


def _display_matrix(primaries: list[Spectra], wavelengths: np.ndarray) -> np.ndarray:
    matrix = np.zeros((len(wavelengths), len(primaries)))
    for idx, primary in enumerate(primaries):
        matrix[:, idx] = primary.interpolate_values(wavelengths).data
    return matrix


def _metamer_pair_for_observer(
    color_space: ColorSpace,
    primaries: list[Spectra],
    proportion: float,
) -> tuple[np.ndarray, np.ndarray, Spectra, Spectra]:
    wavelengths = primaries[0].wavelengths
    base = np.full(len(primaries), 0.5)

    direction = _raw_cone_excitation_null_direction_in_disp(
        color_space,
        color_space.metameric_axis,
    )
    step_limits = np.where(np.abs(direction) > 1e-15, 0.5 / np.abs(direction), np.inf)
    step = float(np.min(step_limits)) * proportion

    weights_1 = np.clip(base + step * direction, 0, 1)
    weights_2 = np.clip(base - step * direction, 0, 1)

    spectrum_1 = Spectra(
        wavelengths=wavelengths,
        data=sum(weight * primary.data for weight, primary in zip(weights_1, primaries)),
        normalized=False,
    )
    spectrum_2 = Spectra(
        wavelengths=wavelengths,
        data=sum(weight * primary.data for weight, primary in zip(weights_2, primaries)),
        normalized=False,
    )
    return weights_1, weights_2, spectrum_1, spectrum_2


def _raw_cone_excitation_null_direction_in_disp(
    color_space: ColorSpace,
    metameric_axis: int,
) -> np.ndarray:
    excitation_matrix = color_space.get_raw_display_to_cone_matrix()
    nulled = np.delete(excitation_matrix, metameric_axis, axis=0)
    _, _, vh = np.linalg.svd(nulled)
    direction = vh[-1]
    if excitation_matrix[metameric_axis] @ direction < 0:
        direction = -direction
    return direction / np.linalg.norm(direction)


def _raw_cone_diff(
    observer: Observer,
    primaries: list[Spectra],
    weights_1: np.ndarray,
    weights_2: np.ndarray,
) -> np.ndarray:
    display = _display_matrix(primaries, observer.wavelengths)
    raw_display_to_cone = observer.sensor_matrix @ display
    return np.abs(raw_display_to_cone @ weights_1 - raw_display_to_cone @ weights_2)


def _draw_spectra(ax, spectra: tuple[Spectra, Spectra], title: str, show_xlabel: bool) -> None:
    spectrum_1, spectrum_2 = spectra
    ax.plot(spectrum_1.wavelengths, spectrum_1.data, color=M1_COLOR, lw=1.0, label="M1")
    ax.plot(spectrum_2.wavelengths, spectrum_2.data, color=M2_COLOR, lw=1.0, label="M2")
    ax.set_title(title)
    ax.set_xlim(400, 700)
    ax.set_ylim(bottom=0)
    ax.set_ylabel("Power")
    if show_xlabel:
        ax.set_xlabel("Wavelength (nm)")
    ax.grid(True, alpha=0.25, linewidth=0.4)


def _draw_projection(
    ax,
    values: np.ndarray,
    labels: list[str],
    title: str,
    show_xlabel: bool,
) -> None:
    x = np.arange(len(labels))
    ax.bar(x, values, width=0.64, color=DIFF_COLOR, alpha=0.45, edgecolor=DIFF_COLOR, linewidth=0.5)
    ax.set_title(title)
    ax.set_xticks(x)
    ax.set_xticklabels(labels, rotation=45, ha="right", rotation_mode="anchor")
    ax.set_ylim(bottom=0)
    ax.set_ylabel(r"$|\Delta$ raw cone$|$")
    if show_xlabel:
        ax.set_xlabel("Cone peak")
    ax.grid(True, axis="y", alpha=0.25, linewidth=0.4)


def _configure_style() -> None:
    apply_style()
    if shutil.which("latex") is None:
        plt.rcParams.update({
            "text.usetex": False,
            "font.family": "sans-serif",
        })


def make_figure(
    primaries_path: str,
    plots_dir: str,
    output_name: str,
    proportion: float,
) -> None:
    primaries = load_primaries_from_csv(primaries_path, extract_zero=False, primary_order="BGOR")
    wavelengths = primaries[0].wavelengths
    genotypes = ObserverGenotypes(wavelengths=wavelengths, dimensions=[3], template="baylor")
    hyperobserver = Observer.hyperobserver(
        wavelengths=wavelengths,
        template="baylor",
        illuminant="raw",
        degree=2.0,
    )

    _configure_style()

    fig, axes = plt.subplots(2, 4, figsize=(DOUBLE_COL, DOUBLE_COL * 0.34))

    legend_handles = None
    legend_labels = None
    projection_axes = []
    max_contrast_diff = 0.0
    for row, target in enumerate(TARGETS):
        show_xlabel = row == len(TARGETS) - 1

        target_genotype = tuple(sorted(set(target + (547,))))
        target_observer = genotypes.get_observer_for_peaks(
            target_genotype,
            degree=2.0,
            illuminant="raw",
            template="baylor",
        )
        target_peaks = sorted((420,) + target_genotype)
        target_color_space = ColorSpace(
            target_observer,
            display_primaries=primaries,
            metameric_axis=target_peaks.index(547),
        )
        lms_observer = genotypes.get_observer_for_peaks(
            LMS_PROJECTION_TARGET,
            degree=2.0,
            illuminant="raw",
            template="baylor",
        )
        lmsq_observer = genotypes.get_observer_for_peaks(
            LMSQ_PROJECTION_TARGET,
            degree=2.0,
            illuminant="raw",
            template="baylor",
        )

        weights_1, weights_2, spectrum_1, spectrum_2 = _metamer_pair_for_observer(
            target_color_space,
            primaries,
            proportion,
        )
        spectra = (spectrum_1, spectrum_2)

        lms_diff = _raw_cone_diff(lms_observer, primaries, weights_1, weights_2)
        lmsq_diff = _raw_cone_diff(lmsq_observer, primaries, weights_1, weights_2)
        lmsq_sorted_peaks = sorted((420,) + LMSQ_PROJECTION_TARGET)
        lmsq_display_indices = [lmsq_sorted_peaks.index(peak) for peak in LMSQ_DISPLAY_ORDER]
        lmsq_diff = lmsq_diff[lmsq_display_indices]
        hyper_diff = _raw_cone_diff(hyperobserver, primaries, weights_1, weights_2)
        max_contrast_diff = max(
            max_contrast_diff,
            float(np.max(lms_diff)),
            float(np.max(lmsq_diff)),
            float(np.max(hyper_diff)),
        )

        row_label = f"Target ({', '.join(_fmt_peak(p) for p in target)})"
        axes[row, 0].set_ylabel(f"{row_label}\nPower")

        _draw_spectra(
            axes[row, 0],
            spectra,
            "Spectra" if row == 0 else "",
            show_xlabel,
        )
        axes[row, 0].set_ylabel(f"{row_label}\nPower")

        lms_labels = ["S"] + [_fmt_peak(peak) for peak in LMS_PROJECTION_TARGET]
        _draw_projection(
            axes[row, 1],
            lms_diff,
            lms_labels,
            "LMS diff" if row == 0 else "",
            show_xlabel,
        )
        projection_axes.append(axes[row, 1])

        lmsq_labels = [_fmt_peak(peak) for peak in LMSQ_DISPLAY_ORDER]
        _draw_projection(
            axes[row, 2],
            lmsq_diff,
            lmsq_labels,
            "LMSQ diff" if row == 0 else "",
            show_xlabel,
        )
        projection_axes.append(axes[row, 2])

        hyper_labels = [_fmt_peak(peak) for peak in HYPER_PEAKS]
        _draw_projection(
            axes[row, 3],
            hyper_diff,
            hyper_labels,
            "Hyperobserver diff" if row == 0 else "",
            show_xlabel,
        )
        projection_axes.append(axes[row, 3])

        if legend_handles is None:
            legend_handles, legend_labels = axes[row, 0].get_legend_handles_labels()

    shared_ylim = max_contrast_diff * 1.08 if max_contrast_diff > 0 else 1.0
    for ax in projection_axes:
        ax.set_ylim(0, shared_ylim)

    if legend_handles:
        fig.legend(
            legend_handles,
            legend_labels,
            loc="upper center",
            ncol=2,
            frameon=False,
            bbox_to_anchor=(0.5, 1.02),
        )

    plots_path = Path(plots_dir)
    plots_path.mkdir(parents=True, exist_ok=True)
    png = plots_path / f"{output_name}.png"
    pdf = plots_path / f"{output_name}.pdf"
    plt.tight_layout(rect=(0, 0, 1, 0.94))
    plt.savefig(png, dpi=300, bbox_inches="tight", facecolor="white")
    plt.savefig(pdf, bbox_inches="tight", facecolor="white")
    plt.close()
    print(f"Saved: {png}")
    print(f"Saved: {pdf}")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Create a simple 2x4 spectra/projection figure.",
    )
    parser.add_argument("--primaries", required=True, help="Path to display primaries in BGOR order.")
    parser.add_argument("--plots-dir", default="paper-viz/output", help="Directory for output figures.")
    parser.add_argument("--output-name", default="simple_observer_projection", help="Output filename stem.")
    parser.add_argument("--proportion", type=float, default=0.8, help="Fraction of maximal null-space step.")
    args = parser.parse_args()

    make_figure(
        primaries_path=args.primaries,
        plots_dir=args.plots_dir,
        output_name=args.output_name,
        proportion=args.proportion,
    )


if __name__ == "__main__":
    main()
