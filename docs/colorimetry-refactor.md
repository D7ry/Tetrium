# TetriumColor Colorimetry Refactor

## Problem Statement

TetriumColor currently mixes reflective and emissive colorimetry assumptions in APIs that look generic. The most visible issue is that display/projector workflows sometimes route raw cone excitations through a mode named `cone_contrast`, even when no adapting background is used and the value is not true cone contrast.

This makes it easy for future code to silently choose the wrong transform for values that have different meanings:

- object reflectance spectra
- emitted spectral power distributions
- raw cone excitations
- true cone contrast values relative to a background
- display/projector coordinates such as `DISP`, `DISP_6P`, `BGYR`, `MaxBasis`, and LED primary weights

The refactor should make the data model explicit enough that API names reveal the required assumptions.

## Goals

- Separate reflective and emissive colorimetry paths so transform calls do not accidentally apply illuminant or reflectance assumptions to display emission workflows.
- Make raw cone excitation a first-class representation instead of hiding it behind `cone_contrast`.
- Keep `Observer` focused on cone fundamentals, wavelength grids, sensor matrices, and low-level integration utilities.
- Move stimulus-type policy into transform/value-layer APIs.
- Support workflows needing normalized cone sensitivities and display/projector transforms, including 2D Contour Estimation.
- Preserve useful existing transforms with a staged migration and deprecation warnings.

## Non-Goals For First Pass

- Do not redesign every color space in the library at once.
- Do not change scientific equations without identifying the specific current assumption or equation that is wrong.
- Do not remove reflective/object-color workflows to simplify emissive workflows.
- Do not keep using `cone_contrast` for raw cone excitation after the migration window.
- Do not extract a new repository before in-place compatibility tests are written.

## Current Repo Findings

- Core transform code is in `extern/TetriumColor/TetriumColor/ColorSpace.py`.
- `Observer` in `extern/TetriumColor/TetriumColor/Observer/Observer.py` owns cone fundamentals and also stores an optional illuminant. Its `raw_illuminant` mode makes emission-style workflows possible, but the API does not make the stimulus kind explicit.
- `ColorSpaceType.CONE` is used as a central internal coordinate, but it can mean normalized display-conversion coordinates, raw cone responses, or cone-like vectors depending on the call path.
- `ColorSpace.get_raw_display_to_cone_matrix()` already computes the direct display-primary to raw cone excitation matrix.
- `QuestColorGenerator` currently allows `color_picking_space='cone_contrast'`, but its own docstring says this computes raw cone-excitation axes in display space.
- Generated metadata already contains `quest_raw_cone_delta`, but also duplicates it into `quest_cone_contrast_delta`.
- Validation and visualization scripts contain logic that switches labels based on `color_picking_space == 'cone_contrast'`, even when the plotted quantity is raw cone response.

## Terminology

Use these names consistently in code, metadata, docs, plots, and file names:

| Term | Meaning | Requires |
| --- | --- | --- |
| Reflectance | Object reflectance, generally bounded 0-1 by wavelength | Illuminant to become cone excitation |
| Illuminant | Light incident on reflectance | Wavelength alignment |
| Emission SPD | Light emitted by display/projector/LED source | Cone fundamentals only |
| Raw cone excitation | Absolute receptor response from integrating stimulus SPD | Wavelength alignment and observer |
| Normalized cone sensitivity | Analysis basis derived from cone fundamentals | Explicit normalization rule |
| Cone contrast | Relative response change `(sample - background) / background` | Background/reference response |
| Display weights | Device primary weights, e.g. `DISP`, `DISP_6P`, LED weights | Display primary calibration |
| Transform basis | Derived coordinate basis such as `BGYR`, `MaxBasis`, `Hering` | Declared source value type |

Avoid using `cone` as an unqualified noun in new public APIs. Prefer `raw_cone`, `normalized_cone_sensitivity`, or `cone_contrast`.

## Target Data Model

Use explicit value names and require the missing scientific context at boundaries:

- `ReflectanceSpectrum`: dimensioned reflectance values over wavelengths. Requires an illuminant before cone excitations can be computed.
- `EmissionSpectrum`: emitted spectral power distribution over wavelengths. Integrates directly against cone fundamentals.
- `RawConeExcitation`: absolute cone response vector from integrating an emission SPD or illuminated reflectance.
- `ConeContrast`: `(cone_sample - cone_background) / cone_background`, requiring a defined background/reference.
- `DisplayPrimaryWeights`: display/projector primary coordinates such as `DISP`, `DISP_6P`, or LED weights.
- `NormalizedConeSensitivity`: cone fundamentals normalized for analysis/contour workflows, not a stimulus response.

