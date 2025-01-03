```
___  ___ ___  __
 |  |__   |  |__) | |  |  |\/|
 |  |___  |  |  \ | \__/  |  |

```

Display engine targeting color experiences beyond regular human perception.

## Developing

Refer to [App.h](src/apps/App.h) for details and documentations on developing self-contained 
applications that writes to both RGB and OCV color spaces.

## Build

```bash
mkdir build && cd build
cmake ../ && make
```

### Dependencies


The following libraries are required at link time:
- vulkan SDK
- python3.11
- tiff
- freetype
- drm
- openal
- sndfile

### Building Hardware Even-Odd Sync

Tetrium can be built on all platforms for development and testing.
However, to use hardware frame counter for even-odd presentation, the following must be present:
1. Vulkan instance extension `VK_EXT_display_surface_counter`, for counting the actual vblank #.
2. Vulkan instance extension `VK_KHR_display` and `VK_EXT_direct_mode_display`, for establishing direct
connection between GPU and the display that allows for querying the API in (1).

Hardware-sync build is only available for linux, since neither windows nor MoltenVK support `VK_EXT_display_surface_counter`.  
In addition, only X11 protocol is supported.

NVIDIA RTX 30/40 series laptop GPU running in discrete graphics mode on Ubuntu 22.04(X11) and Fedora 41(X11) have been tested to be able to provide stable hardware frame counter.

## Run

Config loading is not supported yet. To configure run option, modify `Tetrium::InitOptions options`
field to set display mode.

## References

[Theory of Human Tetrachromatic Color Experience and Printing](https://dl.acm.org/doi/10.1145/3658232)

[TetriumColor](https://github.com/imjal/TetriumColor)

[TetraPolyscope](https://github.com/i-geng/polyscope)
