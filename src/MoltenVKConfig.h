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
inline VkLayerSettingsCreateInfoEXT GetLayerSettingsCreatInfo() {
    VkLayerSettingsCreateInfoEXT layerSettingsCreateInfo = {};
    layerSettingsCreateInfo.sType
        = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT;
    static std::vector<VkLayerSettingEXT> settings;
    VkLayerSettingEXT traceSetting = {};
    traceSetting.pLayerName = "MoltenVK";
    traceSetting.pSettingName = "MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS";
    traceSetting.valueCount = 1;
    traceSetting.type = VK_LAYER_SETTING_TYPE_BOOL32_EXT;
    static uint32_t value = true;
    traceSetting.pValues = std::addressof(value);
    settings.push_back(traceSetting);

    layerSettingsCreateInfo.settingCount
        = static_cast<uint32_t>(settings.size());
    layerSettingsCreateInfo.pSettings = settings.data();

    return layerSettingsCreateInfo;
}

}; // namespace MoltenVKConfig
#endif // __APPLE__