First-pass implementation can use lightweight dataclasses around numpy arrays rather than a large type system. Each wrapper should carry the minimum metadata needed to prevent misuse:

- `wavelengths` for spectral values.
- `observer` or `observer_id` for cone-space values when practical.
- `background` for `ConeContrast`.
- `space` or `primary_order` for display weights.
- `normalization` for normalized cone sensitivities.

The wrappers should support `np.asarray(value)` only when that does not erase required context at an unsafe boundary. Otherwise, expose `.data` explicitly.

## Target API Direction

Prefer explicit methods over generic conversion names when stimulus meaning matters:

- `observer.integrate_emission(emission) -> RawConeExcitation`
- `observer.integrate_reflectance(reflectance, illuminant) -> RawConeExcitation`
- `observer.normalized_cone_sensitivities(wavelengths=None) -> NormalizedConeSensitivity`
- `color_space.display_to_raw_cone_matrix()`
- `color_space.display_to_raw_cone(display_weights)`
- `color_space.raw_cone_to_display_weights(raw_cone, method=...)`
- `color_space.display_to_cone_contrast_matrix(background)`
- `color_space.cone_contrast_delta(a, b, background, sample_space=...)`
- `QuestColorGenerator(color_picking_space='raw_cone_excitation')`

Compatibility aliases may remain temporarily, but they must warn and document the canonical replacement.

### Legacy API Mapping

| Existing API/name | New API/name | Migration behavior |
| --- | --- | --- |
| `ColorSpace.get_raw_display_to_cone_matrix()` | `ColorSpace.display_to_raw_cone_matrix()` | Keep old method as alias with no numerical change |
| `ColorSpaceType.CONE` for display response | `RawConeExcitation` or explicit `raw_cone` method | Keep enum during transition, clarify docs |
| `color_picking_space='cone_contrast'` in `QuestColorGenerator` raw-excitation path | `color_picking_space='raw_cone_excitation'` | Warn, map to canonical value |
| `quest_cone_contrast_delta` when equal to raw delta | `quest_raw_cone_delta` | Emit both during migration, mark old key deprecated |
| Plot labels `cone contrast` for raw deltas | `raw cone response` or `raw cone delta` | Rename labels, keep true contrast labels only for background-normalized values |
| `observer.observe(spectra)` | `integrate_emission`, `integrate_reflectance`, or legacy `observe` | Add explicit helpers first, deprecate ambiguous usage later |

### First-Pass API Rules

- Any API named `contrast` must require or derive a background response.
- Any API named `reflectance` must require an illuminant unless it returns an intermediate object.
- Any API named `emission` must not multiply by an illuminant.
- Any display-to-cone method must say whether it returns raw cone excitation or normalized/internal `CONE` coordinates.
- Metadata must store the canonical name and may store deprecated aliases only with a comment or schema note.

## Repository Strategy

Use staged extraction:

1. Refactor TetriumColor in place under `extern/TetriumColor`.
2. Add regression tests that lock current display and object-color behavior.
3. Migrate active scripts and generated metadata to explicit names.
4. Extract the stable core into a new package only after APIs and tests settle.

The future repository should contain observers, spectra, value types, display/projector transforms, and colorimetry tests. Tetrium app code, GUIs, paper figures, and experiment-specific scripts should remain in this repo unless they become clean examples.

### Candidate Extracted Repository Shape

Use this as the starting structure when extraction begins:

```text
tetrium-colorimetry/
  pyproject.toml
  README.md
  src/tetrium_colorimetry/
    observers/
      cones.py
      observer.py
      genotypes.py
    spectra/
      base.py
      illuminants.py
      primaries.py
    values.py
    transforms/
      reflectance.py
      emission.py
      display.py
      contrast.py
      bases.py
    datasets/
    testing/
  tests/
    test_reflectance_emission.py
    test_display_raw_cone.py
    test_cone_contrast.py
    test_legacy_compat.py
```

Extraction should happen only after in-place tests pass and active Tetrium scripts have been migrated to canonical names.

### Extraction Boundaries

Move to new repo:

