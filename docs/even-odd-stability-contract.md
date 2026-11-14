# Even-Odd Stability Contract

This report documents the contract applications must follow to keep Tetrium's
even-odd presentation stable, especially for fixed-background stimuli such as
`AppPseudoIsochromaticTest` steady adaptation mode.

## Executive Summary

Tetrium's even-odd path assumes that every application frame submitted after a
target vblank sample will be ready for that target vblank. If an application does
expensive CPU work, synchronous GPU uploads, descriptor allocation, blocking
queue operations, or resource destruction inside `TickImGui()`/`TickVulkan()`,
the frame can miss the target vblank. Missing exactly one vblank is enough to
swap the physical delivery slot: content selected as RGO is shown during the BGO
slot, or vice versa.

The application contract should be:

1. The `ColorSpace` passed by the engine is the only source of truth for whether
   the current draw is RGO/RGB or BGO/OCV.
2. Once a frame enters `TickImGui()` or `TickVulkan()`, the app may only issue
   bounded, already-prepared draw work.
3. Resource creation, image decode, texture upload, descriptor creation, Python
   generation, file I/O, sound or logging stalls, and resource destruction must
   not happen on a parity-critical presentation frame.
4. Visual state changes must be pair-atomic: a new stimulus, background, noise
   frame, ramp phase, or response state must become visible only when both RGO
   and BGO resources for that state are resident and the app can draw the same
   logical state for a complete even/odd pair.
5. If a heavy update is unavoidable, it must happen while presenting a parity
   neutral holding field and the app must intentionally resynchronize before
   showing color-critical content.

## How Even-Odd Works

Tetrium delivers a four-primary image by presenting two normal frames in
alternation:

- even physical display slot: normally RGO/RGB
- odd physical display slot: normally BGO/OCV

The user-facing calibration can flip this mapping with `_flipEvenOdd`, but the
important invariant is that the physical display's parity must stay aligned with
the engine's `ColorSpace`.

In hardware sync mode, the engine reads the swapchain vblank counter and derives
parity from `counter % 2`. The relevant path is:

- `drawFrame()` acquires a swapchain image first because `vkAcquireNextImageKHR`
  can block until a vblank frees an image.
- After acquire, `waitAndAlignVBlank()` reads the hardware vblank counter and
  sets `expectedNextCounter = current + 1`.
- `getCurrentColorSpace()` returns RGB for even and OCV for odd, with optional
  `_flipEvenOdd` inversion.
- The app draws ImGui/Vulkan content for that `ColorSpace`.
- FIFO present queues the rendered image for the next blanking interval.

Relevant code:

- `src/Tetrium_Tick.cpp:181` explains why parity is chosen after acquire.
- `src/Tetrium_EvenOdd.cpp:241` samples the vblank counter and records the next
  expected presentation counter.
- `src/Tetrium_EvenOdd.cpp:207` maps parity to `ColorSpace`.
- `src/Tetrium_Tick.cpp:353` relies on FIFO present to display the submitted
  image on the next vblank.

This means the sampled counter is a prediction with a deadline. The selected
`ColorSpace` is correct only if the app and engine finish recording/submitting in
time for that next vblank.

## How It Breaks

Even-odd breaks when a frame takes long enough that the frame selected for vblank
`N + 1` is not actually presented until `N + 2` or later. If the miss is odd, the
physical slot has opposite parity from the `ColorSpace` used to draw the frame.
That is the RGO/BGO delivery-slot switch.

The engine already detects this condition:

- `src/Tetrium_EvenOdd.cpp:250` compares the current vblank counter against the
  previous `expectedNextCounter`.
- `src/Tetrium_EvenOdd.cpp:253` logs `"Vblank pacing: missed ... target vblanks"`.

Concrete break cases:

1. Blocking application work after target-vblank sampling.
   The target is sampled before `drawImGui()` (`src/Tetrium_Tick.cpp:184` then
   `src/Tetrium_Tick.cpp:193`). Anything slow inside app drawing consumes the
   one-vblank budget.

2. Synchronous texture loading during `TickImGui()`.
   `AppPseudoIsochromaticTest::TickImGui()` calls `populatePromptContext()` when
   `_needsTextureLoad` is set (`src/apps/AppPseudoIsochromaticTest.cpp:126`).
   That function unloads old textures, loads the RGB and OCV images, and creates
   ImGui descriptors (`src/apps/AppPseudoIsochromaticTest.cpp:1773` through
   `src/apps/AppPseudoIsochromaticTest.cpp:1789`).

