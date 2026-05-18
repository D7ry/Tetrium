#!/usr/bin/env python3
"""Analyze projector LED drift sessions collected by Tetrium AppAutoMeasure."""

from __future__ import annotations

import argparse
import csv
import json
import sys
from dataclasses import dataclass
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

REPO_ROOT = Path(__file__).resolve().parents[2]
TETRIUM_COLOR_ROOT = REPO_ROOT / "extern" / "TetriumColor"
if str(TETRIUM_COLOR_ROOT) not in sys.path:
    sys.path.insert(0, str(TETRIUM_COLOR_ROOT))

from TetriumColor.Plotting.PlotStyle import (  # noqa: E402
    COLORS,
    DOUBLE_COL,
    PAPER_BLUE,
    PAPER_RED,
    apply_style,
)

LED_COLORS = {
    "R": PAPER_RED,
    "G": COLORS[530],
    "B": PAPER_BLUE,
    "O": COLORS[552],
}


@dataclass
class DriftRow:
    elapsed_minutes: float
    cycle_index: int
    led: str
    luminance: float
    integrated_power: float
    peak_wavelength: float
    peak_power: float
    spectrum_file: str


def _float(row: dict[str, str], key: str) -> float:
    return float(row[key]) if row.get(key) else np.nan


def load_timeseries(session: Path) -> list[DriftRow]:
    path = session / "led_drift_timeseries.csv"
    if not path.exists():
        raise FileNotFoundError(f"Missing timeseries CSV: {path}")

    rows: list[DriftRow] = []
    with path.open(newline="") as fh:
        reader = csv.DictReader(fh)
        for row in reader:
            rows.append(
                DriftRow(
                    elapsed_minutes=_float(row, "elapsed_minutes"),
                    cycle_index=int(row["cycle_index"]),
                    led=row["led"],
                    luminance=_float(row, "luminance"),
                    integrated_power=_float(row, "integrated_power"),
                    peak_wavelength=_float(row, "peak_wavelength"),
                    peak_power=_float(row, "peak_power"),
                    spectrum_file=row.get("spectrum_file", ""),
                )
            )
    if not rows:
        raise ValueError(f"No rows found in {path}")
    return rows


def load_metadata(session: Path) -> dict:
    path = session / "session_metadata.json"
    if not path.exists():
        return {}
    with path.open() as fh:
        return json.load(fh)


def rows_for_led(rows: list[DriftRow], led: str) -> list[DriftRow]:
    return [row for row in rows if row.led == led]


def percent_change(values: np.ndarray) -> np.ndarray:
    if values.size == 0 or not np.isfinite(values[0]) or values[0] == 0:
        return np.full_like(values, np.nan, dtype=float)
    return 100.0 * (values - values[0]) / values[0]


def rolling_average(values: np.ndarray, window: int) -> np.ndarray:
    if window <= 1 or values.size <= 1:
        return values

    smoothed = np.full(values.shape, np.nan, dtype=float)
    half_window = window // 2
    for i in range(values.size):
        start = max(0, i - half_window)
        end = min(values.size, i + half_window + 1)
        segment = values[start:end]
        finite = segment[np.isfinite(segment)]
        if finite.size:
            smoothed[i] = float(np.mean(finite))
    return smoothed


def linear_fit(x: np.ndarray, y: np.ndarray) -> tuple[float, float]:
    mask = np.isfinite(x) & np.isfinite(y)
    if mask.sum() < 2:
        return np.nan, np.nan
    slope, intercept = np.polyfit(x[mask], y[mask], 1)
    return float(slope), float(intercept)


def fit_decline_model(x: np.ndarray, y: np.ndarray) -> dict[str, float | str]:
    mask = np.isfinite(x) & np.isfinite(y)
    if mask.sum() < 4:
        slope, intercept = linear_fit(x, y)
        return {"model": "linear", "slope_per_min": slope, "intercept": intercept}

    try:
        from scipy.optimize import curve_fit

        def model(t, amplitude, tau_min, offset):
            return offset + amplitude * np.exp(-t / tau_min)

        y_masked = y[mask]
        x_masked = x[mask]
        tau_guess = max((x_masked[-1] - x_masked[0]) / 2.0, 1.0)
        params, _ = curve_fit(
            model,
            x_masked,
            y_masked,
            p0=[y_masked[0] - y_masked[-1], tau_guess, y_masked[-1]],
            bounds=([-np.inf, 0.1, -np.inf], [np.inf, 24.0 * 60.0, np.inf]),
            maxfev=10000,
        )
        return {
            "model": "exp_offset",
            "amplitude": float(params[0]),
            "tau_min": float(params[1]),
            "offset": float(params[2]),
        }
    except Exception:
        slope, intercept = linear_fit(x, y)
        return {"model": "linear", "slope_per_min": slope, "intercept": intercept}


def evaluate_decline_model(fit: dict[str, float | str], x: np.ndarray) -> np.ndarray:
    if fit.get("model") == "exp_offset":
        amplitude = float(fit["amplitude"])
        tau_min = float(fit["tau_min"])
        offset = float(fit["offset"])
        return offset + amplitude * np.exp(-x / tau_min)
    if fit.get("model") == "linear":
        slope = float(fit["slope_per_min"])
        intercept = float(fit["intercept"])
        return slope * x + intercept
    return np.full(x.shape, np.nan, dtype=float)


