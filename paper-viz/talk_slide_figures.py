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
from matplotlib.font_manager import FontProperties
from matplotlib.patches import Circle, Rectangle, Wedge
from matplotlib.path import Path as MplPath
from matplotlib.textpath import TextPath

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
# Linux Biolinum: the OpenType build on Linux, the TrueType build on macOS.
FONT_DIRS = (
    Path("/usr/share/fonts/opentype/linux-libertine"),
    Path.home() / "Library" / "Fonts",
    Path("/Library/Fonts"),
)
FONT_STEMS = ("LinBiolinum_R", "LinBiolinum_RB", "LinBiolinum_RI")
FONT_FAMILY = "Linux Biolinum O"
TOTAL_TESTS = 220  # C(12, 3) candidate null-space tests


def setup_style() -> None:
    global FONT_FAMILY
    found = None
    for font_dir in FONT_DIRS:
        paths = [font_dir / f"{stem}{ext}" for stem in FONT_STEMS for ext in (".otf", ".ttf")]
        paths = [path for path in paths if path.exists()]
        if paths:
            found = paths
            break
    if not found:
        raise RuntimeError(f"Linux Biolinum not found in any of {[str(d) for d in FONT_DIRS]}; "
                           "install Linux Libertine/Biolinum rather than falling back to DejaVu.")
    for path in found:
        font_manager.fontManager.addfont(str(path))
    FONT_FAMILY = FontProperties(fname=str(found[0])).get_name()
    plt.rcParams.update({
        "font.family": FONT_FAMILY,
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
        "mathtext.rm": FONT_FAMILY,
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
# 0a. Hook: a pattern shown to seven observers (simulated appearance)
# --------------------------------------------------------------------------
HOOK_BG = np.array([204, 200, 206]) / 255
HOOK_BLOB = np.array([237, 178, 203]) / 255


def _blob_image(n: int, centers, blob_at: int | None, opacity: float = 1.0,
                radius: float = 0.13) -> np.ndarray:
    """Background-colored square with a soft-edged blob at one of `centers`."""
    yy, xx = np.mgrid[0:n, 0:n] / (n - 1)
    img = np.broadcast_to(HOOK_BG, (n, n, 3)).copy()
    if blob_at is not None and opacity > 0:
        cx, cy = centers[blob_at]
        r = np.hypot(xx - cx, yy - cy)
        alpha = np.clip((radius - r) / (0.25 * radius) + 0.5, 0, 1) * opacity
        img = img * (1 - alpha[..., None]) + HOOK_BLOB * alpha[..., None]
    return img


def _person(ax, x: float, y: float, s: float, color: str) -> None:
    ax.add_patch(Circle((x, y + 0.62 * s), 0.2 * s, color=color, lw=0))
    ax.add_patch(Wedge((x, y), 0.36 * s, 0, 180, color=color, lw=0))


def fig_hook_stimulus() -> None:
    centers = [(0.5, 0.2), (0.8, 0.5), (0.5, 0.8), (0.2, 0.5)]
    fig = plt.figure(figsize=(SLIDE_W, BODY_H))
    ax = fig.add_axes([0.03, 0.12, 0.36, 0.76])
    ax.imshow(_blob_image(600, centers, blob_at=1), extent=(0, 1, 0, 1), origin="lower")
    for i, (cx, cy) in enumerate(centers):
        if i != 1:
            ax.add_patch(Circle((cx, cy), 0.13, fill=False, ec="#a9a6ad", lw=1.6, ls=(0, (4, 4))))
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis("off")
    fig.text(0.03, 0.05, "Simulated: the real stimulus needs four primaries.",
             fontsize=18, color=MUTED, va="center")
    fig.text(0.03 + 0.18, 0.97, "Where is the blob? (4 choices)", fontsize=20,
             ha="center", va="top", color=INK)

    ax2 = fig.add_axes([0.45, 0.12, 0.54, 0.84])
    ax2.set_xlim(0, 7.4)
    ax2.set_ylim(0, 4)
    ax2.set_aspect("equal")
    ax2.axis("off")
    for i in range(7):
        x = 0.6 + i * 1.03
        _person(ax2, x, 1.75, 1.0, MUTED)
        ax2.text(x, 1.3, f"S{i + 1}", ha="center", va="top", fontsize=20)
    save(fig, "00a_hook_stimulus")


# --------------------------------------------------------------------------
# 0b. Hook answer: four of seven never found it
# --------------------------------------------------------------------------
HOOK_TEST = (530.0, 559.0)
HOOK_EXPECTED = {"S7": 0.50, "S6": 0.69, "S2": 0.98}


def fig_hook_answer() -> None:
    sec = _load_sibling("subject_equivalence_classes")
    df = sec._load_quest_summary(sec.SUMMARY_CSV)
    order = sec._trichromat_prior_order(10)
    thresholds, censored, subjects = sec._build_subject_matrix(df, order)
    genos = [tuple(float(p) for p in t.split(",")) for t in order]
    col = genos.index(HOOK_TEST)
    n = 7  # S1-S7: the normal observers; S8 is anomalous.
    thr = thresholds[:n, col]
    cen = censored[:n, col]
    for label, expected in HOOK_EXPECTED.items():
        r = int(label[1:]) - 1
        assert not cen[r] and abs(thr[r] - expected) < 0.01, (label, thr[r], expected)
    assert cen.sum() == 4, cen
    print("hook thresholds:", {f"S{r + 1}": ("censored" if cen[r] else round(float(thr[r]), 2))
                               for r in range(n)})

    fig, ax = plt.subplots(figsize=(SLIDE_W, BODY_H))
    y = np.arange(n)[::-1]
    past = 1.18
    for r in range(n):
        if cen[r]:
            ax.barh(y[r], past, color=RED, height=0.66)
            ax.text(past + 0.02, y[r], "never found it", va="center", fontsize=19, color=RED)
        else:
            ax.barh(y[r], thr[r], color=MUTED, height=0.66)
            ax.text(thr[r] - 0.015, y[r], f"{thr[r]:.2f}", ha="right", va="center",
                    fontsize=19, color="white")
    ax.plot([1.0, 1.0], [-0.6, n - 0.5], color=INK, lw=1.6, ls=(0, (5, 4)), zorder=3)
    ax.text(1.0, n - 0.4, "max contrast", ha="center", va="bottom", fontsize=19)
    ax.set_yticks(y)
    ax.set_yticklabels([f"S{r + 1}" for r in range(n)])
    ax.tick_params(axis="y", length=0)
    ax.spines["left"].set_visible(False)
    ax.set_xlim(0, 1.5)
    ax.set_xticks([0, 0.25, 0.5, 0.75, 1.0])
    ax.set_ylim(-0.6, n + 0.3)
    ax.set_xlabel("Contrast needed to find it (fraction of max)")
    save(fig, "00b_hook_answer")


# --------------------------------------------------------------------------
# 0c. A generated pseudo-isochromatic plate (protan confusion line)
# --------------------------------------------------------------------------
# Vienot, Brettel & Mollon 1999: linear RGB -> LMS, and the protanope projection.
VIENOT_RGB2LMS = np.array([
    [17.8824, 43.5161, 4.11935],
    [3.45565, 27.1554, 3.86714],
    [0.0299566, 0.184309, 1.46709],
])
VIENOT_PROTAN = np.array([[0, 2.02344, -2.52581], [0, 1, 0], [0, 0, 1]])


def _srgb_encode(lin: np.ndarray) -> np.ndarray:
    lin = np.clip(lin, 0, 1)
    return np.where(lin <= 0.0031308, 12.92 * lin, 1.055 * lin ** (1 / 2.4) - 0.055)


def _plate_dots(rng, n_try: int = 40000):
    dots = []
    for _ in range(n_try):
        r = rng.uniform(0.012, 0.032)
        x, y = rng.uniform(-1, 1, 2)
        if np.hypot(x, y) + r > 0.98:
            continue
        if any(np.hypot(x - a, y - b) < r + c + 0.006 for a, b, c in dots):
            continue
        dots.append((x, y, r))
    return np.array(dots)


def _digit_mask(points: np.ndarray, digit: str = "5") -> np.ndarray:
    path = TextPath((0, 0), digit, size=1.0, prop=FontProperties(family=FONT_FAMILY, weight="bold"))
    verts = path.vertices
    lo, hi = verts.min(0), verts.max(0)
    scale = 1.25 / (hi - lo)[1]
    center = (lo + hi) / 2
    inside = np.zeros(len(points), dtype=int)
    for poly in path.to_polygons():
        poly = (poly - center) * scale
        inside += MplPath(poly).contains_points(points)
    return inside % 2 == 1


def fig_plate() -> None:
    rng = np.random.default_rng(7)
    dots = _plate_dots(rng)
    in_digit = _digit_mask(dots[:, :2])
    lms2rgb = np.linalg.inv(VIENOT_RGB2LMS)
    base = np.array([0.42, 0.30, 0.12])  # linear RGB
    l_dir = lms2rgb @ np.array([1.0, 0.0, 0.0])  # pure L-cone change
    jitter = rng.uniform(0.72, 1.12, len(dots))
    # Largest symmetric L step that keeps every jittered dot inside [0, 1].
    step = np.inf
    for sign in (1, -1):
        for i in range(3):
            d = sign * l_dir[i]
            if d > 0:
                step = min(step, (1 / jitter.max() - base[i]) / d)
            elif d < 0:
                step = min(step, base[i] / -d)
    step *= 0.9
    lin = base + np.where(in_digit, 1, -1)[:, None] * step * l_dir
    lin = lin * jitter[:, None]
    assert lin.min() >= 0 and lin.max() <= 1, (lin.min(), lin.max())
    lms = lin @ VIENOT_RGB2LMS.T
    dl = lms[in_digit] / jitter[in_digit, None] - lms[~in_digit][:1] / jitter[~in_digit][:1, None]
    assert np.allclose(dl[:, 1:], 0, atol=1e-9)  # digit differs only in L
    protan = (lms @ VIENOT_PROTAN.T) @ lms2rgb.T
    assert protan.min() >= -1e-9 and protan.max() <= 1 + 1e-9, (protan.min(), protan.max())
    gap = np.abs(protan[in_digit] / jitter[in_digit, None] - protan[~in_digit][0] / jitter[~in_digit][0])
    assert gap.max() < 1e-9  # digit vanishes for the protanope
    print(f"plate: {len(dots)} dots, {in_digit.sum()} in digit, L step {step:.4f}")

    fig, axes = plt.subplots(1, 2, figsize=(SLIDE_W, BODY_H))
    for ax, colors, title in ((axes[0], lin, "Normal trichromat"),
                              (axes[1], protan, "Simulated protanope")):
        rgb = _srgb_encode(colors)
        for (x, y, r), c in zip(dots, rgb):
            ax.add_patch(Circle((x, y), r, color=c, lw=0))
        ax.set_xlim(-1.02, 1.02)
        ax.set_ylim(-1.02, 1.02)
        ax.set_aspect("equal")
        ax.axis("off")
        ax.set_title(title, fontsize=22, pad=10)
    fig.subplots_adjust(wspace=0.08)
    save(fig, "00c_plate")


# --------------------------------------------------------------------------
# 0a. Hook: a dot plate whose "5" is hidden from M530 · L559 (measured primaries)
# --------------------------------------------------------------------------
PLATE_K = 12.0  # display exaggeration of cone contrast; stated in the caption
PLATE_TARGET = (530, 559)
PLATE_OBSERVERS = ((530, 559), (530, 555), (530, 533))
PLATE_WL = np.arange(400, 701)


def _hidden_plate(primaries_dir: Path, seed: int = 3):
    """Dots, digit mask, per-observer cone contrast and simulated sRGB colors.

    Background dots show display weights w0 (every primary at 0.5); digit dots
    show w0 + a * n, where n spans the null space of the M530 · L559 cone matrix
    times the display matrix, so the two are metamers for that class.
    """
    import colour
    from scipy.linalg import null_space
    from TetriumColor.Measurement import load_primaries_from_csv

    sop = _load_sibling("simple_observer_projection")
    primaries = load_primaries_from_csv(str(primaries_dir), extract_zero=False, primary_order="BGOR")
    disp = sop._display_matrix(primaries, PLATE_WL)
    genotypes = ObserverGenotypes(wavelengths=PLATE_WL, dimensions=[3], template="baylor")
    kw = dict(degree=2.0, illuminant="raw", template="baylor")
    cones = {g: np.asarray(genotypes.get_observer_for_peaks(g, **kw).sensor_matrix) for g in PLATE_OBSERVERS}

    n = null_space(cones[PLATE_TARGET] @ disp)[:, 0]
    n = n / np.abs(n).max()
    if n[np.argmax(np.abs(n))] < 0:
        n = -n
    w0 = np.full(4, 0.5)
    amp = min(np.min(np.where(n > 0, (1 - w0) / np.where(n > 0, n, 1), np.inf)),
              np.min(np.where(n < 0, w0 / np.where(n < 0, -n, 1), np.inf)))
    w1 = w0 + amp * n
    assert w1.min() >= -1e-12 and w1.max() <= 1 + 1e-12, w1
    contrast = {g: (c @ disp @ w1 - c @ disp @ w0) / (c @ disp @ w0) for g, c in cones.items()}
    max_pct = {g: float(100 * np.abs(v).max()) for g, v in contrast.items()}
    print("plate digit vs background, max cone contrast %:",
          {class_label(g): round(v, 3) for g, v in max_pct.items()}, f"(a = {amp:.3f})")
    assert max_pct[PLATE_TARGET] < 0.05, max_pct
    assert all(v > 0.5 for g, v in max_pct.items() if g != PLATE_TARGET), max_pct

    # Standard-observer rendering: Stockman & Sharpe LMS -> XYZ fit on the display lights.
    ss = colour.MSDS_CMFS["Stockman & Sharpe 2 Degree Cone Fundamentals"]
    cmf = colour.MSDS_CMFS["CIE 1931 2 Degree Standard Observer"]
    std_lms = np.array([[ss[w][i] for w in PLATE_WL] for i in (2, 1, 0)])  # S, M, L
    xyz = np.array([[cmf[w][i] for w in PLATE_WL] for i in range(3)])
    lms2xyz = np.linalg.lstsq((std_lms @ disp).T, (xyz @ disp).T, rcond=None)[0]
    base_lms = std_lms @ disp @ w0
    y_norm = (base_lms @ lms2xyz)[1] / 0.62

    rng = np.random.default_rng(seed)
    dots = _plate_dots(rng)
    in_digit = _digit_mask(dots[:, :2])
    jitter = rng.uniform(0.72, 1.0, len(dots))  # identical for every observer
    colors = {}
    for g in PLATE_OBSERVERS:
        con = np.where(in_digit[:, None], contrast[g][None, :], 0.0)
        lms = base_lms * (1 + PLATE_K * con) * jitter[:, None]
        rgb = colour.XYZ_to_sRGB(lms @ lms2xyz / y_norm)
        assert rgb.min() > -0.01 and rgb.max() < 1.01, (class_label(g), rgb.min(), rgb.max())
        colors[g] = np.clip(rgb, 0, 1)
    ref = colors[PLATE_TARGET][~in_digit]
    assert all(np.allclose(c[~in_digit], ref) for c in colors.values())  # same background
    return dots, colors, max_pct


def _draw_plate(ax, dots: np.ndarray, rgb: np.ndarray) -> None:
    for (x, y, r), c in zip(dots, rgb):
        ax.add_patch(Circle((x, y), r, color=c, lw=0))
    ax.set_xlim(-1.03, 1.03)
    ax.set_ylim(-1.03, 1.03)
    ax.set_aspect("equal")
    ax.set_xticks([])
    ax.set_yticks([])
    for side in ax.spines.values():
        side.set_visible(False)


def fig_plate_question(plate) -> None:
    dots, colors, _ = plate
    fig = plt.figure(figsize=(SLIDE_W, BODY_H))
    h = 0.84
    w = h * BODY_H / SLIDE_W
    ax = fig.add_axes([0.5 - w / 2, 0.12, w, h])
    _draw_plate(ax, dots, colors[PLATE_TARGET])
    fig.text(0.5, 0.04, "Simulated: the real plate needs four primaries.",
             ha="center", va="center", fontsize=18, color=MUTED)
    save(fig, "00a_plate_question")


def fig_plate_reveal(plate) -> None:
    dots, colors, max_pct = plate
    fig = plt.figure(figsize=(SLIDE_W, BODY_H))
    h = 0.62
    w = h * BODY_H / SLIDE_W
    y0 = 0.25
    for cx, geno in ((0.3, PLATE_TARGET), (0.7, (530, 555))):
        ax = fig.add_axes([cx - w / 2, y0, w, h])
        _draw_plate(ax, dots, colors[geno])
        hidden = geno == PLATE_TARGET
        if hidden:
            for side in ax.spines.values():
                side.set_visible(True)
                side.set_color(RED)
                side.set_linewidth(4)
        fig.text(cx, y0 + h + 0.03, class_label(geno), ha="center", va="bottom", fontsize=22)
        fig.text(cx, y0 - 0.03, "can't see the 5" if hidden else "sees a 5", ha="center",
                 va="top", fontsize=22, color=RED if hidden else INK)
        fig.text(cx, y0 - 0.115, f"{max_pct[geno]:.2f}% cone contrast", ha="center", va="top",
                 fontsize=19, color=RED if hidden else INK)
    fig.text(0.5, 0.03, f"Simulated: real contrast ≈1%, magnified ×{PLATE_K:.0f} for this screen; "
             "the real plate needs four primaries.", ha="center", va="center",
             fontsize=18, color=MUTED)
    save(fig, "00a2_plate_reveal")


# --------------------------------------------------------------------------
# 0d. Each normal type has its own hidden pattern (measured primaries)
# --------------------------------------------------------------------------
def fig_hidden_per_type(primaries_dir: Path) -> None:
    sop = _load_sibling("simple_observer_projection")
    from TetriumColor.ColorSpace import ColorSpace
    from TetriumColor.Measurement import load_primaries_from_csv

    primaries = load_primaries_from_csv(str(primaries_dir), extract_zero=False, primary_order="BGOR")
    wl = primaries[0].wavelengths
    genotypes = ObserverGenotypes(wavelengths=wl, dimensions=[3], template="baylor")
    kw = dict(degree=2.0, illuminant="raw", template="baylor")
    disp = sop._display_matrix(primaries, wl)
    targets = [(530, 547, 559), (530, 547, 555)]
    observers = [(530, 559), (530, 555)]
    contrast = np.zeros((2, 2))
    for i, target in enumerate(targets):
        cs = ColorSpace(genotypes.get_observer_for_peaks(target, **kw),
                        display_primaries=primaries, metameric_axis=2)
        w1, w2, _, _ = sop._metamer_pair_for_observer(cs, primaries, 0.8)
        for j, geno in enumerate(observers):
            obs = genotypes.get_observer_for_peaks(geno, **kw)
            diff = sop._raw_cone_diff(obs, primaries, w1, w2)
            mean = 0.5 * np.abs(obs.sensor_matrix @ disp @ (w1 + w2))
            contrast[i, j] = float(np.max(100 * diff / mean))
    print("hidden-per-type max cone contrast %:", np.round(contrast, 3).tolist())
    assert np.all(np.diag(contrast) < 0.05) and np.all(contrast[[0, 1], [1, 0]] > 1.0), contrast

    fig = plt.figure(figsize=(SLIDE_W, BODY_H))
    h = 0.32
    w = h * BODY_H / SLIDE_W  # square cells
    col_x = (0.47, 0.72)
    row_y = (0.53, 0.09)
    for i, target in enumerate(targets):
        for j, geno in enumerate(observers):
            ax = fig.add_axes([col_x[j] - w / 2, row_y[i], w, h])
            c = contrast[i, j]
            ax.imshow(_blob_image(300, [(0.5, 0.5)], 0, opacity=c / contrast.max(), radius=0.24),
                      extent=(0, 1, 0, 1), origin="lower")
            ax.set_xticks([])
            ax.set_yticks([])
            hidden = c < 0.05
            for side in ax.spines.values():
                side.set_visible(True)
                side.set_color(RED if hidden else FAINT)
                side.set_linewidth(4 if hidden else 1)
            note = f"{c:.1f}% contrast" + (" · can't see" if hidden else "")
            ax.text(0.5, -0.04, note, transform=ax.transAxes, ha="center", va="top",
                    fontsize=19, color=RED if hidden else INK)
            if i == 0:
                ax.text(0.5, 1.04, f"Observer\n{class_label(geno)}", transform=ax.transAxes,
                        ha="center", va="bottom", fontsize=20)
        fig.text(col_x[0] - w / 2 - 0.03, row_y[i] + h / 2,
                 f"Pattern hidden from\n{class_label((target[0], target[2]))}",
                 ha="right", va="center", fontsize=20)
    save(fig, "00d_hidden_per_type")


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
    ax.text(12, ordered[12] - 32,
            "idealized: assumes each observer fails\nonly their own class's test",
            color=ACCENT, fontsize=18, style="italic", va="top")
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
    plate = _hidden_plate(args.primaries)
    fig_plate_question(plate)
    fig_plate_reveal(plate)
    fig_hook_answer()
    fig_plate()
    fig_hidden_per_type(args.primaries)
    fig_cone_variants()
    fig_class_prior(prior)
    fig_invisible_to_one(args.primaries)
    fig_tests_needed(prior)
    fig_results(None, "05a_results_all")
    fig_results("normals", "05b_results_normals_differ")
    fig_results("S8", "05c_results_protanomalous")


if __name__ == "__main__":
    main()
