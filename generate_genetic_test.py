#!/usr/bin/env python3
"""
Generate genetic color picker test grid from primaries directory.
Usage: python generate_genetic_test.py <primaries_dir> <output_dir>
"""

from PIL import Image
from TetriumColor.Utils.ImageUtils import CreatePaddedGrid
from TetriumColor.Measurement import load_primaries_from_csv
from TetriumColor import PseudoIsochromaticPlateGenerator, ColorSpaceType
from TetriumColor.TetraColorPicker import GeneticColorGenerator
import sys
import os
import numpy as np

# Add TetriumColor to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'extern', 'TetriumColor'))


def generate_genetic_test(primaries_dir: str, output_dir: str, testing_dim: int = 3):
    """
    Generate genetic color picker test grid.

    Args:
        primaries_dir: Directory containing primaries CSV files
        output_dir: Directory to save output images
        testing_dim: Testing dimension (default: 3)
    """
    # Load primaries
    print(f"Loading primaries from: {primaries_dir}")
    primaries = load_primaries_from_csv(primaries_dir)

    # Create color generator
    color_generator = GeneticColorGenerator(
        sex='both',
        percentage_screened=0.99,
        display_primaries=primaries,
        dimensions=[testing_dim],
        metameric_axes=[2],
        trials_per_direction=1,
        randomize_genotypes=False,
        debug_middle=True
    )

    print(f"Number of Genotypes: {len(color_generator.genotypes)}")

    # Create plate generator
    plate_generator = PseudoIsochromaticPlateGenerator(color_generator)

    # Parameters
    lum_noise = 0.0
    s_cone_noise = 0.1
    output_space = ColorSpaceType.DISP_6P

    # Create output directory
    os.makedirs(output_dir, exist_ok=True)

    # Generate plates
    landolt_symbols = ['landolt_up', 'landolt_down', 'landolt_left', 'landolt_right']
    images = []

    idx = 0
    while True:
        random_landolt_symbol = np.random.choice(landolt_symbols)
        print(f"Generating plate {idx}")

        image_info = plate_generator.GetTest(
            None,
            os.path.join(output_dir, f"test_{idx}"),
            random_landolt_symbol,
            output_space=output_space,
            lum_noise=lum_noise,
            s_cone_noise=s_cone_noise
        )

        if image_info is None:
            break

        images.append(image_info)
        idx += 1

    # Create grid
    print(f"Creating grid from {len(images)} plates")
    rgb_images = [Image.open(img['rgb_path']) for img in images]
    ocv_images = [Image.open(img['ocv_path']) for img in images]

    rgb_grid = CreatePaddedGrid(rgb_images, padding=0, channels=3, square_grid=False)
    ocv_grid = CreatePaddedGrid(ocv_images, padding=0, channels=3, square_grid=False)

    # Save grids
    rgb_grid_path = os.path.join(output_dir, "genetic_test_grid_RGB.png")
    ocv_grid_path = os.path.join(output_dir, "genetic_test_grid_OCV.png")

    rgb_grid.save(rgb_grid_path)
    ocv_grid.save(ocv_grid_path)

    print(f"Saved grid to: {rgb_grid_path} and {ocv_grid_path}")

    return rgb_grid_path, ocv_grid_path


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python generate_genetic_test.py <primaries_dir> <output_dir> [testing_dim]")
        sys.exit(1)

    primaries_dir = sys.argv[1]
    output_dir = sys.argv[2]
    testing_dim = int(sys.argv[3]) if len(sys.argv) > 3 else 3

    generate_genetic_test(primaries_dir, output_dir, testing_dim)