- Cone fundamentals, observer construction, and genotype utilities.
- Spectral value classes and wavelength alignment helpers.
- Reflectance, emission, raw cone, cone contrast, and display/projector transforms.
- Pure numpy/scipy colorimetry tests.

Keep in Tetrium repo:

- Vulkan/C++ app code.
- Measurement GUIs and experiment launchers.
- Paper visualization scripts unless converted into examples.
- Generated outputs, calibration measurements, and subject-specific experiment configs.
- Legacy scripts that are not part of the stable API.

## Acceptance Criteria

- A developer can tell from an API name whether a value is reflectance, emission, raw cone excitation, cone contrast, or display primary weights.
- Emissive/display workflows no longer need to use a misleading `cone_contrast` option for raw cone excitation.
- True cone contrast APIs require a background/reference response.
- Reflective workflows continue through an explicit illuminant path.
- `Observer` has a clear responsibility and does not hide stimulus-type assumptions in surprising ways.
- Normalized cone sensitivity access is explicit enough for 2D Contour Estimation.

## Implementation Phases

### Phase 0: Inventory And Freeze Current Behavior

Goal: understand all ambiguous call sites before renaming or changing behavior.

- Inventory every `cone_contrast` string, method, metadata key, file name, and plot label.
- Classify each occurrence as one of:
  - true cone contrast with background
  - raw cone excitation or raw cone delta
  - legacy label with unclear meaning
  - file/path compatibility that should not be renamed yet
- Inventory every `ColorSpaceType.CONE` call site in active scripts and package code.
- Classify each `ColorSpaceType.CONE` use as:
  - raw cone excitation
  - normalized display-conversion coordinate
  - generic internal basis
  - visualization-only coordinate
- Add numerical regression tests around current output before changing transform internals.

Exit gate:

- A checked-in inventory table exists in this doc or a linked migration note.
- Display validation generation and reflective/object-color workflows have baseline tests or reproducible scripts.

### Phase 1: Naming Cleanup Without Numerical Change

Goal: make raw cone excitation explicit while preserving old behavior.

- Add `raw_cone_excitation` as the canonical `QuestColorGenerator.color_picking_space` for the raw display null-direction path.
- Keep `cone_contrast` as a deprecated alias only for this path.
- Ensure `QuestColorGenerator` metadata emits `quest_raw_cone_delta` as canonical.
- Keep `quest_cone_contrast_delta` temporarily as a deprecated alias only when downstream consumers still need it.
- Add `ColorSpace.display_to_raw_cone_matrix()` as a clear alias for `get_raw_display_to_cone_matrix()`.
- Add helper methods for `display_to_raw_cone(...)` and `raw_cone_to_display_weights(...)` if they can delegate to existing matrix logic without numerical changes.
- Rename plot labels that are raw cone response/delta, not true contrast.

Exit gate:

- Active scripts no longer request `color_picking_space='cone_contrast'` for raw cone excitation.
- Old scripts still run with warnings.
- Generated metadata has canonical raw-cone keys.

### Phase 2: Explicit Reflectance, Emission, And Contrast APIs

Goal: make stimulus assumptions explicit at transform boundaries.

- Add `ReflectanceSpectrum` and `EmissionSpectrum` wrappers or constructors around current `Spectra`.
- Add `Observer.integrate_emission(...)`.
- Add `Observer.integrate_reflectance(..., illuminant=...)`.
- Add a true `ConeContrast` wrapper or constructor that requires background response and rejects zero/near-zero background channels.
- Document `Observer.observe(...)` as legacy ambiguous behavior, then route it internally through explicit helpers where feasible.
- Add explicit normalized cone sensitivity accessor for 2D Contour Estimation.

Exit gate:

- New code paths can perform display emission, object reflectance, raw cone, and cone contrast workflows without ambiguous API names.
- Tests cover missing illuminant, missing background, and zero-background contrast errors.

### Phase 3: Reduce `ColorSpaceType.CONE` Ambiguity

Goal: prevent generic conversion from hiding stimulus meaning.

- Audit `ColorSpace.convert(..., ColorSpaceType.CONE, ...)` and decide which calls should move to explicit raw-cone or display methods.
- Keep `ColorSpaceType.CONE` for internal basis routing only where removal would create large churn.
- Update docstrings so `ColorSpaceType.CONE` no longer claims to be a single scientific representation.
- Consider adding separate enum values only if wrappers and explicit methods are insufficient.

