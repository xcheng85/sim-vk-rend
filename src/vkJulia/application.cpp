#include <format>
#include <random>
#include <application.h>
#include <window.h>
#include <context.h>
#include <cameraBase.h>

#include <glm/ext.hpp>
#include <glm/glm.hpp>

#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>

#define VK_NO_PROTOTYPES // for volk
#define VOLK_IMPLEMENTATION

// triple-buffer
static constexpr int MAX_FRAMES_IN_FLIGHT = 3;
static constexpr int MAX_DESCRIPTOR_SETS = 1 * MAX_FRAMES_IN_FLIGHT + 1 + 4;

// static constexpr int MAX_DESCRIPTOR_SETS = 1000;
//  Default fence timeout in nanoseconds
#define DEFAULT_FENCE_TIMEOUT 100000000000

void VkApplication::init()
{
    _ctx.createSwapChain();
    _swapChainRenderPass = _ctx.createSwapChainRenderPass();
    _ctx.initDefaultCommandBuffers();

    createShaderModules();
    createUniformBuffers();

    createDescriptorSetLayout();
    createDescriptorPool();
    allocateDescriptorSets();

    createGraphicsPipeline();
    createSwapChainFramebuffers();
    bindResourceToDescriptorSets();

#ifdef VK_PRERECORD_COMMANDS
    // prebuild command buffers for all swapchain images; no one-time flag
    recordCommandBuffersForAllSwapChainImage();
#endif
}

void VkApplication::teardown()
{
    auto logicalDevice = _ctx.getLogicDevice();
    auto vmaAllocator = _ctx.getVmaAllocator();
    vkDeviceWaitIdle(logicalDevice);
    deleteSwapChain();

    // shader module
    vkDestroyShaderModule(logicalDevice, _vsShaderModule, nullptr);
    vkDestroyShaderModule(logicalDevice, _fsShaderModule, nullptr);

    // shader data
    vkDestroyDescriptorPool(logicalDevice, _descriptorSetPool, nullptr);
    for (const auto &descriptorSetLayout : _descriptorSetLayouts)
    {
        vkDestroyDescriptorSetLayout(logicalDevice, descriptorSetLayout, nullptr);
    }
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
    }
    for (const auto &[pipelineType, pipeline] : std::get<0>(_graphicsPipelineEntity))
    {
        vkDestroyPipeline(logicalDevice, pipeline, nullptr);
    }
    vkDestroyPipelineLayout(logicalDevice, std::get<1>(_graphicsPipelineEntity), nullptr);
    vkDestroyRenderPass(logicalDevice, _swapChainRenderPass, nullptr);
}

void VkApplication::createShaderModules()
{
    log(Level::Info, "-->createShaderModules");
    auto logicalDevice = _ctx.getLogicDevice();
    // lateral for filepath in modern c++
    const auto shadersPath = getAssetPath();
    const auto vertexShaderPath = shadersPath + "/triangle.vert";
    const auto fragShaderPath = shadersPath + "/triangle.frag";
    log(Level::Info, "vertexShaderPath: ", vertexShaderPath);
    log(Level::Info, "fragShaderPath: ", fragShaderPath);
    _vsShaderModule = createShaderModule(
        logicalDevice,
        vertexShaderPath,
        "main",
        "triangle.vert");
    _fsShaderModule = createShaderModule(
        logicalDevice,
        fragShaderPath,
        "main",
        "triangle.frag");
    log(Level::Info, "<--createShaderModules");
}

void VkApplication::createUniformBuffers()
{
}

void VkApplication::createDescriptorSetLayout()
{
    std::vector<std::vector<VkDescriptorSetLayoutBinding>> setBindings(0);
    _descriptorSetLayouts = _ctx.createDescriptorSetLayout(setBindings);
}

void VkApplication::createDescriptorPool()
{
    _descriptorSetPool = _ctx.createDescriptorSetPool({}, 100);
}

void VkApplication::allocateDescriptorSets()
{
}

