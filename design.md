## Engine Architecture
### Backend

Engine backend consists of core logic on vulkan rendering, presentation, and even-odd
frame synchronization. 

#### Even-Odd Frame Synchronization

To determine even/odd-ness of the frame to present, the engine relies on the total number of
vertical blanking periods % 2. This information is obtained using [vkGetSwapchainCounterEXT](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkGetSwapchainCounterEXT.html), when the engine runs under `kEvenOddHardwareSync`. 

`kEvenOddSoftwareSync` mode provides an inaccurate simulation for frame synchronization, useful for
developing on devices without discrete GPU.

### Developing Applications

Refer to [App.h](src/apps/App.h) for details and documentations on developing self-contained 
applications that writes to both RGB and OCV color spaces.