#if __APPLE__
#pragma once
#include <MoltenVK/vk_mvk_moltenvk.h>

namespace MoltenVKConfig
{
inline void Setup() {}

// returns layer settings creat info to be attached to
// `VkInstanceCreateInfo::pNext` Currently, this info:
//
// - enables metal argument buffers, that increases the
// number of descriptor sets and texture array sizes to be
// on-par with modern GPU.
inline VkLayerSettingsCreateInfoEXT GetLayerSettingsCreatInfo()
{

    uint32_t u32True = true;
    VkLayerSettingsCreateInfoEXT layerSettingsCreateInfo = {};
    layerSettingsCreateInfo.sType = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT;
    static std::vector<VkLayerSettingEXT> settings;
    VkLayerSettingEXT argumentBufferSetting = {};
    argumentBufferSetting.pLayerName = "MoltenVK";
    argumentBufferSetting.pSettingName = "MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS";
    argumentBufferSetting.valueCount = 1;
    argumentBufferSetting.type = VK_LAYER_SETTING_TYPE_BOOL32_EXT;
    argumentBufferSetting.pValues = std::addressof(u32True);
    settings.push_back(argumentBufferSetting);

    VkLayerSettingEXT performanceLoggingSetting = {};
    performanceLoggingSetting.pLayerName = "MoltenVK";
    performanceLoggingSetting.pSettingName = "MVK_CONFIG_PERFORMANCE_LOGGING_INLINE";
    performanceLoggingSetting.valueCount = 1;
    performanceLoggingSetting.type = VK_LAYER_SETTING_TYPE_BOOL32_EXT;
    performanceLoggingSetting.pValues = std::addressof(u32True);
    settings.push_back(performanceLoggingSetting);

    layerSettingsCreateInfo.settingCount = static_cast<uint32_t>(settings.size());
    layerSettingsCreateInfo.pSettings = settings.data();

    return layerSettingsCreateInfo;
}

}; // namespace MoltenVKConfig
#endif // __APPLE__
