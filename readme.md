```
___  ___ ___  __
 |  |__   |  |__) | |  |  |\/|
 |  |___  |  |  \ | \__/  |  |

```

Vulkan-based tetrachromacy display engine.
## Engine Architecture

### Dual-Frame Buffer Rendering

The engine holds two off-screen frame buffers, each responsible for one color space.

Each tick, the engine simultaneously renders to the two buffers.

Once the rendering is done, depending on the presentation mode:

- `TetraMode::kDualProjector`: both buffers are committed to either RGB or OCV projector.
- `TetraMode::kEvenOddSoftwareSync` or `TetraMode::kEvenOddHardwareSync`: the engine determines the
  count of the vertical blanking period, either through hardware APIs or a virtual software frame
  counter(TODO: add docs for even-odd evaluation); the engine then commits one frame buffer while
  discarding the other.

## TODOs

- [x] dual-fb setup
- [x] high-performance mock even-odd counter for none-NVIDIA GPUs
- [ ] multi-media
    - [ ] image
    - [ ] video
- [ ] python binding?

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

## References

[Theory of Human Tetrachromatic Color Experience and Printing](https://dl.acm.org/doi/10.1145/3658232)

[TetraPolyscope](https://github.com/i-geng/polyscope)