void VkApplication::createGraphicsPipeline()
{
    // life cycle of string must be longer.
    const std::string entryPoint{"main"s};

    _graphicsPipelineEntity = _ctx.createGraphicsPipeline(
        {{VK_SHADER_STAGE_VERTEX_BIT,
          make_tuple(_vsShaderModule, entryPoint.c_str(), nullptr)},
         {VK_SHADER_STAGE_FRAGMENT_BIT,
          make_tuple(_fsShaderModule, entryPoint.c_str(), nullptr)}},
        _descriptorSetLayouts,
        // for push constant
        {},
        _swapChainRenderPass);
}

void VkApplication::createSwapChainFramebuffers()
{
    const auto swapChainImageViews = _ctx.getSwapChainImageViews();
    auto swapChainExtent = _ctx.getSwapChainExtent();
    _swapChainFramebuffers.reserve(swapChainImageViews.size());
    for (size_t i = 0; i < swapChainImageViews.size(); ++i)
    {
        _swapChainFramebuffers.push_back(_ctx.createFramebuffer(
            "swapchainFBO" + i,
            _swapChainRenderPass,
            {swapChainImageViews[i]},
            VK_NULL_HANDLE,
            VK_NULL_HANDLE,
            swapChainExtent.width,
            swapChainExtent.height));
    }
}

void VkApplication::bindResourceToDescriptorSets()
{
    auto logicalDevice = _ctx.getLogicDevice();
}

void VkApplication::deleteSwapChain()
{
    const auto logicalDevice = _ctx.getLogicDevice();
    for (size_t i = 0; i < _swapChainFramebuffers.size(); ++i)
    {
        vkDestroyFramebuffer(logicalDevice, _swapChainFramebuffers[i], nullptr);
    }
}

void VkApplication::renderPerFrame()
{
    auto logicalDevice = _ctx.getLogicDevice();
    auto graphicsQueue = _ctx.getGraphicsComputeQueue();
    auto presentationQueue = _ctx.getPresentationQueue();
    auto swapChain = _ctx.getSwapChain();
    uint32_t swapChainImageIndex = _ctx.getSwapChainImageIndexToRender();

#ifndef VK_PRERECORD_COMMANDS
    auto [currentFrameId, cmdBuffersForRendering] = _ctx.getCommandBufferForRendering();
    // // no timeout set
    // VK_CHECK(vkWaitForFences(logicalDevice, 1, &fenceToWait, VK_TRUE, UINT64_MAX));
    // // vkWaitForFences ensure the previous command is submitted from the host, now it can be modified.
    // VK_CHECK(vkResetCommandBuffer(cmdToRecord, 0));
    _ctx.BeginRecordCommandBuffer(cmdBuffersForRendering);
    // 2. main rendering pass
    const auto cmdToRecord = std::get<COMMAND_BUFFER_ENTITY_OFFSET::COMMAND_BUFFER>(cmdBuffersForRendering);
    recordCommandBufferForOneSwapChainImage(currentFrameId, cmdToRecord, swapChainImageIndex);

    _ctx.EndRecordCommandBuffer(cmdBuffersForRendering);
#endif
    {
        ZoneScopedN("CmdMgr: submit");
        _ctx.submitCommand();
    }
    _ctx.present(swapChainImageIndex);
    _ctx.advanceCommandBuffer();
}

void VkApplication::recordCommandBuffersForAllSwapChainImage()
{
    const auto swapchainImages = _ctx.getSwapChainImages();
    auto &cmdBuffersForRendering = _ctx.getCommandBufferForRenderingForAllSwapChains();

    // // no timeout set
    // VK_CHECK(vkWaitForFences(logicalDevice, 1, &fenceToWait, VK_TRUE, UINT64_MAX));
    // // vkWaitForFences ensure the previous command is submitted from the host, now it can be modified.
    // VK_CHECK(vkResetCommandBuffer(cmdToRecord, 0));

    // currtFrameId == swapchainIndex
    for (int i = 0; i < swapchainImages.size(); i++)
    {
        _ctx.BeginRecordCommandBuffer(cmdBuffersForRendering[i]);

        recordCommandBufferForOneSwapChainImage(
            i,
            std::get<COMMAND_BUFFER_ENTITY_OFFSET::COMMAND_BUFFER>(cmdBuffersForRendering[i]),
            i);

        _ctx.EndRecordCommandBuffer(cmdBuffersForRendering[i]);
    }
}

