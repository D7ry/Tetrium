# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

Tetrium is a display engine targeting color experiences beyond regular human perception, specifically designed for tetrachromatic color rendering. The project implements the theory from "Theory of Human Tetrachromatic Color Experience and Printing" (ACM 2024).

## Build & Development

### Environment Setup
```bash
# Create and activate conda environment
conda create --name tetrium python=3.11.9
conda activate tetrium
```

### Building

**UNIX (macOS/Linux):**
```bash
mkdir build && cd build
cmake ../ && make
```

**Windows:**
```bash
cmake --preset vs2022-windows
```
Note: `vcpkg` is required for Windows builds.

### Shaders
Shaders must be compiled to SPIR-V before running:
```bash
python compile_shaders.py
```
This compiles all `.vert` and `.frag` files in `shaders/` and `assets/apps/` directories using `glslc`.

### Running
```bash
./build/Tetrium
```

Configuration is currently code-based. Modify `Tetrium::InitOptions options` in `src/main.cpp` to change display mode.

## Architecture

### Core Rendering Pipeline

Tetrium uses a dual-colorspace rendering approach:
- **RGB**: Standard RGB color space
- **OCV**: Opponent Color Vision space for tetrachromatic rendering
- **RYGB**: Red-Yellow-Green-Blue intermediate framebuffer format
- **ROCV**: RGB-or-OCV even-odd frame presentation mode

The engine renders to both color spaces and can present them using three modes:
1. **kEvenOddHardwareSync**: Hardware-synced even-odd frames (Linux only, requires `VK_EXT_display_surface_counter`)
2. **kEvenOddSoftwareSync**: Software-timed even-odd frames (all platforms)
3. **kDualProjector**: Dual projector superposition (not implemented)

### Application System

Apps are self-contained modules that inherit from `TetriumApp::App` (see `src/apps/App.h`):

```cpp
class App {
    virtual void Init(InitContext& ctx);
    virtual void Cleanup(CleanupContext& ctx);
    virtual void OnOpen();  // Called when app becomes primary
    virtual void OnClose(); // Called when app is deactivated

    // Tick functions (only called when app is active)
    virtual void TickImGui(const TickContextImGui& ctx);
    virtual void TickVulkan(TickContextVulkan& ctx);
    virtual void TickOffScreen(const TickContextOffScreen& ctx);
};
```

Apps are registered in `src/main.cpp` and can render to both RGB and OCV color spaces. The engine manages app lifecycle and provides rendering contexts.

**Available Apps:**
- `AppPseudoIsochromaticTest`: Tetrachromatic color blindness testing
- `AppScrambledFaceTest`: Perception testing with scrambled faces
- `AppTemporalAFC`: Temporal 3-alternative forced choice tests
- `AppAnomaloscope`: Color matching tests
- `AppTetraHueSphere`: Tetrachromatic hue sphere visualization
- `AppImageViewer`: View tetrachromatic images
- `AppPainter`: Paint in tetrachromatic color space
- `AppAutoMeasure`: Automated measurement tools
- `AppGeneticTestViewer`: Genetic test visualization

### Key Components

**Engine Core** (`src/Tetrium.h`):
- Main engine class managing Vulkan, GLFW windowing, swapchains, and render passes
- Handles even-odd frame synchronization for tetrachromatic rendering
- Manages multiple apps and switches between them

**VQ Library** (`src/lib/VQ*.h`):
- Vulkan utility classes: `VQDevice`, `VQBuffer`, `VQDeviceImage`, `VQPipelineBuilder`
- Abstraction layer over Vulkan API

**Components** (`src/components/`):
- `TextureManager`: Load and manage textures
- `ShaderUtils`: Shader compilation and management
- `Camera`: 3D camera with view/projection matrices
- `InputManager`: Keyboard/mouse input handling
- `SoundManager`: Audio playback
- `TaskQueue`: Async task execution
- `DeltaTimer`: Frame timing
- `Profiler`: Performance profiling
- `RYGBTextureRenderer`: Renders RYGB textures to screen

**ImGui Widgets** (`src/components/imgui_widgets/`):
- `ImGuiWidgetPerfPlot`: Performance graphs
- `ImGuiWidgetDeviceInfo`: Device information display
- `ImGuiWidgetEvenOddCalibration`: Calibrate even-odd frame timing
- `ImGuiWidgetColorTile`: Color tile picker
- `ImGuiWidgetBlobHunter`: Blob detection/tracking

**Shared Structures** (`src/structs/`):
- `ColorSpace`: RGB/OCV enum
- `SharedEngineStructs`: Common engine context structures
- `Vertex`: Vertex data structure for meshes
- `ImGuiTexture`: ImGui texture descriptor wrapper

### TetriumColor Python Library

`extern/TetriumColor` is a Python library that provides color computation utilities for tetrachromatic rendering. Install it with:
```bash
pip install -e /path/to/TetriumColor
```

### Platform-Specific Notes

**Hardware Even-Odd Sync (Linux only):**
- Requires Vulkan extensions: `VK_EXT_display_surface_counter`, `VK_KHR_display`, `VK_EXT_direct_mode_display`
- Only works on X11 (not Wayland)
- Tested on NVIDIA RTX 30/40 series discrete GPUs on Ubuntu 22.04 and Fedora 41

**macOS:**
- Uses MoltenVK for Vulkan support
- Limited to software even-odd sync

**Windows:**
- Supports hardware even-odd sync via DXGI integration
- Uses DirectX interop for swapchain management

### Dependencies

Runtime/link-time dependencies:
- Vulkan SDK
- Python 3.11
- libtiff
- freetype
- libdrm (Linux)
- openal-soft
- libsndfile

Vendored in `extern/`:
- glfw, glm, imgui, implot, spdlog, stb, tinyobjloader

## Common Workflows

### Adding a New App

1. Create `src/apps/AppMyApp.h` and `src/apps/AppMyApp.cpp`
2. Inherit from `TetriumApp::App`
3. Implement `Init()`, `Cleanup()`, and desired `Tick*()` methods
4. Add source file to `SOURCES_BUILD` in `CMakeLists.txt`
5. Register app in `src/main.cpp`:
   ```cpp
   apps.push_back({new TetriumApp::AppMyApp(), "My App"});
   ```

### Working with Shaders

Shaders live in `shaders/` or `assets/apps/`. After modifying `.vert` or `.frag` files:
```bash
python compile_shaders.py
```

### Display Calibration

Use the `ImGuiWidgetEvenOddCalibration` widget to calibrate even-odd frame timing for tetrachromatic displays. Access via the main menu when running the engine.

### Test Data Logging

Use `TestDataLogger` (`src/lib/TestDataLogger.h`) to log experimental data from psychophysical tests.

## File Organization

```
src/
├── apps/              # Self-contained applications
├── components/        # Engine components (camera, input, sound, etc.)
├── lib/               # Vulkan utilities and core libraries
├── structs/           # Shared data structures
├── Tetrium*.cpp       # Main engine implementation (bootstrap, tick, windowing, etc.)
└── main.cpp           # Entry point

shaders/               # GLSL shaders (compile to SPIR-V)
assets/                # Textures, models, app-specific shaders
extern/                # Third-party dependencies
```
