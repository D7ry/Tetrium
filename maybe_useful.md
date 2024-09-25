Stuff that may be useful eventually

Code to power on display
```cpp
#if POWER_ON_DISPLAY
    static bool poweredOn = false;
    if (false && !poweredOn) { // power display on, seems unnecessary
        poweredOn = true;
        // power on display
        DEBUG("Turning display on...");
        VkDisplayPowerInfoEXT powerInfo{
            .sType = VK_STRUCTURE_TYPE_DISPLAY_POWER_INFO_EXT,
            .pNext = VK_NULL_HANDLE,
            .powerState = VkDisplayPowerStateEXT::VK_DISPLAY_POWER_STATE_ON_EXT
        };
        PFN_vkDisplayPowerControlEXT fnPtr = reinterpret_cast<PFN_vkDisplayPowerControlEXT>(
            vkGetInstanceProcAddr(_instance, "vkDisplayPowerControlEXT")
        );
        ASSERT(fnPtr);
        VK_CHECK_RESULT(fnPtr(_device->logicalDevice, _mainProjectorDisplay.display, &powerInfo));
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
#endif // POWER_ON_DISPLAY

```