3. Texture upload implementation is blocking.
   `TextureManager::LoadTexture()` decodes the image, builds staging resources,
   creates the GPU image, does layout transitions, copies pixels, and creates a
   sampler (`src/components/TextureManager.cpp:294` through
   `src/components/TextureManager.cpp:351`). Each layout transition/copy uses
   `QuickCommandBuffer`, whose destructor submits to the graphics queue and calls
   `vkQueueWaitIdle()` (`src/lib/VulkanUtils.cpp:276` through
   `src/lib/VulkanUtils.cpp:286`). A two-texture trial load therefore performs
   multiple queue-idle waits during the already-targeted frame.

4. Resource destruction during a presentation frame.
   `populatePromptContext()` unloads prior RGB/OCV textures immediately before
   loading the next trial (`src/apps/AppPseudoIsochromaticTest.cpp:1773`). Even
   if destruction is usually fast, it is still unbounded driver work in the
   parity-critical path.

5. Pair-incoherent visual state.
   Any state or animation driven by raw ImGui frame count, wall-clock time, or a
   one-frame state transition can change between the RGO and BGO halves. Steady
   adaptation currently selects temporal noise with `ImGui::GetFrameCount() / 2`
   (`src/apps/AppPseudoIsochromaticTest.cpp:634`). That is an attempt to hold one
   noise texture for two engine frames, but it is not explicitly tied to a
   stable even/odd pair. If a frame is missed, the noise phase can become another
   visible cue that parity has slipped.

## Why Steady Adaptation Exposes the Problem

Steady adaptation mode tries to keep a fixed display-space background on screen
and overlay trial content:

- `drawAdaptationFrame()` draws the fixed background every frame
  (`src/apps/AppPseudoIsochromaticTest.cpp:603`).
- During identification, it draws the trial texture selected by
  `ctx.colorSpace` (`src/apps/AppPseudoIsochromaticTest.cpp:852`).
- During blank/fixation/answer, it draws noise and fixation over the adaptation
  field (`src/apps/AppPseudoIsochromaticTest.cpp:820` and
  `src/apps/AppPseudoIsochromaticTest.cpp:852`).

The fixed-background case makes parity errors more obvious because the intended
RGO/BGO pair should be perceptually stable around the same background. When a
trial starts or changes, the current code may synchronously load both trial
textures at the top of `TickImGui()`. The app is trying to avoid mid-frame GPU
sync by deferring work to the next frame, but the next frame is still after the
engine has chosen a target vblank and `ColorSpace`. That deferral avoids some
resource lifetime hazards, but it does not satisfy the even-odd timing contract.

In short: steady adaptation's fixed background is stable in content, but the
trial-switch frame is not stable in time.

## Application Contract

### 1. Treat `ColorSpace` as a sampled presentation slot

Apps must not infer parity from `ImGui::GetFrameCount()`, `currentFrameInFlight`,
wall-clock time, frame index, trial index, or their own counters. Use
`ctx.colorSpace` only.

Allowed:

- `texture = stimulus.texture[ctx.colorSpace]`
- recording identical geometry with color-space-specific resources

Not allowed:

- `frameCount % 2` to choose RGO/BGO
- assuming `NUM_FRAME_IN_FLIGHT` or swapchain image index corresponds to
  display parity
- updating stimulus state once per app frame without considering even/odd pairs

### 2. Keep parity-critical frames bounded

After the engine samples the next vblank, app code must fit comfortably under
one refresh interval, including engine overhead. At 120 Hz that is 8.33 ms total;
at 60 Hz that is 16.67 ms total. The application budget should be lower than the
refresh interval because acquire, ImGui setup, command recording, submission,
driver overhead, and FIFO present all consume time.

Practical rule:

- normal app draw: no blocking calls and no new GPU resources
- target: stable p99 app draw time below 25-35% of the refresh period
- hard fail: any draw frame that can call `vkQueueWaitIdle`,
  `vkDeviceWaitIdle`, file I/O, image decode, Python, network, process spawn,
  large allocation, descriptor pool churn, or synchronous logging

### 3. Prepare resources before visibility

A color-critical stimulus is drawable only after both halves are fully resident:

- RGO/RGB texture or buffer exists
- BGO/OCV texture or buffer exists
- ImGui/Vulkan descriptors exist
- old resources remain alive until no in-flight frame can reference them

The app may change a trial pointer during drawing only by swapping to a
prevalidated, already-resident pair.