def save_figure(fig: plt.Figure, path: Path) -> None:
    try:
        fig.savefig(path, bbox_inches="tight")
    except Exception as exc:
        if "latex" not in str(exc).lower() and "tex" not in str(exc).lower():
            raise
        plt.rcParams["text.usetex"] = False
        fig.savefig(path, bbox_inches="tight")
    finally:
        plt.close(fig)


def plot_metric(
    rows: list[DriftRow],
    out_path: Path,
    attr: str,
    ylabel: str,
    title: str,
    rolling_window: int,
) -> None:
    fig, ax = plt.subplots(figsize=(DOUBLE_COL, 3.1))
    for led in LED_COLORS:
        led_rows = rows_for_led(rows, led)
        if not led_rows:
            continue
        x = np.array([row.elapsed_minutes for row in led_rows], dtype=float)
        y = np.array([getattr(row, attr) for row in led_rows], dtype=float)
        y_smooth = rolling_average(y, rolling_window)
        ax.plot(x, y, "o", color=LED_COLORS[led], alpha=0.25, markersize=2.0)
        ax.plot(x, y_smooth, "-", color=LED_COLORS[led], label=led)
    ax.set_xlabel("Elapsed time (min)")
    ax.set_ylabel(ylabel)
    ax.set_title(f"{title} ({rolling_window}-sample rolling mean)")
    ax.legend(title="LED")
    save_figure(fig, out_path)


def plot_percent_luminance(rows: list[DriftRow], out_path: Path, rolling_window: int) -> None:
    fig, ax = plt.subplots(figsize=(DOUBLE_COL, 3.1))
    for led in LED_COLORS:
        led_rows = rows_for_led(rows, led)
        if not led_rows:
            continue
        x = np.array([row.elapsed_minutes for row in led_rows], dtype=float)
        y = percent_change(np.array([row.luminance for row in led_rows], dtype=float))
        y_smooth = rolling_average(y, rolling_window)
        fit = fit_decline_model(x, y)
        x_fit = np.linspace(float(np.nanmin(x)), float(np.nanmax(x)), 300)
        y_fit = evaluate_decline_model(fit, x_fit)
        ax.plot(x, y, "o", color=LED_COLORS[led], alpha=0.25, markersize=2.0)
        ax.plot(x, y_smooth, "-", color=LED_COLORS[led], label=led)
        ax.plot(x_fit, y_fit, "--", color=LED_COLORS[led], alpha=0.75, linewidth=0.9)
    ax.axhline(0, color="#777777", linewidth=0.8, linestyle="--")
    ax.set_xlabel("Elapsed time (min)")
    ax.set_ylabel("Luminance change vs first sample (%)")
    ax.set_title(f"LED luminance drift ({rolling_window}-sample rolling mean + fitted curve)")
    ax.legend(title="LED")
    save_figure(fig, out_path)


def read_spectrum(path: Path) -> tuple[np.ndarray, np.ndarray]:
    wavelengths: list[float] = []
    powers: list[float] = []
    with path.open(newline="") as fh:
        reader = csv.DictReader(fh)
        for row in reader:
            normalized = {key.strip(): value for key, value in row.items() if key is not None}
            wavelengths.append(float(normalized["wavelength"]))
            powers.append(float(normalized["power"]))
    return np.array(wavelengths), np.array(powers)


