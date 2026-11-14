#!/usr/bin/env python3
"""Slide figures for the 10-minute hyper-observer talk.

Each figure makes exactly one point and is sized to a 16:9 slide body
(13.33 in wide), so font sizes in points match the slide. Colors keep one
meaning across the deck:
  red   = "can't see" (failed / censored null-space test)
  M/L   = cone-family colors from PlotStyle
  gray  = context

    python paper-viz/talk_slide_figures.py
    python paper-viz/talk_slide_figures.py --primaries extern/TetriumColor/measurements/2026-04-28/primaries

Outputs land in paper-viz/output/talk_slides/.
"""
from __future__ import annotations

import argparse
import importlib.util
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from matplotlib import font_manager
from matplotlib.patches import Rectangle

REPO_ROOT = Path(__file__).resolve().parents[1]
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(REPO_ROOT / "extern" / "TetriumColor"))

from TetriumColor.Observer.Observer import Observer  # noqa: E402
from TetriumColor.Observer.ObserverGenotypes import ObserverGenotypes  # noqa: E402
from TetriumColor.Plotting.PlotStyle import (  # noqa: E402
    ALL_PEAKS,
    L_PEAKS,
    M_PEAKS,
    PAPER_CONE_FAMILY_COLORS,
    PAPER_RED,
    WAVELENGTHS,
)


