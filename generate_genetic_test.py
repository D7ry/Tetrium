#!/usr/bin/env python3
"""
Generate genetic color picker test grid from primaries directory.
Usage: python generate_genetic_test.py <primaries_dir> <output_dir>
"""

import os
import sys

# Add TetriumColor to path before importing from it.
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'extern', 'TetriumColor'))

from PIL import Image
import numpy as np

from TetriumColor.Utils.ImageUtils import CreatePaddedGrid
from TetriumColor.Utils.CustomTypes import ColorTestResult
from TetriumColor.Measurement import load_primaries_from_csv
from TetriumColor import PseudoIsochromaticPlateGenerator, ColorSpaceType
from TetriumColor.TetraPlate import BipartiteFieldGenerator, GaussianBlobGenerator
from TetriumColor.TetraColorPicker import QuestColorGenerator


def _quantize_display_image(arr: np.ndarray) -> Image.Image:
    return Image.fromarray(np.clip(np.round(arr * 255), 0, 255).astype(np.uint8), "RGB")


def _disp4_to_rgb_ocv(color_space, disp4: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    disp6 = color_space._map_4d_to_6d(np.asarray(disp4, dtype=float))
    return disp6[:3], disp6[3:6]


def _make_tetra_picker_pair_images(
    inside_disp: np.ndarray,
    outside_disp: np.ndarray,
    color_space,
    size: int = 384,
) -> tuple[Image.Image, Image.Image]:
    """Create RGB/OCV bipartite swatches directly from TetraColorPicker DISP values."""
    inside_rgb, inside_ocv = _disp4_to_rgb_ocv(color_space, inside_disp)
    outside_rgb, outside_ocv = _disp4_to_rgb_ocv(color_space, outside_disp)

    rgb = np.zeros((size, size, 3), dtype=np.float64)
    ocv = np.zeros((size, size, 3), dtype=np.float64)
    rgb[:, : size // 2, :] = inside_rgb
    rgb[:, size // 2 :, :] = outside_rgb
    ocv[:, : size // 2, :] = inside_ocv
    ocv[:, size // 2 :, :] = outside_ocv

    return _quantize_display_image(rgb), _quantize_display_image(ocv)


def _generate_tetra_picker_grid(color_generator, output_dir: str) -> tuple[str, str]:
    """Create gaussian-blob stimuli from TetraColorPicker DISP endpoints."""
    test_generator = GaussianBlobGenerator(color_generator)
    return _generate_test_grid(test_generator, color_generator, output_dir, 'tetra_picker')


def _generate_test_grid(test_generator,
                        color_generator,
                        output_dir: str,
                        generator_type: str) -> tuple[str, str]:
    os.makedirs(output_dir, exist_ok=True)

    landolt_symbols = ['landolt_up', 'landolt_down', 'landolt_left', 'landolt_right']
    output_space = ColorSpaceType.DISP_6P
    lum_noise = 0.0
    s_cone_noise = 0.0 if generator_type == 'tetra_picker' else 0.1
    images = []

    for idx in range(color_generator.get_num_samples()):
        hidden_symbol = (
            np.random.choice(landolt_symbols)
            if generator_type in {'plate', 'gaussian_blob', 'tetra_picker'}
            else None
        )

        print(f"Generating test {idx}")
        filename = os.path.join(output_dir, f"test_{idx}")
        if idx == 0:
            image_info = test_generator.NewTest(
                filename,
                hidden_symbol,
                output_space=output_space,
                lum_noise=lum_noise,
                s_cone_noise=s_cone_noise
            )
        else:
            image_info = test_generator.GetTest(
                ColorTestResult.NoAnswer,
                filename,
                hidden_symbol,
                output_space=output_space,
                lum_noise=lum_noise,
                s_cone_noise=s_cone_noise
            )
        if image_info is None:
            break
        images.append(image_info)

    print(f"Creating grid from {len(images)} tests")
    rgb_images = [Image.open(img['rgb_path']) for img in images]
    ocv_images = [Image.open(img['ocv_path']) for img in images]

    rgb_grid = CreatePaddedGrid(rgb_images, padding=0, channels=3, square_grid=False)
    ocv_grid = CreatePaddedGrid(ocv_images, padding=0, channels=3, square_grid=False)

    rgb_grid_path = os.path.join(output_dir, "genetic_test_grid_RGB.png")
    ocv_grid_path = os.path.join(output_dir, "genetic_test_grid_OCV.png")
    rgb_grid.save(rgb_grid_path)
    ocv_grid.save(ocv_grid_path)
    print(f"Saved grid to: {rgb_grid_path} and {ocv_grid_path}")
    return rgb_grid_path, ocv_grid_path


def generate_genetic_test(primaries_dir: str,
                          output_dir: str,
                          testing_dim: int = 3,
                          generator_type: str = 'tetra_picker',
                          gaussian_blob_mode: str = 'raw'):
    """
    Generate genetic color picker test grid.

    Args:
        primaries_dir: Directory containing primaries CSV files
        output_dir: Directory to save output images
        testing_dim: Testing dimension (default: 3)
        generator_type: Type of generator to use - 'tetra_picker', 'plate', 'bipartite', or 'gaussian_blob'
        gaussian_blob_mode: 'raw' or 'raw_cone_excitation' when generator_type is gaussian_blob.
            The legacy value 'cone_contrast' is still accepted as an alias.
    """
    # Load primaries
    print(f"Loading primaries from: {primaries_dir}")
    primaries = load_primaries_from_csv(primaries_dir, extract_zero=False, primary_order='RGBO')

    # Match AppPseudoIsochromaticTest genetic gaussian mode: top observers,
    # display-midpoint background, one MCS level at maximal contrast.
    top_n_genotypes = 10
    uses_raw_cone_excitation = (
        generator_type == 'tetra_picker'
        or gaussian_blob_mode in {'raw_cone_excitation', 'cone_contrast'}
    )
    color_picking_space = (
        'raw_cone_excitation'
        if uses_raw_cone_excitation
        else 'cone'
    )
    direct_display_output = generator_type in {'tetra_picker', 'gaussian_blob'}
    color_generator = QuestColorGenerator(
        sex='both',
        percentage_screened=1.0,
        luminance=0.5,
        dimensions=[testing_dim],
        display_primaries=primaries,
        metameric_axes=[2],
        trials_per_direction=1,
        mcs_k=1,
        observer_indices=list(range(top_n_genotypes)),
        color_picking_space=color_picking_space,
        bipolar=False,
        return_color_space=ColorSpaceType.DISP if direct_display_output else ColorSpaceType.CONE,
        wavelengths=primaries[0].wavelengths,
    )

    print(f"Number of Genotypes: {len(color_generator.genotypes)}")

    if generator_type == 'tetra_picker':
        print("Using TetraColorPicker negative-direction gaussian blob over midpoint background")
        return _generate_tetra_picker_grid(color_generator, output_dir)

    # Create test generator based on type
    if generator_type == 'bipartite':
        print("Using BipartiteFieldGenerator")
        test_generator = BipartiteFieldGenerator(color_generator)
    elif generator_type == 'gaussian_blob':
        print("Using GaussianBlobGenerator")
        test_generator = GaussianBlobGenerator(
            color_generator,
            constant_disp_background=(
                gaussian_blob_mode in {'raw_cone_excitation', 'cone_contrast'}
            )
        )
    else:
        print("Using PseudoIsochromaticPlateGenerator")
        test_generator = PseudoIsochromaticPlateGenerator(color_generator)

    return _generate_test_grid(test_generator, color_generator, output_dir, generator_type)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(
            "Usage: python generate_genetic_test.py <primaries_dir> <output_dir> "
            "[testing_dim] [generator_type] [gaussian_blob_mode]"
        )
        print("  generator_type: 'tetra_picker' (default), 'plate', 'bipartite', or 'gaussian_blob'")
        print("  gaussian_blob_mode: 'raw' (default), 'raw_cone_excitation', or legacy 'cone_contrast'")
        sys.exit(1)

    primaries_dir = sys.argv[1]
    output_dir = sys.argv[2]
    testing_dim = int(sys.argv[3]) if len(sys.argv) > 3 else 3
    generator_type = sys.argv[4] if len(sys.argv) > 4 else 'tetra_picker'
    gaussian_blob_mode = sys.argv[5] if len(sys.argv) > 5 else 'raw'

    generate_genetic_test(
        primaries_dir,
        output_dir,
        testing_dim,
        generator_type,
        gaussian_blob_mode
    )
