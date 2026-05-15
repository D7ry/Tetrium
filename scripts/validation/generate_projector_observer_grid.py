#!/usr/bin/env python3
"""Generate observer/template metamer grids for AppImageViewer.

This is a projector-facing wrapper around the template observer grid workflow.
It writes paired *_RGB.png and *_OCV.png files directly into
assets/apps/AppImageViewer/tetra_images so the Image Viewer app can load them
without moving files.
"""

from __future__ import annotations

import argparse
import csv
import os
import sys
from pathlib import Path

import numpy as np
from PIL import Image


REPO_ROOT = Path(__file__).resolve().parents[2]
TETRIUM_COLOR_ROOT = REPO_ROOT / "extern" / "TetriumColor"
TEMPLATE_GRID_SCRIPT_DIR = TETRIUM_COLOR_ROOT / "scripts" / "simulation"

sys.path.insert(0, str(TETRIUM_COLOR_ROOT))
sys.path.insert(0, str(TEMPLATE_GRID_SCRIPT_DIR))

from TetriumColor.Measurement import load_primaries_from_csv  # noqa: E402
from TetriumColor.Observer import Observer  # noqa: E402
from TetriumColor.Observer.ObserverGenotypes import ObserverGenotypes  # noqa: E402
from generate_template_observer_grid import (  # noqa: E402
    TEMPLATES,
    make_pair_cell,
    observer_from_genotype,
    place_cell,
    solve_bgor_pair,
)


DEFAULT_OUTPUT_DIR = REPO_ROOT / "assets" / "apps" / "AppImageViewer" / "tetra_images"
DEFAULT_PRIMARIES_DIR = TETRIUM_COLOR_ROOT / "measurements" / "2026-05-08" / "primaries"
SUITE_PREFIX = "observer_grid_"


