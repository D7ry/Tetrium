
#include "backends/imgui_impl_vulkan.h"

#include "Quarkolor.h"

#include "ImGuiWidgetTetraViewerDemo.h"
#include "components/TextureManager.h"
#include "components/VulkanUtils.h"
#include "stb_image.h"

void ImGuiWidgetTetraViewerDemo::Draw(Quarkolor* engine, ColorSpace colorSpace)
{
    static bool init = false;
    static TextureManager::Texture tex;
    static VkDescriptorSet ds;
    if (!init) {
        tex = engine->_textureManager.GetTexture("../assets/textures/spot.png");
        ds = ImGui_ImplVulkan_AddTexture(tex.sampler, tex.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        init = true;
    }
    ImGui::Begin("Vulkan Texture Test");
    ImGui::Text("pointer = %p", ds);
    ImGui::Text("size = %d x %d", tex.width, tex.height);
    ImGui::Image((ImTextureID)ds, ImVec2(tex.width,tex.height));
    ImGui::End();
}
