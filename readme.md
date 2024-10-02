```
___  ___ ___  __
 |  |__   |  |__) | |  |  |\/|
 |  |___  |  |  \ | \__/  |  |

```

Vulkan-based tetrachromacy display engine.
## Engine Architecture

### Frontend

Frontend is still under construction! Currently all renders are hard-coded in backend.

### Backend

Engine backend consists of core logic on vulkan rendering, presentation, and even-odd
frame synchronization. 

#### "Deferred" Presentation

The engine holds two off-screen frame buffers, each responsible for one color space(RGB/OCV)

Each tick, the engine simultaneously renders to the two buffers with RGB and OCV logic.

Once the rendering is done, depending on the presentation mode:

- `TetraMode::kDualProjector`: both buffers are committed to either RGB or OCV projector.
- `TetraMode::kEvenOddSoftwareSync` or `TetraMode::kEvenOddHardwareSync`: the engine determines the
  count of the vertical blanking period, either through hardware APIs or a virtual software frame
  counter; the engine then commits one frame buffer while discarding the other.

#### Even-Odd Frame Synchronization

To determine even/odd-ness of the frame to present, the engine relies on the total number of
vertical blanking periods % 2. This information is obtained using [vkGetSwapchainCounterEXT](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkGetSwapchainCounterEXT.html), when the engine runs under `kEvenOddSoftwareSync`. 
For `kEvenOddSoftwareSync`, [vkGetPastPresentationTimingGOOGLE](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkGetPastPresentationTimingGOOGLE.html) is used to provide a rought estimation.

### ImGui

Given most of tetrachromacy test are performed under 2D space, the engine provides an interface 
with [DearImGui](https://github.com/ocornut/imgui) that allows for easy integration and development
of 2D RGV&OCV apps.

In engine backend `Tetrium::drawImGui(ColorSpace)` function calls into various ImGui render widgets.
Internally, `drawImGui` operates on both RGB and OCV color space, one can differentiate them using
the `colorSpace` argument and develop various widgets. As an example:

```cpp
enum ColorSpace {
    RGB,
    OCV
};
drawImGui(ColorSpace colorSpace) {
    switch colorSpace {
        case RGB:
            ImGui::Image(ishiHaraImgRGB);
            break;
        case OCV:
            ImGui::Image(ishiHaraImgOCV);
            break;
    }
}
```


## Requirements

### All Systems

- `CMake`
- [Vulkan SDK](https://vulkan.lunarg.com/)

### Hardware Even-odd frame sync

- NVIDIA GPU supporting `VK_EXT_display_surface_counter`
- refer to [#Linux] for additional instructions

## Build

### Apple

set `VULKAN_LIB_PATH` in `CMakeLists.txt` to your own path after installing Vulkan SDK

#### FreeType

`brew install freetype`

validate that `/opt/homebrew/include/freetype2` and `/opt/homebrew/lib/libfreetype.dylib`
contains freetype library and include files.

### Linux

install `X11` and `Xrandr` for direct display access, for hardware even-odd frame sync

install freetype

`sudo apt-get install libfreetype6-dev`


### Windows

tbd...

## Run

Config loading is not supported yet. To configure run option, modify `Tetrium::InitOptions options`
field to set display mode.

## References

[Theory of Human Tetrachromatic Color Experience and Printing](https://dl.acm.org/doi/10.1145/3658232)

[TetraPolyscope](https://github.com/i-geng/polyscope)
