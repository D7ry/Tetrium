
#include "backends/imgui_impl_vulkan.h"

#include "Tetrium.h"

#include "ImGuiWidgetTetraViewerDemo.h"
#include "components/TextureManager.h"

void ImGuiWidgetTetraViewerDemo::Draw(Tetrium* engine, ColorSpace colorSpace)
{
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 1));
    if (ImGui::Begin("Tetra Viewer")) {
        switch (colorSpace) {
        case ColorSpace::RGB:
            if (!rgbLoaded) {
                auto tex = engine->_textureManager.GetTexture(TETRA_IMAGE_PATH_RGB);
                auto ds = ImGui_ImplVulkan_AddTexture(
                    tex.sampler, tex.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                );
                rgb.height = tex.height;
                rgb.width = tex.width;
                rgb.ds = ds;
                rgbLoaded = true;
            }

            ImGui::Image((ImTextureID)rgb.ds, ImVec2(rgb.width, rgb.height));
            break;
        case ColorSpace::OCV:
            if (!cmyLoaded) {
                auto tex = engine->_textureManager.GetTexture(TETRA_IMAGE_PATH_CMY);
                auto ds = ImGui_ImplVulkan_AddTexture(
                    tex.sampler, tex.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                );
                cmy.height = tex.height;
                cmy.width = tex.width;
                cmy.ds = ds;
                cmyLoaded = true;
            }
            ImGui::Image((ImTextureID)cmy.ds, ImVec2(cmy.width, cmy.height));
            break;
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
}