Exit gate:

- Public workflows no longer require users to know when `ColorSpaceType.CONE` means raw response versus internal coordinate.
- Remaining `ColorSpaceType.CONE` uses are documented as internal or visualization coordinates.

### Phase 4: Extraction Preparation

Goal: prepare a clean repository without destabilizing Tetrium experiments.

- Move stable tests into a package-agnostic layout.
- Write a migration guide with old and new names.
- Define extracted package name, module layout, and dependency policy.
- Decide which data assets are package data versus external calibration inputs.
- Create the new repository only after active Tetrium scripts run against the cleaned in-place package.

Exit gate:

- New package skeleton can run the core tests without Tetrium app dependencies.
- Tetrium repo can depend on the extracted package or vendored package without circular imports.

## Open Questions

- Why is the illuminant currently multiplied into reflectance before cone response normalization, and which invariant was it intended to preserve?
- Is `np.ones(dim)` intended to represent white/flat reflectance, normalized cone response, display midpoint/white, or a generic unit vector in each call site?
- Which existing `ColorSpaceType.CONE` call sites expect raw cone excitation and which expect normalized internal coordinates?
- Should extracted repository APIs use runtime dataclasses only, numpy type aliases only, or both?
- What should the canonical inverse path be for raw cone excitation to `DISP` when the display matrix is non-square or out of gamut?
- Should `Observer` retain an `illuminant` attribute for compatibility, or should illuminant move entirely to reflectance transforms after the migration window?
- What tolerance should define a zero background channel for cone contrast rejection?
- Should display primary weights carry a required `primary_order` field such as `RGBO`, `BGYR`, or `RGB/OCV`?
- Which existing generated JSON files are canonical data artifacts that need schema migration, and which can remain legacy?
- What minimum test data set should be copied into the extracted repository?

## Working Inventory

This inventory should be updated as call sites are audited.

### Known Raw Cone Excitation Sites

| Path | Current symbol/key | Current meaning | Target | Status |
| --- | --- | --- | --- | --- |
| `extern/TetriumColor/TetriumColor/TetraColorPicker.py` | `color_picking_space='cone_contrast'` | Raw cone-excitation null directions in display space | `raw_cone_excitation` canonical; `cone_contrast` deprecated alias | Done |
| `extern/TetriumColor/TetriumColor/TetraColorPicker.py` | `quest_raw_cone_delta` | Raw cone delta | Keep canonical | Done |
| `extern/TetriumColor/TetriumColor/TetraColorPicker.py` | `quest_cone_contrast_delta` | Duplicate of raw cone delta | Keep temporary deprecated alias | Done |
| `generate_genetic_test.py` | `gaussian_blob_mode='cone_contrast'` | Raw cone-excitation display workflow with constant display background | Accept legacy alias; prefer `raw_cone_excitation` | Done |
| `paper-viz/tetra_picker_530_533_blob.py` | `color_picking_space='cone_contrast'` | Raw cone-excitation picker direction | Use `raw_cone_excitation` | Done |
| `paper-viz/simple_observer_projection.py` | `_raw_cone_excitation_null_direction_in_disp` | Explicit raw cone-excitation null direction | Keep, eventually call shared helper | Not started |
| `paper-viz/hyperobserver_diff_viz.py` | `get_raw_display_to_cone_matrix()` with stored `raw_cone` | Raw cone-to-display reconstruction | Prefer `display_to_raw_cone_matrix()` alias where appropriate | Not started |
| `extern/TetriumColor/scripts/validation/convert_bgyr_to_bgor.py` | `get_raw_display_to_cone_matrix()` with stored `raw_cone` | Raw cone-to-display reconstruction | Prefer `display_to_raw_cone_matrix()` alias where appropriate | Not started |
| `extern/TetriumColor/scripts/validation/generate_tetra_picker_validation_metamers.py` | metadata `color_picking_space='cone_contrast'` | Raw cone-excitation validation stimuli | Use `raw_cone_excitation`; keep deprecated metadata notes | Done |
| `extern/TetriumColor/scripts/validation/validate_display_measurements.py` | `use_cone_contrast_plot` and labels | Mixed raw cone vs contrast plot labeling | Rename raw-cone paths and reserve contrast labels for true contrast | Not started |
| `src/apps/AppGeneticTestViewer.cpp` | `gaussian_blob_cone_contrast`, CLI arg `cone_contrast` | Legacy app-level raw-cone mode name | Update after Python CLI accepts both names | Not started |
| `src/apps/AppPseudoIsochromaticTest.cpp` | `cone_contrast` command/config strings | Legacy app-level raw-cone mode name | Update after Python CLI accepts both names | Not started |

