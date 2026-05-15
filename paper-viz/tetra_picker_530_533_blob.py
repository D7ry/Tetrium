#!/usr/bin/env python3
"""Generate the TetraColorPicker Gaussian blob output for genotype (530, 533)."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import numpy as np
from colour import XYZ_to_sRGB
from PIL import Image

REPO_ROOT = Path(__file__).resolve().parents[1]
TETRIUM_ROOT = REPO_ROOT / "extern" / "TetriumColor"
sys.path.insert(0, str(TETRIUM_ROOT))

from TetriumColor import ColorSpaceType  # noqa: E402
from TetriumColor.Measurement import load_primaries_from_csv  # noqa: E402
from TetriumColor.Observer.Spectra import Spectra  # noqa: E402
from TetriumColor.Observer.ObserverGenotypes import ObserverGenotypes  # noqa: E402
from TetriumColor.TetraColorPicker import QuestColorGenerator  # noqa: E402
from TetriumColor.TetraPlate import GaussianBlobGenerator  # noqa: E402

TARGET_GENOTYPE = (530, 533)


def _observer_index_for_genotype(genotype: tuple[int, int], sex: str, seed: int) -> int:
    genotypes = list(ObserverGenotypes(dimensions=[3], seed=seed).get_pdf(sex).keys())
    try:
        return genotypes.index(genotype)
    except ValueError as exc:
        raise ValueError(f"Genotype {genotype} is not in the {sex!r} dimensions=[3] PDF") from exc


def _display_weights_to_srgb(weights: np.ndarray, primaries: list[Spectra], exposure: float) -> np.ndarray:
    """Convert RGBO display-primary weights to sRGB by integrating measured primary spectra."""
    weights_2d = np.asarray(weights, dtype=float).reshape(-1, len(primaries))
    primary_xyz = np.array([primary.to_xyz() for primary in primaries])
    srgb = XYZ_to_sRGB(weights_2d @ primary_xyz)
    return np.clip(srgb * exposure, 0.0, 1.0).reshape(*np.asarray(weights).shape[:-1], 3)


def _save_measured_primary_srgb_blob(
    blob_generator: GaussianBlobGenerator,
    trial: dict,
    primaries: list[Spectra],
    srgb_path: Path,
    exposure: float,
) -> None:
    """Render the TetraPlate display-primary blob as an sRGB preview."""
    metadata = trial["metadata"]
    inside_disp = np.asarray(metadata["inside_disp"], dtype=float)
    outside_disp = np.asarray(metadata["outside_disp"], dtype=float)
    background_disp = (
        np.full_like(outside_disp, metadata["display_background"], dtype=float)
        if metadata["constant_disp_background"]
        else outside_disp
    )

    disp_img, _ = blob_generator._build_disp_image(
        inside_disp,
        background_disp,
        metadata["direction"],
        metadata["degree"],
        metadata["lum_noise"],
        metadata["s_cone_noise"],
    )
    disp_img = np.nan_to_num(disp_img, nan=metadata["display_background"])
    srgb_img = _display_weights_to_srgb(disp_img, primaries, exposure)
    Image.fromarray(np.round(srgb_img * 255.0).astype(np.uint8), "RGB").save(srgb_path)


def generate_blob(args: argparse.Namespace) -> dict:
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    observer_index = _observer_index_for_genotype(TARGET_GENOTYPE, args.sex, args.seed)
    primaries = load_primaries_from_csv(args.primaries, extract_zero=False)
    background = np.ones(4, dtype=float) * args.background

    color_generator = QuestColorGenerator(
        sex=args.sex,
        percentage_screened=args.percentage_screened,
        peak_to_test=args.peak_to_test,
        luminance=args.background,
        dimensions=[3],
        seed=args.seed,
        trials_per_direction=1,
        metameric_axes=[args.metameric_axis],
        bipolar=True,
        degree=args.degree,
        mcs_k=1,
        observer_indices=[observer_index],
        color_picking_space="raw_cone_excitation",
        adapting_background=background,
        adapting_background_space=ColorSpaceType.DISP,
        display_primaries=primaries,
        wavelengths=primaries[0].wavelengths,
        illuminant="raw",
        template="baylor",
    )

    blob_generator = GaussianBlobGenerator(
        color_generator,
        seed=args.seed,
        size=args.size,
        blob_size=args.blob_size,
        constant_disp_background=args.constant_disp_background,
    )

    trial = blob_generator.NewTest(
        str(output_dir / args.stem),
        hidden_symbol=args.hidden_symbol,
        output_space=ColorSpaceType.SRGB,
        lum_noise=0.0,
        s_cone_noise=0.0,
        background_luminance=args.background,
        degree=args.degree,
    )
    srgb_path = output_dir / f"{args.stem}_SRGB.png"
    _save_measured_primary_srgb_blob(
        blob_generator,
        trial,
        primaries,
        srgb_path,
        args.srgb_exposure,
    )
    trial["rgb_path"] = str(srgb_path)
    trial["ocv_path"] = str(srgb_path)
    trial["metadata"]["srgb_preview_method"] = "measured_primaries_to_srgb"
    trial["metadata"]["srgb_exposure"] = args.srgb_exposure

    summary = {
        "target_genotype": list(TARGET_GENOTYPE),
        "observer_index": observer_index,
        "trial": trial,
        "tetra_color_picker_output": color_generator.GetCurrentTrialMetadata(),
    }

    summary_path = output_dir / f"{args.stem}.json"
    with summary_path.open("w") as f:
        json.dump(summary, f, indent=2)
    summary["summary_path"] = str(summary_path)
    return summary


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate a (530, 533) TetraColorPicker Gaussian blob SRGB PNG."
    )
    parser.add_argument(
        "--primaries",
        default=str(TETRIUM_ROOT / "measurements" / "2026-05-08" / "primaries"),
        help="Display primaries directory.",
    )
    parser.add_argument(
        "--output-dir",
        default=str(REPO_ROOT / "paper-viz" / "output" / "tetra_picker_530_533_blob"),
        help="Directory for the generated PNG and JSON summary.",
    )
    parser.add_argument("--stem", default="tetra_picker_530_533_blob")
    parser.add_argument("--sex", default="both", choices=["male", "female", "both"])
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--peak-to-test", type=float, default=547)
    parser.add_argument("--percentage-screened", type=float, default=0.995)
    parser.add_argument("--metameric-axis", type=int, default=2)
    parser.add_argument("--degree", type=float, default=2.0)
    parser.add_argument("--background", type=float, default=0.5)
    parser.add_argument("--size", type=int, default=512)
    parser.add_argument("--blob-size", type=float, default=1.0)
    parser.add_argument(
        "--srgb-exposure",
        type=float,
        default=1000.0,
        help="Exposure multiplier after measured-primary spectra are converted to sRGB.",
    )
    parser.add_argument("--hidden-symbol", default="landolt_right")
    parser.add_argument("--constant-disp-background", action="store_true")
    args = parser.parse_args()

    summary = generate_blob(args)
    print(json.dumps(summary, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