def plot_spectra_snapshots(session: Path, rows: list[DriftRow], out_path: Path) -> None:
    fig, axes = plt.subplots(2, 2, figsize=(DOUBLE_COL, 5.0), sharex=True)
    axes_flat = axes.flatten()
    for ax, led in zip(axes_flat, LED_COLORS):
        led_rows = rows_for_led(rows, led)
        if not led_rows:
            ax.set_visible(False)
            continue
        indices = sorted(set([0, len(led_rows) // 2, len(led_rows) - 1]))
        for idx in indices:
            row = led_rows[idx]
            spectrum_path = session / row.spectrum_file
            if not spectrum_path.exists():
                continue
            wavelengths, powers = read_spectrum(spectrum_path)
            ax.plot(
                wavelengths,
                powers,
                color=LED_COLORS[led],
                alpha=0.35 + 0.25 * idx / max(len(led_rows) - 1, 1),
                label=f"{row.elapsed_minutes:.0f} min",
            )
        ax.set_title(f"{led} spectra")
        ax.set_ylabel("Power")
        ax.legend()
    for ax in axes[1]:
        ax.set_xlabel("Wavelength (nm)")
    save_figure(fig, out_path)


def write_session_summary(session: Path, rows: list[DriftRow]) -> dict[str, dict[str, float | str]]:
    summary: dict[str, dict[str, float | str]] = {}
    out_path = session / "led_drift_summary.csv"
    with out_path.open("w", newline="") as fh:
        fieldnames = [
            "led",
            "n",
            "duration_min",
            "initial_luminance",
            "final_luminance",
            "luminance_change_pct",
            "linear_luminance_slope_pct_per_hour",
            "fit_model",
            "fit_tau_min",
        ]
        writer = csv.DictWriter(fh, fieldnames=fieldnames)
        writer.writeheader()
        for led in LED_COLORS:
            led_rows = rows_for_led(rows, led)
            if not led_rows:
                continue
            x = np.array([row.elapsed_minutes for row in led_rows], dtype=float)
            lum = np.array([row.luminance for row in led_rows], dtype=float)
            pct = percent_change(lum)
            slope_pct_per_min, _ = linear_fit(x, pct)
            fit = fit_decline_model(x, pct)
            row = {
                "led": led,
                "n": len(led_rows),
                "duration_min": float(x[-1] - x[0]) if len(x) > 1 else 0.0,
                "initial_luminance": float(lum[0]),
                "final_luminance": float(lum[-1]),
                "luminance_change_pct": float(pct[-1]),
                "linear_luminance_slope_pct_per_hour": float(slope_pct_per_min * 60.0),
                "fit_model": fit["model"],
                "fit_tau_min": fit.get("tau_min", np.nan),
            }
            writer.writerow(row)
            summary[led] = row
    return summary


def analyze_session(session: Path, rolling_window: int) -> dict[str, dict[str, float | str]]:
    rows = load_timeseries(session)
    plot_metric(
        rows,
        session / "led_luminance_over_time.png",
        "luminance",
        "Luminance",
        "LED luminance over time",
        rolling_window,
    )
    plot_percent_luminance(rows, session / "led_luminance_percent_change.png", rolling_window)
    plot_metric(
        rows,
        session / "led_integrated_power_over_time.png",
        "integrated_power",
        "Integrated spectral power",
        "LED integrated power over time",
        rolling_window,
    )
    plot_spectra_snapshots(session, rows, session / "led_spectra_snapshots.png")
    return write_session_summary(session, rows)


def discover_sessions(root: Path) -> list[Path]:
    return sorted(path for path in root.rglob("led_drift_*") if (path / "led_drift_timeseries.csv").exists())


def write_comparison(sessions: list[Path], out_dir: Path, rolling_window: int) -> None:
    rows = []
    for session in sessions:
        summary = analyze_session(session, rolling_window)
        metadata = load_metadata(session)
        for led, values in summary.items():
            row = {"session": session.name, "led": led, **values}
            row["start_temperature"] = metadata.get("start_temperature", "")
            row["end_temperature"] = metadata.get("end_temperature", "")
            rows.append(row)

    out_csv = out_dir / "led_drift_session_comparison.csv"
    fieldnames = sorted({key for row in rows for key in row.keys()})
    with out_csv.open("w", newline="") as fh:
        writer = csv.DictWriter(fh, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    fig, ax = plt.subplots(figsize=(DOUBLE_COL, 3.4))
    x_positions = np.arange(len(sessions))
    width = 0.18
    for offset, led in enumerate(LED_COLORS):
        vals = []
        for session in sessions:
            match = next((row for row in rows if row["session"] == session.name and row["led"] == led), None)
            vals.append(float(match["luminance_change_pct"]) if match else np.nan)
        ax.bar(
            x_positions + (offset - 1.5) * width,
            vals,
            width=width,
            color=LED_COLORS[led],
            label=led,
        )
    ax.axhline(0, color="#777777", linewidth=0.8)
    ax.set_xticks(x_positions)
    ax.set_xticklabels([session.name.replace("led_drift_", "") for session in sessions], rotation=20, ha="right")
    ax.set_ylabel("Final luminance change (%)")
    ax.set_title("LED drift across sessions")
    ax.legend(title="LED")
    save_figure(fig, out_dir / "led_drift_session_comparison.png")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--session", action="append", type=Path, default=[], help="LED drift session folder.")
    parser.add_argument("--root", type=Path, help="Root folder containing led_drift_* session folders.")
    parser.add_argument("--out-dir", type=Path, help="Comparison output directory. Defaults to root or first session parent.")
    parser.add_argument(
        "--rolling-window",
        type=int,
        default=5,
        help="Centered rolling-average window in samples per LED for time-series plots. Use 1 for raw lines.",
    )
    args = parser.parse_args()

    apply_style()

    sessions = [path.resolve() for path in args.session]
    if args.root:
        sessions.extend(discover_sessions(args.root.resolve()))
    sessions = sorted(set(sessions))
    if not sessions:
        raise SystemExit("No sessions provided. Use --session or --root.")

    rolling_window = max(1, args.rolling_window)
    if len(sessions) == 1 and not args.root:
        analyze_session(sessions[0], rolling_window)
        print(f"Analyzed LED drift session: {sessions[0]}")
        return

    out_dir = args.out_dir.resolve() if args.out_dir else (args.root.resolve() if args.root else sessions[0].parent)
    out_dir.mkdir(parents=True, exist_ok=True)
    write_comparison(sessions, out_dir, rolling_window)
    print(f"Analyzed {len(sessions)} LED drift sessions into {out_dir}")


if __name__ == "__main__":
    main()