### Known True Cone Contrast Sites

| Path | Symbol | Meaning | Target | Status |
| --- | --- | --- | --- | --- |
| `extern/TetriumColor/TetriumColor/ColorSpace.py` | `cone_contrast_delta(...)` | True contrast if called with background and interpreted as `(a - b) / background` | Keep, strengthen docs/tests | Not started |
| `extern/TetriumColor/TetriumColor/ColorSpace.py` | `get_display_to_cone_contrast_matrix(...)` | True contrast matrix `Q / (Q S0)` | Keep, test zero-background rejection | Not started |
| `extern/TetriumColor/TetriumColor/ColorSpace.py` | `get_cone_contrast_null_direction_in_disp(...)` | Contrast-normalized null direction | Keep only for true contrast workflows | Not started |
| `extern/TetriumColor/TetriumColor/ColorSpace.py` | `get_cone_contrast_axis_in(...)` | Raw cone delta implied from unit contrast axis and background | Keep, clarify docs | Not started |
| `extern/TetriumColor/TetriumColor/ColorSampler.py` | `get_cone_contrast_metamers_brainard(...)` | Brainard-style cone contrast modulation | Keep, add tests/examples | Not started |
| `extern/TetriumColor/TetriumColor/ColorSampler.py` | `get_cone_contrast_plate(...)` | Plate generation from Brainard contrast method | Keep, clarify metadata | Not started |
| `extern/TetriumColor/scripts/validation/verify_generated_blob_output.py` | `cone_contrast_delta(...)` | True contrast verification of generated blob against background | Keep as contrast-specific verification | Not started |

### Ambiguous Sites Requiring Review

| Path/pattern | Ambiguity | Review action | Status |
| --- | --- | --- | --- |
| `ColorSpace.convert(..., ColorSpaceType.CONE, ...)` | `CONE` may mean raw cone response, normalized display-conversion coordinate, or internal basis | Classify by source/target pair and caller intent | Not started |
| `Observer.observe(...)` | Input may be reflectance, emission SPD, or generic spectral vector depending on observer illuminant state | Classify by caller and add explicit helper target | Not started |
| `np.ones(dim)` | May mean white point, display midpoint, flat reflectance, or unit cone vector | Classify by caller before replacing | Not started |
| validation JSON metadata | Some `cone_contrast` keys store raw deltas | Keep deprecated keys but add canonical raw-cone keys and schema note | In progress |
| validation scripts plot labels | `cone contrast` label sometimes means raw cone response | Rename by quantity actually plotted | Not started |
| generated output files under `paper-viz/output` | Legacy artifacts contain old metadata | Do not rewrite unless regenerating outputs | Deferred |

## Migration Todos

- [x] Create this living refactor spec.
- [ ] Inventory every `cone_contrast` call site and classify it as true contrast or raw cone excitation.
- [ ] Inventory every `observer.observe(...)` call site and classify input as reflectance, emission SPD, or generic spectral vector.
- [x] Add canonical `raw_cone_excitation` generator mode.
- [x] Keep `cone_contrast` as a deprecated alias only where it preserves current behavior.
- [ ] Update active display-generation scripts to request `raw_cone_excitation`.
- [ ] Update metadata to prefer `quest_raw_cone_delta` and `quest_color_picking_space='raw_cone_excitation'`.
- [ ] Keep deprecated metadata keys during the migration window.
- [ ] Rename plot labels that say cone contrast when they are showing raw cone response.
- [x] Add explicit display-to-raw-cone API names.
- [ ] Add explicit true cone contrast API docs requiring a background.
- [ ] Add reflectance/emission helper APIs or value wrappers.
- [ ] Add explicit normalized cone sensitivity API for contour estimation.
- [ ] Write a migration guide from old names to new names.
- [ ] Decide the extracted package name and public module layout.
- [ ] Remove deprecated aliases after downstream scripts are migrated.

### Detailed Todo Backlog

#### Inventory

