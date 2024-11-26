#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"

#include "components/VulkanUtils.h"

#include "AppTetraHueSphere.h"

namespace TetriumApp
{

// FIXME: figure out a way to resize fb
// by recreating fb on imgui window resize callback
const int FB_WIDTH = 1024;
const int FB_HEIGHT = 1024;

void AppTetraHueSphere::TickImGui(const TetriumApp::TickContextImGui& ctx)
{
    if (ImGui::Begin("TetraHueSphere")) {
        ImGui::Text("Hello, TetraHueSphere!");

        RenderContext& renderCtx = _renderContexts[ctx.currentFrameInFlight];
        // color & depth stencil clear value sliders
        ImGui::ColorEdit4("Clear Color", (float*)&_clearValues[0].color);
        ImGui::SliderFloat("Clear Depth", &_clearValues[1].depthStencil.depth, 0.f, 1.f);

        ImGui::Image(renderCtx.fb.imguiTextureId, ImVec2(FB_WIDTH, FB_HEIGHT));

        if (ImGui::Button("Close")) {
            ctx.controls.wantExit = true;
        }
    }
    ImGui::End();
}

void AppTetraHueSphere::TickVulkan(TetriumApp::TickContextVulkan& ctx)
{
    // DEBUG("ticking vulkan: {}", ctx.currentFrameInFlight);

    RenderContext& renderCtx = _renderContexts[ctx.currentFrameInFlight];
    // render to the correct framebuffer&texture


    // push render semaphore TODO: do we need semahpore here? since the cb is already scheduled
    ctx.commandBuffer.beginRenderPass(
        vk::RenderPassBeginInfo(
            _renderPass,
            renderCtx.fb.frameBuffer,
            vk::Rect2D({0, 0}, {FB_WIDTH, FB_HEIGHT}),
            _clearValues.size(),
            _clearValues.data()
        ),
        vk::SubpassContents::eInline
    );

    ctx.commandBuffer.endRenderPass();
}

void AppTetraHueSphere::Cleanup(TetriumApp::CleanupContext& ctx)
{
    DEBUG("Cleaning up TetraHueSphere...");
};

void AppTetraHueSphere::Init(TetriumApp::InitContext& ctx)
{
    DEBUG("Initializing TetraHueSphere...");
    initRenderPass(ctx);
    initDepthImage(ctx);
    // create all render contexts
    for (int i = 0; i < NUM_FRAME_IN_FLIGHT; i++) {
        initRenderContexts(_renderContexts[i], ctx);
    }

    // init clear values here
    _clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.f};
    _clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.f, 0.f);
};

void AppTetraHueSphere::initRenderContexts(RenderContext& ctx, TetriumApp::InitContext& initCtx)
{
    VirtualFrameBuffer& fb = ctx.fb;

    // create image & image view
    {
        // TODO: do we need to use swapchain's image format??
        VkFormat imageFormat = static_cast<VkFormat>(initCtx.swapchain.imageFormat);
        VulkanUtils::createImage(
            FB_WIDTH,
            FB_HEIGHT,
            imageFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            fb.image.image,
            fb.image.memory,
            initCtx.device.physicalDevice,
            initCtx.device.device
        );

        VkDevice device = initCtx.device.device;
        fb.image.view = VulkanUtils::createImageView(
            fb.image.image, device, imageFormat, VK_IMAGE_ASPECT_COLOR_BIT
        );
    }

    // create fb
    {
        ASSERT(_renderPass != VK_NULL_HANDLE);
        std::array<vk::ImageView, 2> attachments = {fb.image.view, _depthImage.view};
        ASSERT(fb.image.view != VK_NULL_HANDLE);
        ASSERT(_depthImage.view != VK_NULL_HANDLE);

        vk::FramebufferCreateInfo fbCreateInfo(
            {}, _renderPass, attachments.size(), attachments.data(), FB_WIDTH, FB_HEIGHT, 1
        );

        fb.frameBuffer = initCtx.device.device.createFramebuffer(fbCreateInfo);
    }

    // create sampler
    {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 0;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        fb.sampler = initCtx.device.device.createSampler(samplerInfo);
    }

    // create imgui texture
    //
    fb.imguiTextureId = ImGui_ImplVulkan_AddTexture(
        fb.sampler, fb.image.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
}

void AppTetraHueSphere::initRenderPass(TetriumApp::InitContext& initCtx)
{
    DEBUG("Creating render pass...");
    // set up color/depth layout
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // depth attachment ref for subpass, only used when `createDepthAttachment` is true
    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    // color, and optionally depth attachments
    VkAttachmentDescription attachments[2];
    uint32_t attachmentCount = 2;
    // create color attachment
    {
        VkAttachmentDescription& colorAttachment = attachments[0];
        colorAttachment = {};
        colorAttachment.format = static_cast<VkFormat>(initCtx.swapchain.imageFormat);
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        // don't care about stencil
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // paint to the virual fb that read as a texture by imgui pass
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    // add in depth attachment when `createDepthAttachment` is set to true
    // we have already specified above to have depthAttachmentRef for subpass creation
    {
        VkAttachmentDescription& depthAttachment = attachments[1];
        depthAttachment = {};
        depthAttachment = VkAttachmentDescription{};
        depthAttachment.format = static_cast<VkFormat>(initCtx.device.depthFormat);
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    // TODO: refine dependency here
    VkSubpassDependency dependency = VkSubpassDependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL, // wait for all previous subpasses
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // what to wait
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // what to not execute
        // we care about src and dst access masks only when there's a potential data race.
        .srcAccessMask = VK_ACCESS_NONE,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    };

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.pAttachments = attachments;
    renderPassInfo.attachmentCount = attachmentCount;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;

    renderPassInfo.pDependencies = &dependency;

    _renderPass = initCtx.device.device.createRenderPass(renderPassInfo, nullptr);

    ASSERT(_renderPass != VK_NULL_HANDLE);
    DEBUG("render pass created");
}

void AppTetraHueSphere::initDepthImage(TetriumApp::InitContext& initCtx)
{
    DEBUG("Creating depth buffer...");
    VkFormat depthFormat = static_cast<VkFormat>(initCtx.device.depthFormat);

    VulkanUtils::createImage(
        FB_WIDTH,
        FB_HEIGHT,
        depthFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        _depthImage.image,
        _depthImage.memory,
        initCtx.device.physicalDevice,
        initCtx.device.device
    );

    VkDevice device = initCtx.device.device;
    _depthImage.view = VulkanUtils::createImageView(
        _depthImage.image, device, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT
    );
}
} // namespace TetriumApp