def _load_sibling(name: str):
    spec = importlib.util.spec_from_file_location(name, HERE / f"{name}.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


OUT_DIR = HERE / "output" / "talk_slides"
DEFAULT_PRIMARIES = REPO_ROOT / "extern/TetriumColor/measurements/2026-04-28/primaries"
SLIDE_W = 13.33
BODY_H = 5.6
INK = "#202124"
MUTED = "#6b6f75"
FAINT = "#d9dbde"
SAW = "#eef1f4"
RED = PAPER_RED
M_COLOR = PAPER_CONE_FAMILY_COLORS["M"][0]
L_COLOR = PAPER_CONE_FAMILY_COLORS["L"][-1]
S_COLOR = "#8a8f96"
ACCENT = "#2166ac"  # "our method" / emphasis; never means can't-see
L_BAR = "#e08214"
PAIR_COLORS = ("#2166ac", "#7b3294")
FONT_DIR = Path("/usr/share/fonts/opentype/linux-libertine")
TOTAL_TESTS = 220  # C(12, 3) candidate null-space tests


def setup_style() -> None:
    for name in ("LinBiolinum_R.otf", "LinBiolinum_RB.otf", "LinBiolinum_RI.otf"):
        path = FONT_DIR / name
        if path.exists():
            font_manager.fontManager.addfont(str(path))
    plt.rcParams.update({
        "font.family": "Linux Biolinum O",
        "font.size": 20,
        "axes.titlesize": 22,
        "axes.labelsize": 20,
        "xtick.labelsize": 18,
        "ytick.labelsize": 18,
        "axes.edgecolor": INK,
        "axes.labelcolor": INK,
        "text.color": INK,
        "xtick.color": INK,
        "ytick.color": INK,
        "axes.spines.top": False,
        "axes.spines.right": False,
        "axes.linewidth": 1.2,
        "savefig.dpi": 200,
        "savefig.facecolor": "white",
        "text.usetex": False,
        "mathtext.fontset": "custom",
        "mathtext.rm": "Linux Biolinum O",
    })


def class_label(genotype) -> str:
    """(530, 559) -> 'M530 · L559'; (530, 533) -> 'M530 · M533'."""
    parts = []
    for peak in genotype:
        peak = float(peak)
        num = f"{peak:.0f}" if peak.is_integer() else f"{peak:g}"
        parts.append(("M" if peak < 540 else "L") + num)
    return " · ".join(parts)


def trichromat_prior() -> list[tuple[tuple[float, ...], float]]:
    pdf = ObserverGenotypes(dimensions=[1, 2, 3, 4, 5]).get_pdf("both")
    items = [(tuple(float(p) for p in k), float(v)) for k, v in pdf.items() if len(k) == 2]
    total = sum(v for _, v in items)
    return sorted(((k, v / total) for k, v in items), key=lambda kv: -kv[1])


def save(fig, stem: str) -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for ext in ("png", "pdf"):
        fig.savefig(OUT_DIR / f"{stem}.{ext}", bbox_inches="tight", pad_inches=0.15)
    plt.close(fig)
    print(f"wrote {OUT_DIR / stem}.png")


# --------------------------------------------------------------------------
# 1. "Normal" trichromats carry different L and M opsin variants
# --------------------------------------------------------------------------
def fig_cone_variants() -> None:
    hyper = Observer.hyperobserver(wavelengths=WAVELENGTHS)
    sens = np.asarray(hyper.sensor_matrix)
    fig, ax = plt.subplots(figsize=(SLIDE_W, BODY_H))
    for i, peak in enumerate(ALL_PEAKS):
        curve = sens[i] / sens[i].max()
        if peak in M_PEAKS:
            color = PAPER_CONE_FAMILY_COLORS["M"][M_PEAKS.index(peak)]
        elif peak in L_PEAKS:
            color = PAPER_CONE_FAMILY_COLORS["L"][L_PEAKS.index(peak)]
        else:
            color = S_COLOR
        ax.plot(WAVELENGTHS, curve, color=color, lw=2.4, alpha=0.9)

    ax.set_xlim(400, 700)
    ax.set_ylim(0, 1.32)
    ax.set_yticks([])
    ax.spines["left"].set_visible(False)
    ax.set_xlabel("Wavelength (nm)")
    # Direct labels instead of a legend.
    ax.text(442, 1.06, "S", ha="center", fontsize=22, color=S_COLOR)
    ax.annotate("", xy=(536, 1.08), xytext=(530, 1.08),
                arrowprops=dict(arrowstyle="|-|", color=M_COLOR, lw=2, mutation_scale=6))
    ax.text(533, 1.13, "M opsin: 3 variants\n530–536 nm", ha="right", va="bottom",
            fontsize=20, color=M_COLOR)
    ax.annotate("", xy=(559, 1.08), xytext=(547, 1.08),
                arrowprops=dict(arrowstyle="|-|", color=L_COLOR, lw=2, mutation_scale=6))
    ax.text(553, 1.13, "L opsin: 8 variants\n547–559 nm", ha="left", va="bottom",
            fontsize=20, color=L_COLOR)
    save(fig, "01_cone_variants")


# --------------------------------------------------------------------------
# 2. Only two-thirds of trichromats are the "standard observer"
# --------------------------------------------------------------------------
def fig_class_prior(prior) -> None:
    top = prior[:10]
    rest = sum(p for _, p in prior[10:])
    labels = [class_label(k) for k, _ in top] + [f"{len(prior) - 10} other classes"]
    probs = [p for _, p in top] + [rest]
    y = np.arange(len(labels))[::-1]

    fig, ax = plt.subplots(figsize=(SLIDE_W, BODY_H))
    colors = [MUTED] + [ACCENT] * 9 + [FAINT]
    ax.barh(y, np.array(probs) * 100, color=colors, height=0.72)
    for yi, p in zip(y, probs):
        ax.text(p * 100 + 0.8, yi, f"{p * 100:.1f}%" if p >= 0.01 else f"{p * 100:.2f}%",
                va="center", fontsize=17, color=INK)
    ax.set_yticks(y)
    ax.set_yticklabels(labels, fontsize=17)
    ax.set_xlim(0, 95)
    ax.set_xlabel("Share of trichromats (genetic prior, %)")
    ax.spines["left"].set_visible(False)
    ax.tick_params(axis="y", length=0)
    ax.text(73, y[0], "the “standard observer”", va="center", fontsize=19, color=MUTED)
    ax.text(31, y[1], "1 in 4 trichromats", va="center", fontsize=19, color=ACCENT)
    cum10 = sum(probs[:10]) * 100
    ax.text(94, y[6], f"Top 10 classes\ncover {cum10:.1f}%",
            ha="right", va="center", fontsize=20, color=INK)
    save(fig, "02_class_prior")


# --------------------------------------------------------------------------
# 3. One pair of lights: invisible to one class, visible to another
# --------------------------------------------------------------------------
def fig_invisible_to_one(primaries_dir: Path) -> None:
    sop = _load_sibling("simple_observer_projection")
    from TetriumColor.ColorSpace import ColorSpace
    from TetriumColor.Measurement import load_primaries_from_csv

    primaries = load_primaries_from_csv(str(primaries_dir), extract_zero=False, primary_order="BGOR")
    wl = primaries[0].wavelengths
    genotypes = ObserverGenotypes(wavelengths=wl, dimensions=[3], template="baylor")
    kw = dict(degree=2.0, illuminant="raw", template="baylor")
    target = genotypes.get_observer_for_peaks((530, 547, 559), **kw)
    cs = ColorSpace(target, display_primaries=primaries, metameric_axis=2)
    w1, w2, spec1, spec2 = sop._metamer_pair_for_observer(cs, primaries, 0.8)

    obs = {
        "A": ((530, 559), genotypes.get_observer_for_peaks((530, 559), **kw)),
        "B": ((530, 555), genotypes.get_observer_for_peaks((530, 555), **kw)),
    }
    diffs = {k: sop._raw_cone_diff(o, primaries, w1, w2) for k, (_, o) in obs.items()}
    # Relative cone contrast: difference over mean response of the pair.
    disp = sop._display_matrix(primaries, wl)
    means = {k: 0.5 * np.abs(o.sensor_matrix @ disp @ (w1 + w2)) for k, (_, o) in obs.items()}
    contrast = {k: 100 * diffs[k] / means[k] for k in diffs}
    ymax = max(c.max() for c in contrast.values()) * 1.25

    fig = plt.figure(figsize=(SLIDE_W, BODY_H))
    gs = fig.add_gridspec(1, 3, width_ratios=[1.35, 1, 1], wspace=0.35)
    ax0 = fig.add_subplot(gs[0])
    ax0.plot(spec1.wavelengths, spec1.data, color=PAIR_COLORS[0], lw=2.4)
    ax0.plot(spec2.wavelengths, spec2.data, color=PAIR_COLORS[1], lw=2.4)
    ax0.set_xlim(400, 700)
    ax0.set_ylim(bottom=0)
    ax0.set_yticks([])
    ax0.spines["left"].set_visible(False)
    ax0.set_xlabel("Wavelength (nm)")
    ax0.set_title("Two different lights\nfrom our projector", loc="left")

    for col, key in ((1, "A"), (2, "B")):
        ax = fig.add_subplot(gs[col])
        geno, _ = obs[key]
        c = contrast[key]
        bar_colors = [S_COLOR, M_COLOR, L_BAR]
        ax.bar([0, 1, 2], c, color=bar_colors, width=0.62)
        ax.set_xticks([0, 1, 2])
        ax.set_xticklabels(["S", "M", "L"])
        ax.set_ylim(0, ymax)
        ax.set_ylabel("Cone contrast (%)" if col == 1 else "")
        sees = c.max() > 1.0
        verdict = "sees a difference" if sees else "can't tell them apart"
        ax.set_title(f"{class_label(geno)}\n{verdict}", loc="left",
                     color=INK if sees else RED)
        if not sees:
            ax.text(1, ymax * 0.12, "≈ 0 for every cone", ha="center", fontsize=18, color=RED)
    print("cone contrast %:", {k: np.round(v, 2).tolist() for k, v in contrast.items()})
    save(fig, "03_invisible_to_one_class")


# --------------------------------------------------------------------------
# 4. Ordering tests by the prior: 10 tests reach 99%
# --------------------------------------------------------------------------
def fig_tests_needed(prior) -> None:
    probs = np.array([p for _, p in prior])
    k = np.arange(0, 31)
    ordered = np.concatenate([[0], np.cumsum(probs)])[np.minimum(k, len(probs))] * 100
    random = k / TOTAL_TESTS * 100
    e_ordered = float(np.sum(np.arange(1, len(probs) + 1) * probs))
    e_random = (TOTAL_TESTS + 1) / 2

    fig, ax = plt.subplots(figsize=(SLIDE_W, BODY_H))
    ax.step(k, ordered, where="post", color=ACCENT, lw=3)
    ax.step(k, random, where="post", color=MUTED, lw=3)
    ax.set_xlim(0, 30)
    ax.set_ylim(0, 105)
    ax.set_xlabel("Number of tests shown")
    ax.set_ylabel("Trichromats classified (%)")
    ax.text(12, ordered[12] - 9, f"Most common class first\n(expected {e_ordered:.1f} tests)",
            color=ACCENT, fontsize=20, va="top")
    ax.text(12, random[12] + 14, f"Random order of {TOTAL_TESTS} tests (expected {e_random:.0f})",
            color=MUTED, fontsize=20)
    ax.plot([10], [ordered[10]], "o", color=ACCENT, ms=10)
    ax.annotate(f"{ordered[10]:.1f}% after 10 tests", xy=(10, ordered[10]), xytext=(14, 96),
                fontsize=20, va="center", arrowprops=dict(arrowstyle="-", color=INK, lw=1))
    print(f"expected tests: ordered={e_ordered:.2f}, random={e_random:.1f}")
    save(fig, "04_tests_needed")


# --------------------------------------------------------------------------
# 5. Results: each observer fails a different subset of tests
# --------------------------------------------------------------------------
def _results_matrix():
    sec = _load_sibling("subject_equivalence_classes")
    df = sec._load_quest_summary(sec.SUMMARY_CSV)
    order = sec._trichromat_prior_order(10)
    _thr, censored, subjects = sec._build_subject_matrix(df, order)
    genos = [tuple(float(p) for p in t.split(",")) for t in order]
    return censored, genos, len(subjects)


def fig_results(highlight: str | None, stem: str) -> None:
    failed, genos, n_subj = _results_matrix()
    n_test = len(genos)
    fig, ax = plt.subplots(figsize=(SLIDE_W, BODY_H + 0.8))
    for r in range(n_subj):
        row_on = highlight is None or (highlight == "S8" and r == 7) or (
            highlight == "normals" and r < 7)
        alpha = 1.0 if row_on else 0.15
        y0 = n_subj - 1 - r
        for c in range(n_test):
            color = RED if failed[r, c] else SAW
            ax.add_patch(Rectangle((c, y0), 0.94, 0.9, color=color, alpha=alpha, lw=0))
        ax.text(-0.25, y0 + 0.45, f"S{r + 1}", ha="right", va="center",
                fontsize=19, alpha=max(alpha, 0.35))
    for c, g in enumerate(genos):
        ax.text(c + 0.3, n_subj + 0.12, class_label(g), rotation=55, ha="left", va="bottom",
                fontsize=15, rotation_mode="anchor")
    ax.text(-0.25, n_subj + 0.12, "Observer", ha="right", va="bottom", fontsize=17, color=MUTED)
    ax.text(0, -0.55, "One null-space test per class, most common first  →",
            fontsize=17, color=MUTED, va="center")
    ax.add_patch(Rectangle((0, -1.42), 0.42, 0.42, color=RED, lw=0))
    ax.text(0.6, -1.21, "can't see it, even at max contrast", va="center", fontsize=17)
    if highlight == "S8":
        ax.add_patch(Rectangle((n_test - 1 - 0.06, -0.05), 1.06, 1.0, fill=False, ec=INK, lw=2.5))
        ax.text(n_test + 0.3, 0.45, "← fails only M530 · M533,\n    the protanomalous class",
                va="center", fontsize=19)
    ax.set_xlim(-1.8, n_test + 6.2)
    ax.set_ylim(-1.7, n_subj + 3.2)
    ax.set_aspect("equal")
    ax.axis("off")
    save(fig, stem)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--primaries", type=Path, default=DEFAULT_PRIMARIES)
    args = parser.parse_args()
    setup_style()
    prior = trichromat_prior()
    fig_cone_variants()
    fig_class_prior(prior)
    fig_invisible_to_one(args.primaries)
    fig_tests_needed(prior)
    fig_results(None, "05a_results_all")
    fig_results("normals", "05b_results_normals_differ")
    fig_results("S8", "05c_results_protanomalous")


if __name__ == "__main__":
    main()