- [ ] Create a `cone_contrast` inventory table with path, symbol/key, current meaning, target name, and migration action.
- [ ] Create a `ColorSpaceType.CONE` inventory table with path, source space, target space, and intended coordinate semantics.
- [ ] Create an `Observer.observe` inventory table with path, input value type, illuminant behavior, and target helper.
- [ ] Mark generated data files that should keep legacy keys for reproducibility.

#### API Additions

- [x] Add `display_to_raw_cone_matrix()` alias in `ColorSpace`.
- [ ] Add `display_to_raw_cone(display_weights)` helper.
- [ ] Add `raw_cone_to_display_weights(raw_cone, method='pinv')` helper.
- [ ] Add `Observer.integrate_emission(...)`.
- [ ] Add `Observer.integrate_reflectance(..., illuminant=...)`.
- [ ] Add `Observer.normalized_cone_sensitivities(...)`.
- [ ] Add value wrappers for `EmissionSpectrum`, `ReflectanceSpectrum`, `RawConeExcitation`, and `ConeContrast`.

#### Migration

- [x] Update `QuestColorGenerator` default to `raw_cone_excitation`.
- [x] Accept legacy `cone_contrast` with `DeprecationWarning`.
- [x] Update Tetra picker validation generator metadata to use `raw_cone_excitation`.
- [x] Update Gaussian blob verification to request `raw_cone_excitation` unless testing true contrast.
- [x] Update paper visualization scripts to canonical names.
- [ ] Update C++ app command arguments only after Python scripts accept both names.
- [ ] Keep CLI compatibility for old generated workflows until existing configs are regenerated.

#### Documentation

- [ ] Add a short "Reflectance vs Emission" page to the extracted package docs.
- [ ] Add examples for display primary weights to raw cone excitation.
- [ ] Add examples for true cone contrast with a background.
- [ ] Add examples for normalized cone sensitivities for contour estimation.
- [ ] Add a migration guide table from old names to new names.

## Test Todos

- [ ] Unit test: `EmissionSpectrum -> RawConeExcitation` equals `observer.sensor_matrix @ emission_spd`.
- [ ] Unit test: `ReflectanceSpectrum -> RawConeExcitation` refuses missing illuminant.
- [ ] Unit test: `ConeContrast` cannot be constructed without a background response.
- [ ] Regression test: `display_to_raw_cone_matrix()` matches current `get_raw_display_to_cone_matrix()`.
- [ ] Regression test: current validation metamer generation keeps equivalent `DISP` endpoints.
- [ ] Regression test: old `cone_contrast` generator mode warns and maps to `raw_cone_excitation`.
- [ ] Regression test: true cone contrast uses `(sample - background) / background` and rejects zero background response.
- [ ] Integration test: reflective/object-color workflows still work through explicit illuminant paths.
- [ ] Example test: 2D Contour Estimation can use normalized cone sensitivities without touching reflectance or display transforms.

## Projector Observer-Grid Test Suite

Add a runnable test suite modeled on `extern/TetriumColor/scripts/simulation/generate_template_observer_grid.py`.
The goal is not generic projector diagnostics. The suite should generate observer/template metamer grids that can be opened immediately in `AppImageViewer`.

### Output Contract

- Output directory: `assets/apps/AppImageViewer/tetra_images/`
- Every test stimulus must write exactly paired files:
  - `<test_name>_RGB.png`
  - `<test_name>_OCV.png`
- File names must be stable and descriptive so `AppImageViewer` can present them immediately after refresh.
- Existing hand-curated assets in this directory must not be overwritten unless the test name is part of the suite namespace.
- Use a namespace prefix such as `observer_grid_` for all generated test files.
- The `_RGB.png` file should contain the RGO view and the `_OCV.png` file should contain the BGO view, matching the app's paired-image convention.

### Runner Contract

- Add a script or test command that can be run from repo root, for example:
  - `python scripts/validation/generate_projector_observer_grid.py`
- [x] The runner should create the output directory if missing.
- [x] The runner should print the generated base names and absolute paths.
- [x] The runner should support a `--clean-suite-outputs` flag that removes only files matching `observer_grid_*_{RGB,OCV}.png`.
- [x] The default run should be deterministic.
- [x] The default run should not require a connected projector; the projector is the visual inspection target after files are written.
- [x] The runner should expose the same core knobs as the template grid script: `--primaries_dir`, `--num_observers`, `--sex`, `--seed`, `--proportion`, `--od`, `--macular`, `--lens`, `--cell_size`, `--cell_gap`, and `--blob_sigma_frac`.
- [x] Default `--output_dir` should be `assets/apps/AppImageViewer/tetra_images/`.