### 4. Make logical updates pair-atomic

The app should expose each logical visual state for a complete even/odd pair.
Examples:

- background value
- stimulus identity
- stimulus transform and bounds
- temporal noise sample
- ramp brightness
- fixation marker
- answer/blank state

If a state changes between the RGO and BGO halves, the two physical half-frames
no longer represent one coherent tetrachromatic image. This can be acceptable for
non-color UI, but it is not acceptable for color-critical fixed-background
stimuli.

### 5. Use a neutral holding state for expensive transitions

When a new trial requires expensive work, continue presenting a neutral field
that is invariant across RGO/BGO until preparation is complete. For steady
adaptation, that should be the fixed adaptation background plus optional
pair-stable fixation/noise, not a partially loaded or just-swapped stimulus.

The app should then reveal the new trial at a known pair boundary.

## Characterizing Drawing Time Better

The current profiler records broad CPU scopes, but even-odd needs deadline
accounting tied to vblank parity. Useful metrics:

1. `targetCounter`
   The value assigned by `waitAndAlignVBlank()`.

2. `actualCounterAtNextFrame`
   The next frame's hardware counter, compared against the previous target.

3. `missedVblanks`
   Already computed in `waitAndAlignVBlank()`. Make this visible per app state,
   not just as a log line.

4. CPU phase timings after target sampling:
   - app `TickImGui()`
   - app `TickVulkan()`
   - texture/resource API calls
   - command recording
   - queue submit
   - present

5. Hazard counters:
   - number of texture loads during `TickImGui()`
   - number of texture unloads during `TickImGui()`
   - number of `QuickCommandBuffer` queue-idle waits during a frame
   - number of descriptor creations during a frame

6. Pair coherence:
   Track a monotonically increasing `visualEpoch`. It should change only at an
   approved pair boundary, and both RGB and OCV draws for that epoch should use
   the same logical stimulus state.

The most direct implementation is an engine-side even-odd guard:

- Set `parityCriticalFrame = true` after `waitAndAlignVBlank()`.
- Resource APIs assert or warn if called while the guard is active.
- Record app state, `ColorSpace`, target counter, and call site for violations.

## Proposed Fix Direction for Fixed Background

For `AppPseudoIsochromaticTest` steady adaptation:

1. Split trial generation/preparation from trial presentation.
   Generate and load the next trial's RGB/OCV textures before the app attempts to
   present it.

2. Replace `_needsTextureLoad` in `TickImGui()` with a ready-handle swap.
   `TickImGui()` should only assign preloaded handles to the active prompt; it
   should not call `LoadTexture()`, `UnloadTexture()`, or `InitImGuiTexture()`.

3. Keep old trial resources alive for at least all in-flight frames, preferably
   by retiring them through a small deferred-destruction queue.

4. Hold the steady adaptation background while waiting for preparation.
   If the next trial is not ready, keep drawing adaptation background/fixation
   only.

5. Add a pair-boundary reveal.
   Do not reveal a new trial on an arbitrary single frame. Reveal it only when
   both the next RGB and next OCV pass will use the same `visualEpoch`.

6. Make temporal noise pair-stable.
   Drive noise from a pair epoch controlled by the engine or app state machine,
   not raw `ImGui::GetFrameCount()`.

## Checklist for New or Modified Apps

Before an app is considered even-odd safe:

- It never calls texture load/unload or descriptor creation from
  `TickImGui()`/`TickVulkan()` while color-critical content is visible.
- It never performs blocking GPU waits from app draw code.
- It never does image decode, file I/O, Python calls, network calls, process
  launches, or large allocations from app draw code.
- It uses `ctx.colorSpace` for channel selection.
- It keeps both color-space halves of a visual stimulus resident before reveal.
- It changes color-critical visual state only on pair boundaries.
- It keeps previous resources alive until in-flight frames are finished.
- It reports p95/p99 app draw time and missed-vblank count during representative
  trials.
- A stress run with trial changes shows zero missed vblanks while color-critical
  content is visible.

## Bottom Line

Even-odd stability is not only a rendering correctness issue; it is a real-time
deadline contract. The engine can choose the correct RGO/BGO slot only if the app
does not change the timing class of the frame after that choice is made. Steady
adaptation currently violates the contract because it performs trial texture
resource work inside `TickImGui()`, after target-vblank selection. The fixed
background should remain a cheap, already-resident holding state, and new trial
stimuli should become visible only after both halves are prepared and can be
shown as a coherent even/odd pair.
