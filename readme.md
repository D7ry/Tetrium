# Quarkolor

## TODOs

- [ ] dual-projector setup
- [ ] high-performance mock even-odd counter for none-NVIDIA GPUs
- [ ] multi-media
    - [ ] image
    - [ ] video
- [ ] python binding?

## Requirements

- NVIDIA GPU with support for `VK_EXT_display_surface_counter`
- xcb
- Vulkan SDK(MoltenVK for MacOS)
- a custom-made projector

## Build

### Unix
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
