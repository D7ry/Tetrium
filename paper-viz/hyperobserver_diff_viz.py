#!/usr/bin/env python3
"""
Create paper-style metamer spectra and observer-difference plots.

Outputs four figures by default:
  1. LMS subset difference bars, using the matching hyperobserver cones.
  2. LMSQ subset difference bars, using the matching hyperobserver cones.
  3. Full hyperobserver difference bars.
  4. Spectra plus LMS / LMSQ / hyperobserver differences.

The plotting style and data path are intentionally close to
extern/TetriumColor/scripts/validation/visualize_metamer_summary.py.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.gridspec import GridSpec

REPO_ROOT = Path(__file__).resolve().parents[1]
TETRIUM_ROOT = REPO_ROOT / "extern" / "TetriumColor"
sys.path.insert(0, str(TETRIUM_ROOT))

from TetriumColor.ColorSpace import ColorSpace, ColorSpaceType
from TetriumColor.Measurement import get_spectras_from_rgbo_list, load_primaries_from_csv
from TetriumColor.Observer import Observer, Spectra
from TetriumColor.Observer.ObserverGenotypes import ObserverGenotypes
from TetriumColor.Plotting.PlotStyle import DOUBLE_COL, apply_style

VALIDATION_OBSERVER_DEGREE = 2.0

HYPER_PEAKS = [420, 530, 533, 536, 547, 551, 552, 553, 555, 556, 556.5, 559]
HYPER_CONE_LABELS = [
    "420", "530", "533", "536",
    "547", "551", "552", "553",
    "555", "556", "556.5", "559",
]


def _observer_sort_key(obs_data: dict) -> tuple[float, int, tuple]:
    return (
        -float(obs_data.get("probability", 0.0)),
        int(obs_data.get("observer_index", 0)),
        tuple(obs_data.get("genotype", [])),
    )


def _fmt_peak(peak: float) -> str:
    return str(int(peak)) if float(peak).is_integer() else str(peak)


def _metamer_to_bgor(metamer: dict, color_space: ColorSpace, suffix: str) -> tuple[np.ndarray, np.ndarray]:
    raw_key = f"raw_cone_{suffix}"
    cone_key = f"cone_{suffix}"
    if raw_key in metamer:
        raw_cone = np.asarray(metamer[raw_key], dtype=float)
        bgor = np.linalg.solve(color_space.get_raw_display_to_cone_matrix(), raw_cone)
        return bgor, raw_cone

    cone = np.asarray(metamer[cone_key], dtype=float)
    bgor = color_space.convert(cone.reshape(1, -1), ColorSpaceType.CONE, ColorSpaceType.DISP)[0]
    return bgor, cone


def _pick_metamer(obs_data: dict, pair_index: int | None, grid_position: list[int] | None) -> dict:
    metamers = obs_data["metamers"]
    if pair_index is not None:
        for metamer in metamers:
            if int(metamer["pair_index"]) == pair_index:
                return metamer
    if grid_position is not None:
        for metamer in metamers:
            if metamer.get("grid_position") == grid_position:
                return metamer
    return metamers[len(metamers) // 2]


def _bgor_to_spectra(
    bgor_1: np.ndarray,
    bgor_2: np.ndarray,
    primaries: list[Spectra],
    scaling_factor: float,
) -> tuple[Spectra, Spectra, Spectra, Spectra, np.ndarray, np.ndarray]:
    wavelengths = primaries[0].wavelengths
    pred_1 = Spectra(
        wavelengths=wavelengths,
        data=scaling_factor * sum(w * p.data for w, p in zip(bgor_1, primaries)),
        normalized=False,
    )
    pred_2 = Spectra(
        wavelengths=wavelengths,
        data=scaling_factor * sum(w * p.data for w, p in zip(bgor_2, primaries)),
        normalized=False,
    )

    bgor_1_8bit = np.clip(np.round(bgor_1 * 255), 0, 255).astype(int)
    bgor_2_8bit = np.clip(np.round(bgor_2 * 255), 0, 255).astype(int)
    bgor_1_r = bgor_1_8bit / 255.0
    bgor_2_r = bgor_2_8bit / 255.0
    pred_1_rounded = Spectra(
        wavelengths=wavelengths,
        data=scaling_factor * sum(w * p.data for w, p in zip(bgor_1_r, primaries)),
        normalized=False,
    )
    pred_2_rounded = Spectra(
        wavelengths=wavelengths,
        data=scaling_factor * sum(w * p.data for w, p in zip(bgor_2_r, primaries)),
        normalized=False,
    )
    return pred_1, pred_2, pred_1_rounded, pred_2_rounded, bgor_1_8bit, bgor_2_8bit


def _load_measured_spectra(
    measurements_dir: str | None,
    bgor_1_8bit: np.ndarray,
    bgor_2_8bit: np.ndarray,
    wavelengths: np.ndarray,
    scaling_factor: float,
) -> tuple[Spectra | None, Spectra | None]:
    if measurements_dir is None:
        return None, None

    rgbo_1 = (int(bgor_1_8bit[3]), int(bgor_1_8bit[1]), int(bgor_1_8bit[0]), int(bgor_1_8bit[2]))
    rgbo_2 = (int(bgor_2_8bit[3]), int(bgor_2_8bit[1]), int(bgor_2_8bit[0]), int(bgor_2_8bit[2]))
    try:
        measured = get_spectras_from_rgbo_list(measurements_dir, [rgbo_1, rgbo_2], smooth_method="gaussian")
    except Exception as exc:
        print(f"  WARNING: could not load measurements for {rgbo_1} / {rgbo_2}: {exc}")
        return None, None

    return (
        Spectra(
            wavelengths=wavelengths,
            data=scaling_factor * measured[0].interpolate_values(wavelengths).data,
            normalized=False,
        ),
        Spectra(
            wavelengths=wavelengths,
            data=scaling_factor * measured[1].interpolate_values(wavelengths).data,
            normalized=False,
        ),
    )


def _hyperobserver_diffs(hyperobs: Observer, od: dict) -> dict[str, np.ndarray | None]:
    pred_1 = hyperobs.observe_spectras([od["predicted_1"]])[0]
    pred_2 = hyperobs.observe_spectras([od["predicted_2"]])[0]
    pred_1_r = hyperobs.observe_spectras([od["predicted_1_rounded"]])[0]
    pred_2_r = hyperobs.observe_spectras([od["predicted_2_rounded"]])[0]

    diffs = {
        "pred": np.abs(pred_1 - pred_2),
        "pred_8bit": np.abs(pred_1_r - pred_2_r),
        "meas": None,
    }
    if od["measured_1"] is not None and od["measured_2"] is not None:
        meas_1 = hyperobs.observe_spectras([od["measured_1"]])[0]
        meas_2 = hyperobs.observe_spectras([od["measured_2"]])[0]
        diffs["meas"] = np.abs(meas_1 - meas_2)
    return diffs


def _subset_indices(genotype: tuple[float, ...], subset: str) -> tuple[list[int], list[str]]:
    peaks = sorted((420,) + tuple(genotype))
    if subset == "lms":
        peaks = [peaks[0], peaks[1], peaks[-1]]
        labels = ["S", "M", "L"]
    elif subset == "lmsq":
        labels = ["S", "M", "Q", "L"]
    else:
        raise ValueError(f"Unknown subset: {subset}")

    indices = [HYPER_PEAKS.index(peak) for peak in peaks]
    return indices, labels


def _draw_diff_bars(
    ax,
    diffs: dict[str, np.ndarray | None],
    indices: list[int],
    labels: list[str],
    title: str | None = None,
    show_ylabel: bool = False,
    show_xlabel: bool = False,
    show_legend: bool = False,
    highlight_indices: list[int] | None = None,
) -> None:
    x = np.arange(len(indices))
    width = 0.24
    pred = np.asarray(diffs["pred"])[indices]
    pred_8bit = np.asarray(diffs["pred_8bit"])[indices]
    meas = None if diffs.get("meas") is None else np.asarray(diffs["meas"])[indices]

    ax.bar(x - width, pred, width, color="steelblue", alpha=0.85,
           edgecolor="black", linewidth=0.3, label=r"Pred $|$M1$-$M2$|$")
    ax.bar(x, pred_8bit, width, color="steelblue", alpha=0.55,
           edgecolor="steelblue", linewidth=0.5, label=r"8-bit $|$M1$-$M2$|$")
    if meas is not None:
        ax.bar(x + width, meas, width, color="steelblue", alpha=0.3,
               edgecolor="steelblue", linewidth=0.5, label=r"Meas $|$M1$-$M2$|$")

    highlight_set = set(highlight_indices or indices)
    ax.set_xticks(x)
    ax.set_xticklabels(labels, rotation=45, ha="right", rotation_mode="anchor")
    for tick, idx in zip(ax.get_xticklabels(), indices):
        if idx in highlight_set:
            tick.set_fontweight("black")
            tick.set_color("black")
            tick.set_path_effects([pe.withStroke(linewidth=0.005, foreground="black")])
        else:
            tick.set_color("#555555")

    ax.set_ylim(bottom=0)
    ax.grid(True, alpha=0.3, axis="y", linestyle="--")
    ax.tick_params(axis="x", which="major", labelsize=5, length=1.0, width=0.3, pad=1)
    ax.tick_params(axis="y", which="major", labelsize=5, length=1.0, width=0.3, pad=1)
    ax.yaxis.set_major_locator(plt.MaxNLocator(3, prune="both"))
    if title:
        ax.set_title(title, fontsize=7, pad=2)
    if show_ylabel:
        ax.set_ylabel(r"$|\mathrm{M1} - \mathrm{M2}|$", fontsize=6, labelpad=1)
    if show_xlabel:
        ax.set_xlabel("Cone", fontsize=6, labelpad=1)
    if show_legend:
        ax.legend(fontsize=4, loc="upper right")


def _draw_spectra(ax, od: dict, label: str, show_xlabel: bool = False, show_legend: bool = False) -> None:
    wl = od["predicted_1"].wavelengths
    ax.plot(wl, od["predicted_1"].data, color="steelblue", linewidth=0.8, alpha=0.45, label="Pred M1")
    ax.plot(wl, od["predicted_2"].data, color="firebrick", linewidth=0.8, alpha=0.45, label="Pred M2")
    if od["measured_1"] is not None and od["measured_2"] is not None:
        ax.plot(wl, od["measured_1"].data, color="steelblue", linewidth=0.9,
                linestyle="--", alpha=0.95, label="Meas M1")
        ax.plot(wl, od["measured_2"].data, color="firebrick", linewidth=0.9,
                linestyle="--", alpha=0.95, label="Meas M2")

    ax.set_xlim(400, 700)
    ax.set_ylim(bottom=0)
    ax.tick_params(labelsize=5, pad=2)
    ax.yaxis.set_major_locator(plt.MaxNLocator(3, prune="both"))
    ax.grid(True, alpha=0.2, linewidth=0.4)
    ax.set_ylabel(f"({label})", fontsize=6, fontweight="bold", rotation=90, labelpad=2)
    if show_xlabel:
        ax.set_xlabel("Wavelength (nm)", fontsize=6, labelpad=1)
    else:
        ax.tick_params(labelbottom=False)
    if show_legend:
        ax.legend(fontsize=4, loc="lower right", handlelength=1.0, borderpad=0.3, labelspacing=0.2)


def _genotype_label(genotype: tuple[float, ...]) -> str:
    return ", ".join(_fmt_peak(peak) for peak in genotype)


def _prepare_observer_data(
    metamers_config_path: str,
    primaries_path: str,
    measurements_dir: str | None,
    n_observers: int,
    pair_index: int | None,
) -> tuple[list[dict], Observer]:
    print(f"Loading metamer config from: {metamers_config_path}")
    with open(metamers_config_path, "r") as handle:
        config = json.load(handle)

    print(f"Loading display primaries from: {primaries_path}")
    primaries = load_primaries_from_csv(primaries_path, extract_zero=False, primary_order="BGOR")
    wavelengths = primaries[0].wavelengths

    observer_genotypes = ObserverGenotypes(
        wavelengths=wavelengths,
        dimensions=[3],
        seed=config["metadata"].get("seed", 42),
        template="baylor",
    )
    metameric_axis = config["metadata"].get("metameric_axis", 2)

    hyperobs = Observer.hyperobserver(
        wavelengths=wavelengths,
        degree=VALIDATION_OBSERVER_DEGREE,
        illuminant="raw",
        template="baylor",
    )

    grid_size = config["metadata"].get("grid_size")
    grid_position = [grid_size // 2, grid_size // 2] if isinstance(grid_size, int) else None
    observers = sorted(config["observers"], key=_observer_sort_key)[:n_observers]

    observer_data = []
    for obs_data in observers:
        genotype = tuple(sorted(obs_data["genotype"]))
        observer = observer_genotypes.get_observer_for_peaks(
            genotype,
            degree=VALIDATION_OBSERVER_DEGREE,
            illuminant="raw",
            template="baylor",
        )
        color_space = ColorSpace(
            observer,
            display_primaries=primaries,
            metameric_axis=obs_data.get("metameric_axis", metameric_axis),
        )
        metamer = _pick_metamer(obs_data, pair_index, grid_position)
        bgor_1, _ = _metamer_to_bgor(metamer, color_space, "1")
        bgor_2, _ = _metamer_to_bgor(metamer, color_space, "2")

        spectra = _bgor_to_spectra(
            bgor_1,
            bgor_2,
            primaries,
            color_space._disp_metadata["scaling_factor"],
        )
        pred_1, pred_2, pred_1_rounded, pred_2_rounded, bgor_1_8bit, bgor_2_8bit = spectra
        meas_1, meas_2 = _load_measured_spectra(
            measurements_dir,
            bgor_1_8bit,
            bgor_2_8bit,
            wavelengths,
            color_space._disp_metadata["scaling_factor"],
        )

        print(f"  Observer {obs_data.get('observer_index')}: genotype=({_genotype_label(genotype)}) "
              f"pair={metamer['pair_index']}")
        observer_data.append({
            "genotype": genotype,
            "pair_index": metamer["pair_index"],
            "predicted_1": pred_1,
            "predicted_2": pred_2,
            "predicted_1_rounded": pred_1_rounded,
            "predicted_2_rounded": pred_2_rounded,
            "measured_1": meas_1,
            "measured_2": meas_2,
        })

    return observer_data, hyperobs


def _save(fig, plots_path: Path, stem: str) -> None:
    png = plots_path / f"{stem}.png"
    pdf = plots_path / f"{stem}.pdf"
    fig.savefig(png, dpi=600)
    fig.savefig(pdf, dpi=300)
    plt.close(fig)
    print(f"  Saved: {png}")
    print(f"  Saved: {pdf}")


def _add_legend(fig, ax, loc: str) -> None:
    handles, labels = ax.get_legend_handles_labels()
    if handles:
        fig.legend(handles, labels, loc=loc, ncol=len(handles), bbox_to_anchor=(0.99, 1.0),
                   fontsize=4, frameon=False, handlelength=1.0, handletextpad=0.4,
                   columnspacing=0.9)


def _make_diff_figure(observer_data: list[dict], hyperobs: Observer, plots_path: Path, variant: str) -> None:
    n_rows = len(observer_data)
    fig = plt.figure(figsize=(DOUBLE_COL * 0.72, max(2.4, 0.55 * n_rows)), layout="constrained")
    fig.get_layout_engine().set(h_pad=0.03, w_pad=0.03, hspace=0.05, wspace=0.03, rect=(0, 0, 1, 0.96))
    gs = GridSpec(n_rows, 1, figure=fig)

    axes = []
    for row, od in enumerate(observer_data):
        ax = fig.add_subplot(gs[row, 0], sharey=axes[0] if axes else None)
        axes.append(ax)
        diffs = _hyperobserver_diffs(hyperobs, od)

        if variant == "lms":
            indices, labels = _subset_indices(od["genotype"], "lms")
            title = "LMS subset" if row == 0 else None
            stem = "hyperobserver_diff_subset_lms"
        elif variant == "lmsq":
            indices, labels = _subset_indices(od["genotype"], "lmsq")
            title = "LMSQ subset" if row == 0 else None
            stem = "hyperobserver_diff_subset_lmsq"
        elif variant == "hyperobserver":
            indices = list(range(len(HYPER_PEAKS)))
            labels = HYPER_CONE_LABELS
            title = "Hyperobserver" if row == 0 else None
            stem = "hyperobserver_diff_full"
        else:
            raise ValueError(f"Unknown variant: {variant}")

        _draw_diff_bars(
            ax,
            diffs,
            indices,
            labels,
            title=title,
            show_ylabel=(row == n_rows // 2),
            show_xlabel=(row == n_rows - 1),
            show_legend=(row == 0),
        )
        ax.text(-0.02, 0.5, f"({_genotype_label(od['genotype'])})", transform=ax.transAxes,
                fontsize=6, fontweight="bold", rotation=90, va="center", ha="right")

    _save(fig, plots_path, stem)


def _make_combined_figure(observer_data: list[dict], hyperobs: Observer, plots_path: Path) -> None:
    n_rows = len(observer_data)
    fig = plt.figure(figsize=(DOUBLE_COL, max(2.5, 0.62 * n_rows)), layout="constrained")
    fig.get_layout_engine().set(h_pad=0.02, w_pad=0.02, hspace=0.04, wspace=0.03, rect=(0, 0, 1, 0.95))
    gs = GridSpec(n_rows, 4, figure=fig, width_ratios=[1.4, 1.4, 1.75, 3.2])

    spec_axes = []
    diff_axes = []
    for row, od in enumerate(observer_data):
        label = _genotype_label(od["genotype"])
        diffs = _hyperobserver_diffs(hyperobs, od)

        ax_spec = fig.add_subplot(gs[row, 0], sharey=spec_axes[0] if spec_axes else None)
        spec_axes.append(ax_spec)
        _draw_spectra(ax_spec, od, label, show_xlabel=(row == n_rows - 1), show_legend=(row == 0))

        for col, (subset, title) in enumerate([("lms", "LMS"), ("lmsq", "LMSQ")], start=1):
            ax = fig.add_subplot(gs[row, col], sharey=diff_axes[0] if diff_axes else None)
            diff_axes.append(ax)
            indices, labels = _subset_indices(od["genotype"], subset)
            _draw_diff_bars(
                ax,
                diffs,
                indices,
                labels,
                title=title if row == 0 else None,
                show_xlabel=(row == n_rows - 1),
                show_legend=False,
            )

        ax_hyper = fig.add_subplot(gs[row, 3], sharey=diff_axes[0] if diff_axes else None)
        diff_axes.append(ax_hyper)
        _draw_diff_bars(
            ax_hyper,
            diffs,
            list(range(len(HYPER_PEAKS))),
            HYPER_CONE_LABELS,
            title="Hyperobserver" if row == 0 else None,
            show_xlabel=(row == n_rows - 1),
            show_legend=(row == 0),
        )

    _add_legend(fig, diff_axes[-1], "upper right")
    _save(fig, plots_path, "metamer_spectra_lms_lmsq_hyperobserver")


def generate_paper_viz(
    metamers_config_path: str,
    primaries_path: str,
    measurements_dir: str | None,
    plots_dir: str,
    n_observers: int,
    pair_index: int | None,
    figure: str,
) -> None:
    observer_data, hyperobs = _prepare_observer_data(
        metamers_config_path,
        primaries_path,
        measurements_dir,
        n_observers,
        pair_index,
    )
    plots_path = Path(plots_dir)
    plots_path.mkdir(parents=True, exist_ok=True)

    apply_style()
    plt.rcParams.update({
        "text.usetex": False,
        "font.size": 7,
        "axes.titlesize": 7,
        "axes.labelsize": 6,
        "xtick.labelsize": 5,
        "ytick.labelsize": 5,
        "legend.fontsize": 5,
    })

    if figure in ("all", "lms"):
        _make_diff_figure(observer_data, hyperobs, plots_path, "lms")
    if figure in ("all", "lmsq"):
        _make_diff_figure(observer_data, hyperobs, plots_path, "lmsq")
    if figure in ("all", "hyperobserver"):
        _make_diff_figure(observer_data, hyperobs, plots_path, "hyperobserver")
    if figure in ("all", "combined"):
        _make_combined_figure(observer_data, hyperobs, plots_path)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Create paper-style LMS/LMSQ/hyperobserver metamer diff plots.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python paper-viz/hyperobserver_diff_viz.py \\
    --primaries extern/TetriumColor/measurements/2026-04-28/primaries \\
    --plots-dir paper-viz/output
        """,
    )
    parser.add_argument("--primaries", type=str, required=True,
                        help="Path to display primary measurements in BGOR order.")
    parser.add_argument("--metamers", type=str,
                        default=str(TETRIUM_ROOT / "config" / "display_validation_metamers_midpoint.json"),
                        help="Path to BGYR metamer configuration JSON.")
    parser.add_argument("--measurements", type=str, default=None,
                        help="Optional measured spectra directory.")
    parser.add_argument("--plots-dir", type=str, required=True,
                        help="Directory to save output figures.")
    parser.add_argument("--n-observers", type=int, default=8,
                        help="Number of observers from the config to plot.")
    parser.add_argument("--pair-index", type=int, default=None,
                        help="Metamer pair index to plot. Defaults to center pair, then middle pair.")
    parser.add_argument("--figure", type=str, default="all",
                        choices=["all", "lms", "lmsq", "hyperobserver", "combined"],
                        help="Which figure set to generate.")
    args = parser.parse_args()

    generate_paper_viz(
        metamers_config_path=args.metamers,
        primaries_path=args.primaries,
        measurements_dir=args.measurements,
        plots_dir=args.plots_dir,
        n_observers=args.n_observers,
        pair_index=args.pair_index,
        figure=args.figure,
    )


if __name__ == "__main__":
    main()