### Required Observer-Grid Outputs

- [x] `observer_grid_template_metamer_RGB.png` and `observer_grid_template_metamer_OCV.png`: full grid equivalent to the template observer RGO/BGO metamer grid.
- [x] `observer_grid_<template>_RGB.png` and `observer_grid_<template>_OCV.png`: one paired grid per cone nomogram template, useful when comparing a single template row at projector scale.
- [x] `observer_grid_top_observers_RGB.png` and `observer_grid_top_observers_OCV.png`: compact grid across the top N observer genotypes for the default template.
- [ ] Optional per-cell pairs with names like `observer_grid_cell_<template>_obsXX_RGB.png` and `observer_grid_cell_<template>_obsXX_OCV.png` if single-cell inspection is useful.
- [x] A metadata CSV or JSON next to the generated images recording template, genotype, BGOR/RGBO codes, proportion, primaries path, and raw cone/contrast generation mode.

### Numeric Assertions

The suite should generate images and also assert the underlying numeric invariants before writing:

- [x] Every generated RGB/OCV image has the same dimensions.
- [x] Pixel values are finite and clipped to `[0, 255]` only at the final image-writing boundary.
- [x] RGO/BGO splitting follows the existing template-grid convention: BGOR code `[B, G, O, R]` maps to RGB pixel `[R, G, O]` and OCV pixel `[B, G, O]`.
- [x] Generated endpoints stay within `[0, 1]` before quantization.
- [x] Quantized BGOR/RGBO codes in metadata match the written pixels.
- [x] The grid generation path records whether it used true cone-contrast null directions or raw-cone-excitation null directions.
- [x] If true cone contrast is used, the background must be explicit and nonzero.
- [ ] If raw cone excitation is used, the path should use `display_to_raw_cone_matrix()` or equivalent raw display-to-cone math.

### Visual Acceptance

On the projector through `AppImageViewer`:

- [ ] All generated `observer_grid_*` images appear in the image picker.
- [ ] `_RGB.png` and `_OCV.png` pairs load together under one base name.
- [ ] Template rows and observer columns are visually inspectable at projector scale.
- [ ] The paired blobs/cells match the template observer grid layout.
- [ ] The output can be regenerated after changing primaries or template parameters and immediately viewed without moving files.

## Compatibility Policy

- First migration pass: add new names, keep old names, warn on misleading public aliases.
- Active scripts: update to canonical names immediately after aliases exist.
- Generated JSON: emit canonical keys and keep deprecated keys until consumers are migrated.
- CLI arguments: accept both canonical and legacy strings for at least one regeneration cycle.
- Removal: delete deprecated aliases only after the inventory confirms no active script, app command, config, or validation parser requires them.

## Risks

- A raw cone excitation vector and the current internal `ColorSpaceType.CONE` vector may not always be numerically identical. Tests must lock the intended matrix path before renaming.
- Some reflective/object-color code may rely on `Observer` carrying an illuminant. Moving illuminant policy too early could break object color solid calculations.
- Generated validation outputs may be consumed by C++ app code or analysis notebooks that expect legacy metadata keys.
- A non-square display matrix makes raw-cone-to-display inversion policy scientific and operational, not just naming cleanup.
- A wrapper-heavy design could make existing numpy workflows cumbersome. Keep wrappers thin and explicit.

## Definition Of Done For First Pass

- `raw_cone_excitation` is the canonical generator mode for display raw-cone null directions.
- Deprecated `cone_contrast` aliases warn but preserve behavior.
- Documentation and metadata distinguish raw cone excitation from true cone contrast.
- There is at least one regression test for display-to-raw-cone matrix behavior.
- There is at least one regression test for true cone contrast requiring a background.
- This document's inventory sections are updated with audited call sites.
- The next engineer can choose a todo from this file without needing to infer the intended scientific meaning from legacy names.

## Implementation Notes

- Treat `ColorSpaceType.CONE` as a legacy internal coordinate until a clearer split is complete.
- Use deprecation warnings rather than immediate breaks for first-pass public entry points.
- Prefer behavior-level migrations over large file moves until regression tests exist.
- Keep numerical output unchanged unless a test or scientific review proves an existing equation is wrong.
