#include <format>
#include <random>
#include <application.h>
#include <context.h>
#include <cameraBase.h>

#include <glm/ext.hpp>
#include <glm/glm.hpp>

#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>

#include <scene.h>
#include <mathUtils.h>
#include <julia.h>
#include <cuDevice.h>
#include <cuPredefines.h>

// #define VK_USE_PLATFORM_WIN32_KHR
#define VK_NO_PROTOTYPES // for volk
#define VOLK_IMPLEMENTATION

// triple-buffer
static constexpr int MAX_FRAMES_IN_FLIGHT = 3;
static constexpr int MAX_DESCRIPTOR_SETS = 1 * MAX_FRAMES_IN_FLIGHT + 1 + 4;

// static constexpr int MAX_DESCRIPTOR_SETS = 1000;
//  Default fence timeout in nanoseconds
#define DEFAULT_FENCE_TIMEOUT 100000000000

using namespace vkEngine::math;
using namespace cudaEngine;

enum DESC_LAYOUT_SEMANTIC : int
{
    TEX_SAMP = 0,
    DESC_LAYOUT_SEMANTIC_SIZE
};

#include <cuda.h>
#include <cuda_runtime.h>

void VkApplication::init()
{
    _ctx.createSwapChain();
    _swapChainRenderPass = _ctx.createSwapChainRenderPass();
    _ctx.initDefaultCommandBuffers();

    loadTextures();

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
    auto logicalDevice = _ctx.getLogicDevice();
    getVkImageMemoryHandle(logicalDevice, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT);
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

    for (const auto &imageEntity : _imageEntities)
    {
        const auto image = std::get<IMAGE_ENTITY_OFFSET::IMAGE>(imageEntity);
        const auto imageView = std::get<IMAGE_ENTITY_OFFSET::IMAGE_VIEW>(imageEntity);
        const auto imageAllocation = std::get<IMAGE_ENTITY_OFFSET::IMAGE_VMA_ALLOCATION>(imageEntity);

        vkDestroyImageView(logicalDevice, imageView, nullptr);
        vmaDestroyImage(vmaAllocator, image, imageAllocation);
    }

    for (const auto &samplerEntity : _samplerEntities)
    {
        const auto sampler = std::get<0>(samplerEntity);
        vkDestroySampler(logicalDevice, sampler, nullptr);
    }

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
    std::vector<std::vector<VkDescriptorSetLayoutBinding>> setBindings(DESC_LAYOUT_SEMANTIC_SIZE);
    // layout(set = 0, binding = 0) uniform texture2D BindlessImage2D[];
    // layout(set = 0, binding = 1) uniform sampler BindlessSampler;
    setBindings[DESC_LAYOUT_SEMANTIC::TEX_SAMP].resize(2);
    setBindings[DESC_LAYOUT_SEMANTIC::TEX_SAMP][0].binding = 0; // depends on the shader: set 0, binding = 0
    setBindings[DESC_LAYOUT_SEMANTIC::TEX_SAMP][0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    setBindings[DESC_LAYOUT_SEMANTIC::TEX_SAMP][0].descriptorCount = 1;
    setBindings[DESC_LAYOUT_SEMANTIC::TEX_SAMP][0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    setBindings[DESC_LAYOUT_SEMANTIC::TEX_SAMP][1].binding = 1; // depends on the shader: set 0, binding = 0
    setBindings[DESC_LAYOUT_SEMANTIC::TEX_SAMP][1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    setBindings[DESC_LAYOUT_SEMANTIC::TEX_SAMP][1].descriptorCount = 1;
    setBindings[DESC_LAYOUT_SEMANTIC::TEX_SAMP][1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    _descriptorSetLayouts = _ctx.createDescriptorSetLayout(setBindings);
}

void VkApplication::createDescriptorPool()
{
    _descriptorSetPool = _ctx.createDescriptorSetPool({
                                                          {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 10},
                                                          {VK_DESCRIPTOR_TYPE_SAMPLER, 10},
                                                      },
                                                      100);
}

void VkApplication::allocateDescriptorSets()
{
    // only one set in frag.
    // layout(set = 0, binding = 0) uniform texture2D BindlessImage2D[];
    // layout(set = 0, binding = 1) uniform sampler BindlessSampler;
    _descriptorSets = _ctx.allocateDescriptorSet(_descriptorSetPool,
                                                 {{&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::TEX_SAMP],
                                                   1}});
}

void VkApplication::createGraphicsPipeline()
{
    // life cycle of string must be longer.
    const std::string entryPoint{"main"};

    _graphicsPipelineEntity = _ctx.createGraphicsPipeline(
        {{VK_SHADER_STAGE_VERTEX_BIT,
          std::make_tuple(_vsShaderModule, entryPoint.c_str(), nullptr)},
         {VK_SHADER_STAGE_FRAGMENT_BIT,
          std::make_tuple(_fsShaderModule, entryPoint.c_str(), nullptr)}},
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
    {
        const auto dstSets = _descriptorSets[&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::TEX_SAMP]];
        ASSERT(dstSets.size() == 1, "TEX_SAMP descriptor set size is 1");

        // mimic asyn-io callback case
        uint32_t offset = 0;
        for (const auto &imageEntity : _imageEntities)
        {
            _ctx.bindTextureToDescriptorSet(
                {imageEntity},
                dstSets[0],
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                offset++);
        }

        // for glb samplers
        std::vector<VkSampler> imageSamplers;
        std::transform(_samplerEntities.begin(), _samplerEntities.end(),
                       std::back_inserter(imageSamplers),
                       [](const auto &samplerEntity)
                       {
                           return std::get<0>(samplerEntity);
                       });
        _ctx.bindSamplerToDescriptorSet(
            imageSamplers,
            dstSets[0],
            VK_DESCRIPTOR_TYPE_SAMPLER,
            1);
    }
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
    const auto firstSetId = 0;
    const auto numSets = 1;
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            graphicsPipelineLayout, firstSetId, numSets,
                            &_descriptorSets[&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::TEX_SAMP]][0],
                            0, nullptr);
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

void VkApplication::loadTextures()
{
    std::string filename = getAssetPath() + "/lavaplanet_color_rgba.ktx";
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
#if defined(__ANDROID__)
#else
    cuComplexf c(-0.88f, 0.18f);

    // // _texture = std::make_unique<TextureKtx>(filename);
    _texture = std::make_unique<JuliaSet>(512, 1.5f, 200, c);
#endif

    const auto textureMipLevels = getMipLevelsCount(_texture->width(),
                                                    _texture->height());
    const uint32_t textureLayoutCount = 1;
    const VkExtent3D textureExtent = {static_cast<uint32_t>(_texture->width()),
                                      static_cast<uint32_t>(_texture->height()), 1};
    const bool generateMipmaps = true;
    const auto imageEntity = _ctx.createImage("texuture_0",
                                              VK_IMAGE_TYPE_2D,
                                              VK_FORMAT_R8G8B8A8_UNORM,
                                              textureExtent,
                                              textureMipLevels,
                                              textureLayoutCount,
                                              VK_SAMPLE_COUNT_1_BIT,
                                              // usage here: both dst and src as mipmap generation
                                              VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                                  VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                              generateMipmaps);

    _imageEntities.emplace_back(std::move(imageEntity));

    // write raw data from cpu to the mipmap level 0 of image
    const auto stagingBufferSizeForImage = std::get<IMAGE_ENTITY_OFFSET::IMAGE_VMA_ALLOCATION_INFO>(imageEntity).size;
    _imageStagingBuffers.emplace_back(_ctx.createStagingBuffer(
        "Staging Buffer Texture_0",
        stagingBufferSizeForImage));

    // io commands
    auto cmdBuffersForIO = _ctx.getCommandBufferForIO();
    _ctx.BeginRecordCommandBuffer(cmdBuffersForIO);

    _ctx.writeImage(
        _imageEntities.back(),
        _imageStagingBuffers.back(),
        cmdBuffersForIO,
        _texture->data());

    _ctx.EndRecordCommandBuffer(cmdBuffersForIO);

    const auto commandBuffer = std::get<COMMAND_BUFFER_ENTITY_OFFSET::COMMAND_BUFFER>(cmdBuffersForIO);
    const auto fence = std::get<COMMAND_BUFFER_ENTITY_OFFSET::FENCE>(cmdBuffersForIO);
    const auto cmdQueue = std::get<COMMAND_BUFFER_ENTITY_OFFSET::QUEUE>(cmdBuffersForIO);

    VkSubmitInfo submitInfo{};
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.pWaitSemaphores = VK_NULL_HANDLE;
    // This depends on nobody
    // // only useful when having waitsemaphore
    // submitInfo.pWaitDstStageMask = &flags;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    // no body depends on him as well
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores = VK_NULL_HANDLE;

    // must reset fence of waiting (Signal -> not signal, due to initally fence is created with state "signaled")
    auto logicalDevice = _ctx.getLogicDevice();
    VK_CHECK(vkResetFences(logicalDevice, 1, &fence));
    // This will change state of fence to signaled
    VK_CHECK(vkQueueSubmit(cmdQueue, 1, &submitInfo, fence));
    const auto result = vkWaitForFences(logicalDevice, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT);
    if (result == VK_TIMEOUT)
    {
        // should not happen
        ASSERT(false, "vkWaitForFences somehow Timed out !");
        vkDeviceWaitIdle(logicalDevice);
    }

    // staging (pinned buffer in host is not needed anymore, clean)
    auto vmaAllocator = _ctx.getVmaAllocator();
    for (size_t i = 0; i < _imageStagingBuffers.size(); ++i)
    {
        vmaDestroyBuffer(
            vmaAllocator,
            std::get<BUFFER_ENTITY_UID::BUFFER>(_imageStagingBuffers[i]),
            std::get<BUFFER_ENTITY_UID::VMA_ALLOCATION>(_imageStagingBuffers[i]));
    }

    _samplerEntities.emplace_back(_ctx.createSampler("sampler0"));
}