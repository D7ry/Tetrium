# Quarkolor

Vulkan-based tetrachromacy display engine.

## TODOs

- [ ] dual-projector setup
- [ ] high-performance mock even-odd counter for none-NVIDIA GPUs
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

### Unix

#### Apple

set `VULKAN_LIB_PATH` in `CMakeLists.txt` to your own path after installing Vulkan SDK

#### Linux

install `X11` and `Xrandr` for direct display access, for hardware even-odd frame sync

#### Commands
```bash
git clone -recurse-submodules git@github.com:D7ry/quarkolor.git
mkdir build
cd build
cmake ../
make
```

### Windows

tbd...

## References

[TetraPolyscope](https://github.com/i-geng/polyscope)
