#!/usr/bin/env python3
"""
Diagnostic script: verify that GeneticColorGenerator produces true metamers.

For each metamer pair (inside_cone, outside_cone), prints all 4 cone channels
and computes the per-channel difference. True metamers should have:
  - Channels 0 (S), 1 (M), 3 (L) ≈ same  (trichromatic channels)
  - Channel 2 (Q)              ≠ same  (tetrachromatic channel)

Also converts both colors to sRGB for visual reference.

Usage:
    conda run -n tetrium python debug_metamers.py <primaries_dir>
    e.g.: conda run -n tetrium python debug_metamers.py \
        ../extern/TetriumColor/measurements/2026-02-11/primaries
"""

import sys
import os
import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'extern', 'TetriumColor'))

from TetriumColor.Measurement import load_primaries_from_csv
from TetriumColor.TetraColorPicker import GeneticColorGenerator
from TetriumColor import ColorSpaceType

CONE_NAMES = ["S(420)", "M1", "Q(547)", "L"]


def check_metamers(primaries_dir: str, testing_dim: int = 3, metameric_axis: int = 2):
    print(f"\n=== Metamer Diagnostic ===")
    print(f"Primaries dir: {primaries_dir}")
    print(f"Testing dim: {testing_dim}, Metameric axis: {metameric_axis}")

    # Load primaries (BGOR order: Blue, Green, Orange, Red)
    primaries = load_primaries_from_csv(primaries_dir, extract_zero=False)
    print(f"Loaded {len(primaries)} primaries (RGBO order)")
    for i, p in enumerate(primaries):
        label = ["Red", "Green", "Blue", "Orange"][i]
        peak_wl = p.wavelengths[np.argmax(p.data)] if p is not None else "None"
        print(f"  Primary {i} ({label}): peak ~{peak_wl:.0f}nm")

    # Create generator with same params as generate_genetic_test.py
    color_generator = GeneticColorGenerator(
        sex='both',
        percentage_screened=0.99,
        display_primaries=primaries,
        dimensions=[testing_dim],
        metameric_axes=[metameric_axis],
        trials_per_direction=1,
        randomize_genotypes=False,
        debug_middle=True
    )

    print(f"\nGenotypes: {len(color_generator.genotypes)}")
    for g in color_generator.genotypes:
        print(f"  {g}")

    print("\n" + "=" * 70)
    print(f"{'#':>3} {'Genotype':>20} {'Ch':>4}  {'Inside':>8}  {'Outside':>8}  {'Diff':>8}  {'Note'}")
    print("=" * 70)

    pair_idx = 0
    max_trichrom_diffs = []  # S, M, L differences (should be ~0)
    all_q_diffs = []         # Q differences (should be large)

    while True:
        result = color_generator.NewColor()
        if result is None:
            break

        inside_cone, outside_cone, color_space, intensity = result
        genotype, axis = color_generator.GetCurrentTestInfo()

        trichrom_diff = []
        for ch in range(4):
            diff = inside_cone[ch] - outside_cone[ch]
            note = ""
            if ch == metameric_axis:
                note = "<-- tetrachromatic (Q)"
                all_q_diffs.append(abs(diff))
            else:
                note = "<-- should be ~0"
                trichrom_diff.append(abs(diff))

            print(f"{pair_idx:>3} {str(genotype)[:20]:>20} {CONE_NAMES[ch]:>8}  "
                  f"{inside_cone[ch]:>8.4f}  {outside_cone[ch]:>8.4f}  "
                  f"{diff:>+8.4f}  {note}")

        max_trichrom_diffs.append(max(trichrom_diff))

        # Also show sRGB for visual reference
        try:
            inside_srgb = color_space.convert(
                inside_cone.reshape(1, -1), ColorSpaceType.CONE, ColorSpaceType.SRGB)[0]
            outside_srgb = color_space.convert(
                outside_cone.reshape(1, -1), ColorSpaceType.CONE, ColorSpaceType.SRGB)[0]
            srgb_diff = inside_srgb - outside_srgb
            print(f"{'':>3} {'sRGB (visual)':>20} {'R':>8}  {inside_srgb[0]:>8.4f}  {outside_srgb[0]:>8.4f}  {srgb_diff[0]:>+8.4f}")
            print(f"{'':>3} {'':>20} {'G':>8}  {inside_srgb[1]:>8.4f}  {outside_srgb[1]:>8.4f}  {srgb_diff[1]:>+8.4f}")
            print(f"{'':>3} {'':>20} {'B':>8}  {inside_srgb[2]:>8.4f}  {outside_srgb[2]:>8.4f}  {srgb_diff[2]:>+8.4f}")
            srgb_max_diff = np.max(np.abs(srgb_diff))
            metamer_ok = np.max(trichrom_diff) < 0.01
            print(f"    Max trichromat cone diff: {max(trichrom_diff):.6f}  "
                  f"Q diff: {abs(inside_cone[metameric_axis] - outside_cone[metameric_axis]):.4f}  "
                  f"sRGB max diff: {srgb_max_diff:.4f}  "
                  f"{'METAMER OK' if metamer_ok else '!!! NOT A METAMER !!!'}")
        except Exception as e:
            print(f"    (sRGB conversion failed: {e})")

        print("-" * 70)
        pair_idx += 1

    print("\n=== SUMMARY ===")
    if max_trichrom_diffs:
        print(f"Pairs tested: {pair_idx}")
        print(f"Max trichromat cone diff (S,M,L): mean={np.mean(max_trichrom_diffs):.6f}  "
              f"max={np.max(max_trichrom_diffs):.6f}")
        print(f"Q-cone differences:              mean={np.mean(all_q_diffs):.4f}  "
              f"min={np.min(all_q_diffs):.4f}  max={np.max(all_q_diffs):.4f}")

        bad = [d for d in max_trichrom_diffs if d > 0.01]
        print(f"\nPairs where trichromat diff > 0.01: {len(bad)}/{pair_idx}")
        if not bad:
            print("All pairs are valid metamers (trichromat channels differ by < 0.01)")
        else:
            print("!!! Some pairs are NOT valid metamers !!!")
    else:
        print("No pairs generated — check primaries directory and generator config.")


if __name__ == "__main__":
    primaries_dir = sys.argv[1] if len(sys.argv) > 1 else (
        "../extern/TetriumColor/measurements/2026-02-11/primaries"
    )
    testing_dim = int(sys.argv[2]) if len(sys.argv) > 2 else 3
    metameric_axis = int(sys.argv[3]) if len(sys.argv) > 3 else 2

    check_metamers(primaries_dir, testing_dim, metameric_axis)