def _remove_suite_outputs(output_dir: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    removed = 0
    for path in output_dir.glob(f"{SUITE_PREFIX}*"):
        if path.name.endswith(("_RGB.png", "_OCV.png", "_metadata.csv")):
            path.unlink()
            removed += 1
    print(f"Removed {removed} observer-grid suite output files from {output_dir}")


def _save_pair(output_dir: Path, base_name: str, rgb: np.ndarray, ocv: np.ndarray) -> tuple[Path, Path]:
    if not base_name.startswith(SUITE_PREFIX):
        raise ValueError(f"Suite image base name must start with {SUITE_PREFIX!r}: {base_name}")
    if rgb.shape != ocv.shape:
        raise ValueError(f"RGB/OCV shape mismatch for {base_name}: {rgb.shape} != {ocv.shape}")
    if rgb.dtype != np.uint8 or ocv.dtype != np.uint8:
        raise ValueError(f"RGB/OCV images must be uint8 for {base_name}")

    rgb_path = output_dir / f"{base_name}_RGB.png"
    ocv_path = output_dir / f"{base_name}_OCV.png"
    Image.fromarray(rgb).save(rgb_path)
    Image.fromarray(ocv).save(ocv_path)
    return rgb_path, ocv_path


def _build_canvas(rows: int, cols: int, cell_size: int, cell_gap: int) -> np.ndarray:
    height = rows * cell_size + max(rows - 1, 0) * cell_gap
    width = cols * cell_size + max(cols - 1, 0) * cell_gap
    return np.zeros((height, width, 3), dtype=np.uint8)


def generate_projector_observer_grid(
    primaries_dir: Path,
    output_dir: Path,
    num_observers: int,
    sex: str,
    seed: int,
    proportion: float,
    od: float,
    macular: float,
    lens: float,
    cell_size: int,
    cell_gap: int,
    blob_sigma_frac: float,
    write_cells: bool,
) -> list[dict[str, object]]:
    output_dir.mkdir(parents=True, exist_ok=True)

    primaries = load_primaries_from_csv(str(primaries_dir), extract_zero=False, primary_order="BGOR")
    wavelengths = primaries[0].wavelengths
    observer_genotypes = ObserverGenotypes(wavelengths=wavelengths, dimensions=[3], seed=seed)
    genotypes = list(observer_genotypes.get_pdf(sex).keys())[:num_observers]

    rows = len(TEMPLATES)
    cols = len(genotypes)
    full_rgb = _build_canvas(rows, cols, cell_size, cell_gap)
    full_ocv = _build_canvas(rows, cols, cell_size, cell_gap)
    top_rgb = _build_canvas(1, cols, cell_size, cell_gap)
    top_ocv = _build_canvas(1, cols, cell_size, cell_gap)
    per_template_rgb = {template: _build_canvas(1, cols, cell_size, cell_gap) for template in TEMPLATES}
    per_template_ocv = {template: _build_canvas(1, cols, cell_size, cell_gap) for template in TEMPLATES}

    metadata_rows: list[dict[str, object]] = []

    for row, template in enumerate(TEMPLATES):
        for col, genotype in enumerate(genotypes):
            observer = observer_from_genotype(
                wavelengths=wavelengths,
                genotype=genotype,
                template=template,
                od=od,
                macular=macular,
                lens=lens,
            )
            codes, bgor, _spectra, scale, _midpoint_spectrum = solve_bgor_pair(
                observer,
                primaries,
                proportion,
            )
            rgo_cell, bgo_cell = make_pair_cell(codes[0], codes[1], cell_size, blob_sigma_frac)

            place_cell(full_rgb, rgo_cell, row, col, cell_size, cell_gap)
            place_cell(full_ocv, bgo_cell, row, col, cell_size, cell_gap)
            place_cell(per_template_rgb[template], rgo_cell, 0, col, cell_size, cell_gap)
            place_cell(per_template_ocv[template], bgo_cell, 0, col, cell_size, cell_gap)

            if row == 0:
                place_cell(top_rgb, rgo_cell, 0, col, cell_size, cell_gap)
                place_cell(top_ocv, bgo_cell, 0, col, cell_size, cell_gap)

            if write_cells:
                cell_base = f"{SUITE_PREFIX}cell_{template}_obs{col:02d}"
                _save_pair(output_dir, cell_base, rgo_cell, bgo_cell)

            rgbo_1 = codes[0][[3, 1, 0, 2]]
            rgbo_2 = codes[1][[3, 1, 0, 2]]
            metadata_rows.append(
                {
                    "row": row,
                    "col": col,
                    "template": template,
                    "genotype": repr(genotype),
                    "m_peak": sorted(genotype)[0],
                    "q_peak": 547,
                    "l_peak": sorted(genotype)[1],
                    "generation_mode": "true_cone_contrast_null_direction",
                    "background_space": "DISP",
                    "background": "[0.5, 0.5, 0.5, 0.5]",
                    "proportion": proportion,
                    "display_scale": scale,
                    "bgor_1": codes[0].tolist(),
                    "bgor_2": codes[1].tolist(),
                    "rgbo_1": rgbo_1.tolist(),
                    "rgbo_2": rgbo_2.tolist(),
                    "quantized_bgor_1": bgor[0].tolist(),
                    "quantized_bgor_2": bgor[1].tolist(),
                }
            )
            print(f"{template} obs{col:02d} genotype={genotype}: BGOR {codes[0].tolist()} / {codes[1].tolist()}")

    written = []
    written.append(_save_pair(output_dir, f"{SUITE_PREFIX}template_metamer", full_rgb, full_ocv))
    written.append(_save_pair(output_dir, f"{SUITE_PREFIX}top_observers", top_rgb, top_ocv))
    for template in TEMPLATES:
        written.append(_save_pair(output_dir, f"{SUITE_PREFIX}{template}", per_template_rgb[template], per_template_ocv[template]))

    metadata_path = output_dir / f"{SUITE_PREFIX}metadata.csv"
    with metadata_path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(metadata_rows[0].keys()), lineterminator="\n")
        writer.writeheader()
        writer.writerows(metadata_rows)

    print(f"Wrote metadata: {metadata_path}")
    for rgb_path, ocv_path in written:
        print(f"Wrote pair: {rgb_path} | {ocv_path}")
    if write_cells:
        print(f"Wrote {len(metadata_rows)} per-cell observer_grid_cell_* pairs")

    return metadata_rows


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--primaries_dir", type=Path, default=DEFAULT_PRIMARIES_DIR)
    parser.add_argument("--output_dir", type=Path, default=DEFAULT_OUTPUT_DIR)
    parser.add_argument("--num_observers", type=int, default=10)
    parser.add_argument("--sex", choices=("male", "female", "both"), default="both")
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--proportion", type=float, default=1.0)
    parser.add_argument("--od", type=float, default=0.5)
    parser.add_argument("--macular", type=float, default=1.0)
    parser.add_argument("--lens", type=float, default=1.0)
    parser.add_argument("--cell_size", type=int, default=96)
    parser.add_argument("--cell_gap", type=int, default=6)
    parser.add_argument("--blob_sigma_frac", type=float, default=0.16)
    parser.add_argument("--write_cells", action="store_true")
    parser.add_argument(
        "--clean-suite-outputs",
        action="store_true",
        help="Remove existing observer_grid_* RGB/OCV/metadata outputs before generating.",
    )
    args = parser.parse_args()

    if args.clean_suite_outputs:
        _remove_suite_outputs(args.output_dir)

    generate_projector_observer_grid(
        primaries_dir=args.primaries_dir,
        output_dir=args.output_dir,
        num_observers=args.num_observers,
        sex=args.sex,
        seed=args.seed,
        proportion=args.proportion,
        od=args.od,
        macular=args.macular,
        lens=args.lens,
        cell_size=args.cell_size,
        cell_gap=args.cell_gap,
        blob_sigma_frac=args.blob_sigma_frac,
        write_cells=args.write_cells,
    )


if __name__ == "__main__":
    main()
