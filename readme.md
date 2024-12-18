```
___  ___ ___  __
 |  |__   |  |__) | |  |  |\/|
 |  |___  |  |  \ | \__/  |  |

```

Vulkan-based tetrachromacy display engine.
## Engine Architecture

### Developing Applications

Refer to [App.h](src/apps/App.h) for details and documentations on developing self-contained 
applications that writes to both RGB and OCV color spaces.

### Backend

Engine backend consists of core logic on vulkan rendering, presentation, and even-odd
frame synchronization. 

#### Even-Odd Frame Synchronization

To determine even/odd-ness of the frame to present, the engine relies on the total number of
vertical blanking periods % 2. This information is obtained using [vkGetSwapchainCounterEXT](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkGetSwapchainCounterEXT.html), when the engine runs under `kEvenOddHardwareSync`. 

`kEvenOddSoftwareSync` mode provides an inaccurate simulation for frame synchronization, useful for
developing on devices without discrete GPU.


## Requirements

### All Systems

- `CMake`
- [Vulkan SDK](https://vulkan.lunarg.com/)
- `make` or `ninja`
- openAL
- libsndfile
- openSSL

### Hardware Even-odd frame sync

- NVIDIA GPU supporting `VK_EXT_display_surface_counter`

## Build

### Apple

set `VULKAN_LIB_PATH` and `MOLTENVK_LIB_PATH` in `CMakeLists.txt` to your own path after installing Vulkan SDK

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