void VkApplication::recordCommandBufferForOneSwapChainImage(
    uint32_t currentFrameId,
    VkCommandBuffer commandBuffer,
    uint32_t swapChainImageIndex)
{
    auto swapChainExtent = _ctx.getSwapChainExtent();
    const auto tracyCtx = _ctx.getTracyContext();
    const auto swapchainImages = _ctx.getSwapChainImages();
    const auto swapchainImageViews = _ctx.getSwapChainImageViews();
    TracyPlot("Swapchain image index", (int64_t)swapChainImageIndex);
    // Begin Render Pass, only 1 render pass

    // dynamic rendering implementation extra image memory barrier setup

    // answer the following questions
    // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT: output merger (after blending)
    // ignore the src and dst queue family index config since it is within the same queue
    // uint32_t                   srcQueueFamilyIndex;
    // uint32_t                   dstQueueFamilyIndex;

    // VkPipelineStageFlags2      srcStageMask; VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    // VkAccessFlags2             srcAccessMask;   VK_ACCESS_NONE
    // VkPipelineStageFlags2      dstStageMask; VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    // VkAccessFlags2             dstAccessMask;  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT (specifies write access during stage: )
    // VkImageLayout              oldLayout; VK_IMAGE_LAYOUT_UNDEFINED
    // VkImageLayout              newLayout; VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL
    // ignore
    // uint32_t                   srcQueueFamilyIndex;
    // uint32_t                   dstQueueFamilyIndex;

    // swapchain image
    // VkImage                    image;
    // VkImageSubresourceRange    subresourceRange;

#ifdef VK_DYNAMIC_RENDERING
    // v1.3 dynamic rendering
    VkImageMemoryBarrier2 acquireBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_NONE,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // it is for renderpass render into swapchain image
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .image = swapchainImages[swapChainImageIndex],
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    VkDependencyInfo dependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &acquireBarrier,
    };
    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);

    // specific struct to dynamic rendering
    VkRenderingAttachmentInfo colorAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    colorAttachment.imageView = swapchainImageViews[swapChainImageIndex];
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    // similar to what desc configed for creating renderPass
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = {0.0f, 0.0f, 0.0f, 0.0f};

    VkRenderingInfo renderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO_KHR};
    renderingInfo.renderArea = {0, 0, swapChainExtent.width, swapChainExtent.height};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;

    // Start a dynamic rendering section
    vkCmdBeginRendering(commandBuffer, &renderingInfo);

#endif
// prior to vk1.3 without using dynamic rendering (which exclude renderpass and framebuffer)
#ifndef VK_DYNAMIC_RENDERING
    constexpr VkClearValue clearColor{0.0f, 0.0f, 0.0f, 0.0f};
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = _swapChainRenderPass;
    // fbo corresponding to the swapchain image index
    renderPassInfo.framebuffer = _swapChainFramebuffers[swapChainImageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChainExtent;
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
#endif
    // Dynamic States (when create the graphics pipeline, they are not specified)
    VkViewport viewport{};
    viewport.width = (float)swapChainExtent.width;
    viewport.height = (float)swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = swapChainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    vkCmdSetDepthTestEnable(commandBuffer, VK_TRUE);

    // apply graphics pipeline to the cmd
    auto graphicsPipelineLookUpTable = std::get<0>(_graphicsPipelineEntity);
    auto graphicsPipelineLayout = std::get<1>(_graphicsPipelineEntity);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLookUpTable[GRAPHICS_PIPELINE_SEMANTIC::NORMAL]);

    {
        // extra scope as required by TracyVkZone
        TracyVkZone(tracyCtx, commandBuffer, "main draw pass");
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    }

#if defined(VK_DYNAMIC_RENDERING)
    vkCmdEndRendering(commandBuffer);
#else
    vkCmdEndRenderPass(commandBuffer);
#endif

    TracyVkCollect(tracyCtx, commandBuffer);
}