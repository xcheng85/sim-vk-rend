#include <format>
#include <application.h>
#include <window.h>
#include <context.h>
#include <camera.h>

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

    createShaderModules();
    createUniformBuffers();

    // application logic
    _cmdBuffers.insert(std::make_pair(COMMAND_SEMANTIC::RENDERING,
                                      _ctx.createGraphicsCommandBuffers("rendering", MAX_FRAMES_IN_FLIGHT,
                                                                        MAX_FRAMES_IN_FLIGHT, VK_FENCE_CREATE_SIGNALED_BIT)));
    // mipmap requires graphics capablilities
    // VK_FENCE_CREATE_SIGNALED_BIT to unify waitFence behavior in beginRecordCommandBuffer
    _cmdBuffers.insert(std::make_pair(COMMAND_SEMANTIC::IO,
                                      _ctx.createGraphicsCommandBuffers("io", 1, 1, VK_FENCE_CREATE_SIGNALED_BIT)));

    // vao, textures and glb all depends on host-device io
    // one-time commandBuffer _uploadCmd
    preHostDeviceIO();
    // loadVao();
    //  must prior to bindResourceToDescriptorSets due to imageView
    // loadTextures();
    loadGLB();
    createDescriptorSetLayout();
    createDescriptorPool();
    allocateDescriptorSets();
    createGraphicsPipeline();
    createSwapChainFramebuffers();
    createPerFrameSyncObjects();
    postHostDeviceIO();
    bindResourceToDescriptorSets();

    // _initialized = true;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                    void * /* pUserData */)
{
    // LOGE("MessageID: %s %d \nMessage: %s\n\n", pCallbackData->pMessageIdName,
    //      pCallbackData->messageIdNumber, pCallbackData->pMessage);
    return VK_FALSE;
}

void VkApplication::teardown()
{
    auto logicalDevice = _ctx.getLogicDevice();
    auto vmaAllocator = _ctx.getVmaAllocator();
    vkDeviceWaitIdle(logicalDevice);
    deleteSwapChain();

    // // delete io related commandbuffers and fence
    // vkDestroyFence(logicalDevice, _ioFence, nullptr);
    // // free cmdBuffer for io purpose
    // vkFreeCommandBuffers(logicalDevice, _commandPool, 1, &_uploadCmd);

    for (const auto &[cmdSemantic, cmdEntity] : _cmdBuffers)
    {
        VkCommandPool p{VK_NULL_HANDLE};
        for (const auto &t : cmdEntity)
        {
            const auto &cmdPool = std::get<0>(t);
            const auto &cmdBuffer = std::get<1>(t);
            const auto &cmdFence = std::get<2>(t);
            p = cmdPool;

            vkDestroyFence(logicalDevice, cmdFence, nullptr);
            vkFreeCommandBuffers(logicalDevice, cmdPool, 1, &cmdBuffer);
        }
        vkDestroyCommandPool(logicalDevice, p, nullptr);
    }

    // shader module
    vkDestroyShaderModule(logicalDevice, _vsShaderModule, nullptr);
    vkDestroyShaderModule(logicalDevice, _fsShaderModule, nullptr);

    // texture
    // vkDestroyImageView(logicalDevice, _imageView, nullptr);
    // vkDestroyImage(logicalDevice, _image, nullptr);
    // vkDestroySampler(logicalDevice, _sampler, nullptr);
    // vmaFreeMemory(vmaAllocator, _vmaImageAllocation);

    // glb
    for (const auto &imageEntity : _glbImageEntities)
    {
        const auto image = std::get<0>(imageEntity);
        const auto imageView = std::get<1>(imageEntity);
        const auto imageAllocation = std::get<2>(imageEntity);

        vkDestroyImageView(logicalDevice, imageView, nullptr);
        vmaDestroyImage(vmaAllocator, image, imageAllocation);
    }

    for (const auto &samplerEntity : _glbSamplerEntities)
    {
        const auto sampler = std::get<0>(samplerEntity);
        vkDestroySampler(logicalDevice, sampler, nullptr);
    }

    vmaDestroyBuffer(vmaAllocator, std::get<0>(_compositeVB), std::get<1>(_compositeVB));
    vmaDestroyBuffer(vmaAllocator, std::get<0>(_compositeIB), std::get<1>(_compositeIB));
    vmaDestroyBuffer(vmaAllocator, std::get<0>(_compositeMatB), std::get<1>(_compositeMatB));
    vmaDestroyBuffer(vmaAllocator, std::get<0>(_indirectDrawB), std::get<1>(_indirectDrawB));
    // shader data
    vkDestroyDescriptorPool(logicalDevice, _descriptorSetPool, nullptr);
    for (const auto &descriptorSetLayout : _descriptorSetLayouts)
    {
        vkDestroyDescriptorSetLayout(logicalDevice, descriptorSetLayout, nullptr);
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        // vma buffer
        // unmap a buffer not mapped will crash
        // vmaUnmapMemory(_vmaAllocator, _vmaAllocations[i]);
        vmaDestroyBuffer(vmaAllocator, std::get<0>(_uniformBuffers[i]), std::get<1>(_uniformBuffers[i]));
        // sync
        vkDestroySemaphore(logicalDevice, _imageCanAcquireSemaphores[i], nullptr);
        vkDestroySemaphore(logicalDevice, _imageRendereredSemaphores[i], nullptr);
    }

    // vkDestroyCommandPool(logicalDevice, _commandPool, nullptr);
    vkDestroyPipeline(logicalDevice, _graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(logicalDevice, _pipelineLayout, nullptr);
    vkDestroyRenderPass(logicalDevice, _swapChainRenderPass, nullptr);
    //  _initialized = false;
}

void VkApplication::renderPerFrame()
{
    auto logicalDevice = _ctx.getLogicDevice();
    auto graphicsQueue = _ctx.getGraphicsQueue();
    auto presentationQueue = _ctx.getPresentationQueue();
    auto swapChain = _ctx.getSwapChain();

    auto cmdBuffersForRendering = _cmdBuffers[COMMAND_SEMANTIC::RENDERING];
    ASSERT(_currentFrameId >= 0 && _currentFrameId < cmdBuffersForRendering.size(),
           "_currentFrameId must be in the valid range of cmdBuffers");

    _ctx.BeginRecordCommandBuffer(cmdBuffersForRendering[_currentFrameId]);

    const auto cmdToRecord = std::get<1>(cmdBuffersForRendering[_currentFrameId]);
    const auto fenceToWait = std::get<2>(cmdBuffersForRendering[_currentFrameId]);

    // // no timeout set
    // VK_CHECK(vkWaitForFences(logicalDevice, 1, &fenceToWait, VK_TRUE, UINT64_MAX));
    // // vkWaitForFences ensure the previous command is submitted from the host, now it can be modified.
    // VK_CHECK(vkResetCommandBuffer(cmdToRecord, 0));
    // VK_CHECK(vkResetFences(device_, 1, &acquireFence_));
    uint32_t swapChainImageIndex;
    VkResult result = vkAcquireNextImageKHR(
        logicalDevice, swapChain, UINT64_MAX, _imageCanAcquireSemaphores[_currentFrameId],
        VK_NULL_HANDLE, &swapChainImageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        recreateSwapChain();
        return;
    }
    assert(result == VK_SUCCESS ||
           result == VK_SUBOPTIMAL_KHR); // failed to acquire swap chain image
    updateUniformBuffer(_currentFrameId);
    
    recordCommandBuffer(cmdToRecord, swapChainImageIndex);
    _ctx.EndRecordCommandBuffer(cmdBuffersForRendering[_currentFrameId]);

    // submit command
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {_imageCanAcquireSemaphores[_currentFrameId]};
    // pipeline stages
    // specifies the stage of the pipeline after blending where the final color values are output from the pipeline
    // basically wait for the previous rendering finished
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdToRecord;
    // signal semaphore
    VkSemaphore signalRenderedSemaphores[] = {_imageRendereredSemaphores[_currentFrameId]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalRenderedSemaphores;
    
    // vkWaitForFences and reset pattern
    VK_CHECK(vkResetFences(logicalDevice, 1, &fenceToWait));
    // signal fence
    VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, fenceToWait));

    // present after rendering is done
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalRenderedSemaphores;

    VkSwapchainKHR swapChains[] = {swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &swapChainImageIndex;
    presentInfo.pResults = nullptr;
    VK_CHECK(vkQueuePresentKHR(presentationQueue, &presentInfo));

    _currentFrameId = (_currentFrameId + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VkApplication::recreateSwapChain()
{
    const auto logicalDevice = _ctx.getLogicDevice();
    // wait on the host for the completion of outstanding queue operations for all queues
    vkDeviceWaitIdle(logicalDevice);
    deleteSwapChain();
    // createSwapChain();
    // createSwapChainImageViews();
    createSwapChainFramebuffers();
}

void VkApplication::deleteSwapChain()
{
    const auto logicalDevice = _ctx.getLogicDevice();
    for (size_t i = 0; i < _swapChainFramebuffers.size(); ++i)
    {
        vkDestroyFramebuffer(logicalDevice, _swapChainFramebuffers[i], nullptr);
    }
}

// depends on shader, and used by graphicsPipelineDesc
// each set have one instance of layout
void VkApplication::createDescriptorSetLayout()
{
    std::vector<std::vector<VkDescriptorSetLayoutBinding>> setBindings(DESC_LAYOUT_SEMANTIC_SIZE);

    setBindings[DESC_LAYOUT_SEMANTIC::UBO].resize(1);
    setBindings[DESC_LAYOUT_SEMANTIC::UBO][0].binding = 0; // depends on the shader: set 0, binding = 0
    setBindings[DESC_LAYOUT_SEMANTIC::UBO][0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    setBindings[DESC_LAYOUT_SEMANTIC::UBO][0].descriptorCount = 1;
    setBindings[DESC_LAYOUT_SEMANTIC::UBO][0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    setBindings[DESC_LAYOUT_SEMANTIC::COMBO_VERT].resize(1);
    setBindings[DESC_LAYOUT_SEMANTIC::COMBO_VERT][0].binding = 0; // depends on the shader: set 0, binding = 0
    setBindings[DESC_LAYOUT_SEMANTIC::COMBO_VERT][0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    setBindings[DESC_LAYOUT_SEMANTIC::COMBO_VERT][0].descriptorCount = 1;
    setBindings[DESC_LAYOUT_SEMANTIC::COMBO_VERT][0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    setBindings[DESC_LAYOUT_SEMANTIC::COMBO_IDR].resize(1);
    setBindings[DESC_LAYOUT_SEMANTIC::COMBO_IDR][0].binding = 0; // depends on the shader: set 0, binding = 0
    setBindings[DESC_LAYOUT_SEMANTIC::COMBO_IDR][0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    setBindings[DESC_LAYOUT_SEMANTIC::COMBO_IDR][0].descriptorCount = 1;
    setBindings[DESC_LAYOUT_SEMANTIC::COMBO_IDR][0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    setBindings[DESC_LAYOUT_SEMANTIC::TEX_SAMP].resize(2);
    setBindings[DESC_LAYOUT_SEMANTIC::TEX_SAMP][0].binding = 0; // depends on the shader: set 0, binding = 0
    setBindings[DESC_LAYOUT_SEMANTIC::TEX_SAMP][0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    setBindings[DESC_LAYOUT_SEMANTIC::TEX_SAMP][0].descriptorCount = _glbImageEntities.size();
    setBindings[DESC_LAYOUT_SEMANTIC::TEX_SAMP][0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    setBindings[DESC_LAYOUT_SEMANTIC::TEX_SAMP][1].binding = 1; // depends on the shader: set 0, binding = 0
    setBindings[DESC_LAYOUT_SEMANTIC::TEX_SAMP][1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    setBindings[DESC_LAYOUT_SEMANTIC::TEX_SAMP][1].descriptorCount = 1;
    setBindings[DESC_LAYOUT_SEMANTIC::TEX_SAMP][1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    setBindings[DESC_LAYOUT_SEMANTIC::COMBO_MAT].resize(1);
    setBindings[DESC_LAYOUT_SEMANTIC::COMBO_MAT][0].binding = 0; // depends on the shader: set 0, binding = 0
    setBindings[DESC_LAYOUT_SEMANTIC::COMBO_MAT][0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    setBindings[DESC_LAYOUT_SEMANTIC::COMBO_MAT][0].descriptorCount = 1;
    setBindings[DESC_LAYOUT_SEMANTIC::COMBO_MAT][0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    _descriptorSetLayouts = _ctx.createDescriptorSetLayout(setBindings);

    //     const auto logicalDevice = _ctx.getLogicDevice();
    //     // be careful of the MAX_FLIGHT
    //     // set0: one ubo in vs: layout (set = 0, binding = 0) uniform UBO (Yes, has MAX_FLIGHT)
    //     // set1: one sampler2D in fs: layout (set = 1, binding = 0) uniform sampler2D samplerColor;
    //     // set2: glb packed buffer: layout(set = 1, binding = 0) readonly buffer VertexBuffer
    //     // set6: glb material combo buffer

    //     // Descriptor binding flag VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT:
    //     // This flag indicates that descriptor set does not need to have valid descriptors in them
    //     // as long as the invalid descriptors are not accessed during shader execution.

    //     constexpr VkDescriptorBindingFlags flagsToEnable = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
    //                                                        VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT |
    //                                                        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    //     {
    //         // for set0: ubo with MAX_FLIGHTS
    //         std::vector<VkDescriptorSetLayoutBinding> dsLayoutBindings(1);
    //         dsLayoutBindings[0].binding = 0; // depends on the shader: set 0, binding = 0
    //         dsLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    //         // array resource
    //         dsLayoutBindings[0].descriptorCount = 1;
    //         dsLayoutBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    //         dsLayoutBindings[0].pImmutableSamplers = nullptr;

    //         std::vector<VkDescriptorBindingFlags> bindFlags(dsLayoutBindings.size(), flagsToEnable);
    //         const VkDescriptorSetLayoutBindingFlagsCreateInfo extendedInfo{
    //             .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
    //             .pNext = nullptr,
    //             .bindingCount = static_cast<uint32_t>(dsLayoutBindings.size()),
    //             .pBindingFlags = bindFlags.data(),
    //         };
    //         VkDescriptorSetLayoutCreateInfo layoutInfo{};
    //         layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    //         layoutInfo.bindingCount = dsLayoutBindings.size();
    //         layoutInfo.pBindings = dsLayoutBindings.data();
    // #if defined(_WIN32)
    //         layoutInfo.pNext = &extendedInfo;
    //         layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;
    // #endif
    //         VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
    //         VK_CHECK(vkCreateDescriptorSetLayout(logicalDevice, &layoutInfo, nullptr,
    //                                              &descriptorSetLayout));

    //         _descriptorSetLayouts.push_back(descriptorSetLayout);
    //         _descriptorSetLayoutForUbo = descriptorSetLayout;
    //     }

    //     {
    //         std::vector<VkDescriptorSetLayoutBinding> dsLayoutBindings(1);
    //         dsLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    //         dsLayoutBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    //         dsLayoutBindings[0].binding = 0;
    //         dsLayoutBindings[0].descriptorCount = 1;

    //         std::vector<VkDescriptorBindingFlags> bindFlags(dsLayoutBindings.size(), flagsToEnable);
    //         const VkDescriptorSetLayoutBindingFlagsCreateInfo extendedInfo{
    //             .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
    //             .pNext = nullptr,
    //             .bindingCount = static_cast<uint32_t>(dsLayoutBindings.size()),
    //             .pBindingFlags = bindFlags.data(),
    //         };

    //         VkDescriptorSetLayoutCreateInfo layoutInfo{};
    //         layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    //         layoutInfo.bindingCount = dsLayoutBindings.size();
    //         layoutInfo.pBindings = dsLayoutBindings.data();
    // #if defined(_WIN32)
    //         layoutInfo.pNext = &extendedInfo;
    //         layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;
    // #endif

    //         VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
    //         VK_CHECK(vkCreateDescriptorSetLayout(logicalDevice, &layoutInfo, nullptr,
    //                                              &descriptorSetLayout));

    //         _descriptorSetLayouts.push_back(descriptorSetLayout);
    //         _descriptorSetLayoutForComboVertexBuffer = descriptorSetLayout;
    //     }

    //     {
    //         // set3 ssbo: for glb indirectDrawBuffer
    //         VkDescriptorSetLayoutBinding dsLayoutBindings{};
    //         dsLayoutBindings.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    //         dsLayoutBindings.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    //         dsLayoutBindings.binding = 0;
    //         dsLayoutBindings.descriptorCount = 1;

    //         const VkDescriptorSetLayoutBindingFlagsCreateInfo extendedInfo{
    //             .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
    //             .pNext = nullptr,
    //             .bindingCount = 1,
    //             .pBindingFlags = &flagsToEnable,
    //         };

    //         VkDescriptorSetLayoutCreateInfo layoutInfo{};
    //         layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    //         layoutInfo.bindingCount = 1;
    //         layoutInfo.pBindings = &dsLayoutBindings;
    // #if defined(_WIN32)
    //         layoutInfo.pNext = &extendedInfo;
    //         layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;
    // #endif

    //         VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
    //         VK_CHECK(vkCreateDescriptorSetLayout(logicalDevice, &layoutInfo, nullptr,
    //                                              &descriptorSetLayout));
    //         _descriptorSetLayouts.push_back(descriptorSetLayout);
    //         _descriptorSetLayoutForIndirectDrawBuffer = descriptorSetLayout;
    //     }

    //     {
    //         // set 5 bindless Textures and sampler for glb
    //         // VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER vs VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
    //         std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
    //         {
    //             VkDescriptorSetLayoutBinding dsLayoutBinding{};
    //             dsLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    //             dsLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    //             dsLayoutBinding.binding = 0;
    //             dsLayoutBinding.descriptorCount = _glbImageEntities.size();
    //             setLayoutBindings.emplace_back(dsLayoutBinding);
    //         }
    //         {
    //             VkDescriptorSetLayoutBinding dsLayoutBinding{};
    //             dsLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    //             dsLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    //             dsLayoutBinding.binding = 1;
    //             dsLayoutBinding.descriptorCount = 1;
    //             setLayoutBindings.emplace_back(dsLayoutBinding);
    //         }

    //         std::vector<VkDescriptorBindingFlags> bindFlags(setLayoutBindings.size(), flagsToEnable);
    //         const VkDescriptorSetLayoutBindingFlagsCreateInfo extendedInfo{
    //             .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
    //             .pNext = nullptr,
    //             .bindingCount = static_cast<uint32_t>(bindFlags.size()),
    //             .pBindingFlags = bindFlags.data(),
    //         };

    //         VkDescriptorSetLayoutCreateInfo layoutInfo{};
    //         layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    //         layoutInfo.bindingCount = 2;
    //         layoutInfo.pBindings = setLayoutBindings.data();
    // #if defined(_WIN32)
    //         layoutInfo.pNext = &extendedInfo;
    //         layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;
    // #endif

    //         VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
    //         VK_CHECK(vkCreateDescriptorSetLayout(logicalDevice, &layoutInfo, nullptr,
    //                                              &descriptorSetLayout));
    //         _descriptorSetLayouts.push_back(descriptorSetLayout);
    //         _descriptorSetLayoutForTextureAndSampler = descriptorSetLayout;
    //     }

    //     {
    //         // set5 ssbo: for glb material
    //         VkDescriptorSetLayoutBinding dsLayoutBindings{};
    //         dsLayoutBindings.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    //         dsLayoutBindings.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    //         dsLayoutBindings.binding = 0;
    //         dsLayoutBindings.descriptorCount = 1;

    //         const VkDescriptorSetLayoutBindingFlagsCreateInfo extendedInfo{
    //             .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
    //             .pNext = nullptr,
    //             .bindingCount = 1,
    //             .pBindingFlags = &flagsToEnable,
    //         };

    //         VkDescriptorSetLayoutCreateInfo layoutInfo{};
    //         layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    //         layoutInfo.bindingCount = 1;
    //         layoutInfo.pBindings = &dsLayoutBindings;
    // #if defined(_WIN32)
    //         layoutInfo.pNext = &extendedInfo;
    //         layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;
    // #endif

    //         VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
    //         VK_CHECK(vkCreateDescriptorSetLayout(logicalDevice, &layoutInfo, nullptr,
    //                                              &descriptorSetLayout));
    //         _descriptorSetLayouts.push_back(descriptorSetLayout);
    //         _descriptorSetLayoutForComboMaterialBuffer = descriptorSetLayout;
    //     }
}

// depends on your glsl
void VkApplication::createDescriptorPool()
{
    _descriptorSetPool = _ctx.createDescriptorSetPool({{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT},
                                                       {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10},
                                                       {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 10},
                                                       {VK_DESCRIPTOR_TYPE_SAMPLER, 10}},
                                                      100);
    // const auto logicalDevice = _ctx.getLogicDevice();
    // // here I need 4 type of descriptors
    // // layout (set = 0, binding = 0) uniform UBO
    // // layout (set = 1, binding = 0) readonly buffer VertexBuffer;
    // // layout (set = 2, binding = 0) readonly buffer IndirectDraw;
    // // layout(set = 3, binding = 0) uniform texture2D BindlessImage2D[];
    // // layout(set = 3, binding = 1) uniform sampler BindlessSampler[];
    // // layout (set = 4, binding = 0) readonly buffer MaterialBuffer;
}

void VkApplication::allocateDescriptorSets()
{
    _descriptorSets = _ctx.allocateDescriptorSet(_descriptorSetPool,
                                                 {{&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::UBO],
                                                   MAX_FRAMES_IN_FLIGHT},
                                                  {&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::COMBO_VERT],
                                                   1},

                                                  {&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::COMBO_IDR],
                                                   1},

                                                  {&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::TEX_SAMP],
                                                   1},
                                                  {&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::COMBO_MAT],
                                                   1}});
}

void VkApplication::createUniformBuffers()
{
    VkDeviceSize bufferSize = sizeof(UniformDataDef1);
    _uniformBuffers.reserve(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        _uniformBuffers.emplace_back(_ctx.createPersistentBuffer(
            "Uniform buffer " + std::to_string(i),
            bufferSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));
    }
}

void VkApplication::updateUniformBuffer(int currentFrameId)
{
    ASSERT(currentFrameId >= 0 && currentFrameId < _uniformBuffers.size(), "currentFrameId must be within the range");
    auto logicalDevice = _ctx.getLogicDevice();
    auto vmaAllocator = _ctx.getVmaAllocator();
    auto swapChainExtent = _ctx.getSwapChainExtent();

    auto vmaAllocation = std::get<1>(_uniformBuffers[currentFrameId]);

    auto view = _camera.viewTransformLH();
    auto persPrj = PerspectiveProjectionTransformLH(0.0001f, 200.0f, 0.3f,
                                                    (float)swapChainExtent.width /
                                                        (float)swapChainExtent.height);

    mat4x4f identity(1.0f);
    auto mv = MatrixMultiply4x4(identity, view);
    auto vp = MatrixMultiply4x4(view, persPrj);
    auto mvp = MatrixMultiply4x4(identity, vp);

    UniformDataDef1 ubo;
    ubo.viewPos = _camera.viewPos();
    ubo.modelView = mv;
    ubo.projection = persPrj;
    ubo.mvp = mvp;

    void *mappedMemory{nullptr};
    VK_CHECK(vmaMapMemory(vmaAllocator, vmaAllocation, &mappedMemory));
    memcpy(mappedMemory, &ubo, sizeof(UniformDataDef1));
    // memcpy(mappedMemory, &ubo, sizeof(ubo));
    vmaUnmapMemory(vmaAllocator, vmaAllocation);
}

void VkApplication::bindResourceToDescriptorSets()
{
    auto logicalDevice = _ctx.getLogicDevice();

    // for ubo
    ASSERT(_descriptorSets[&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::UBO]].size() == MAX_FRAMES_IN_FLIGHT, "ubo descriptor set has frame_in_flight");
    // extra:
    // 1. ssbo for vb
    // 2. ssbo for indirectdraw
    // 3. textures + samplers
    // 4. ssbo for materials
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        _ctx.bindBufferToDescriptorSet(
            std::get<0>(_uniformBuffers[i]),
            0,
            sizeof(UniformDataDef1),
            _descriptorSets[&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::UBO]][i],
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            0);
    }

    // for glb's vb
    {
        const auto dstSets = _descriptorSets[&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::COMBO_VERT]];
        ASSERT(dstSets.size() == 1, "COMBO_VERT descriptor set size is 1");

        _ctx.bindBufferToDescriptorSet(
            std::get<0>(_compositeVB),
            0,
            _compositeVBSizeInByte,
            dstSets[0],
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            0);
    }

    // glb indirect draw
    {
        const auto dstSets = _descriptorSets[&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::COMBO_IDR]];
        ASSERT(dstSets.size() == 1, "COMBO_IDR descriptor set size is 1");

        _ctx.bindBufferToDescriptorSet(
            std::get<0>(_indirectDrawB),
            0,
            _indirectDrawBSizeInByte,
            dstSets[0],
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            0);
    }

    {
        // for glb textures and samplers
        const auto dstSets = _descriptorSets[&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::TEX_SAMP]];
        ASSERT(dstSets.size() == 1, "TEX_SAMP descriptor set size is 1");

        std::vector<VkImageView> imageViews;
        std::transform(_glbImageEntities.begin(), _glbImageEntities.end(),
                       std::back_inserter(imageViews),
                       [](const auto &imageEntity)
                       {
                           return std::get<1>(imageEntity);
                       });

        _ctx.bindTextureToDescriptorSet(
            imageViews,
            dstSets[0],
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            0);

        // for glb samplers
        std::vector<VkSampler> imageSamplers;
        std::transform(_glbSamplerEntities.begin(), _glbSamplerEntities.end(),
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

    {
        // for mat
        const auto dstSets = _descriptorSets[&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::COMBO_MAT]];
        ASSERT(dstSets.size() == 1, "COMBO_MAT descriptor set size is 1");

        _ctx.bindBufferToDescriptorSet(
            std::get<0>(_compositeMatB),
            0,
            _compositeMatBSizeInByte,
            dstSets[0],
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            0);
    }
    // // Validation Error: [ VUID-VkWriteDescriptorSet-descriptorType-00325 ] Object 0: handle = 0xd10d270000000018, type = VK_OBJECT_TYPE_DESCRIPTOR_SET; Object 1: handle = 0x7fc177270ab3, type = VK_OBJECT_TYPE_SAMPLER; | MessageID = 0xce76343a | vkUpdateDescriptorSets(): pDescriptorWrites[7] Attempted write update to sampler descriptor with invalid sample (VkSampler 0x7fc177270ab3[]).
    // //  The Vulkan spec states: If descriptorType is VK_DESCRIPTOR_TYPE_SAMPLER or VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    // //  and dstSet was not allocated with a layout that included immutable samplers for dstBinding with descriptorType, the sampler member of each element of pImageInfo must be a valid VkSampler object (https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/vkspec.html#VUID-VkWriteDescriptorSet-descriptorType-00325)
    // vkUpdateDescriptorSets(logicalDevice, _writeDescriptorSetBundle.size(),
    //                        _writeDescriptorSetBundle.data(), 0,
    //                        nullptr);
}

// // load shader spirv
// std::vector<uint8_t> LoadBinaryFile(const char *file_path,
//                                     AAssetManager *assetManager)
// {
//     std::vector<uint8_t> file_content;
//     assert(assetManager);
//     AAsset *file =
//         AAssetManager_open(assetManager, file_path, AASSET_MODE_BUFFER);
//     size_t file_length = AAsset_getLength(file);

//     file_content.resize(file_length);

//     AAsset_read(file, file_content.data(), file_length);
//     AAsset_close(file);
//     return file_content;
// }

// VkShaderModule createShaderModule(VkDevice logicalDevice, const std::vector<uint8_t> &code)
// {
//     VkShaderModuleCreateInfo createInfo{};
//     createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
//     createInfo.codeSize = code.size();
//     createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());
//     VkShaderModule shaderModule;
//     VK_CHECK(vkCreateShaderModule(logicalDevice, &createInfo, nullptr, &shaderModule));
//     return shaderModule;
// }

void VkApplication::createShaderModules()
{
    auto logicalDevice = _ctx.getLogicDevice();
    // lateral for filepath in modern c++
    const auto shadersPath = getAssetPath();
    const auto vertexShaderPath = shadersPath + "/indirectDraw.vert";
    const auto fragShaderPath = shadersPath + "/indirectDraw.frag";
    _vsShaderModule = createShaderModule(
        logicalDevice,
        vertexShaderPath,
        "main",
        "indirectDraw.vert");
    _fsShaderModule = createShaderModule(
        logicalDevice,
        fragShaderPath,
        "main",
        "indirectDraw.frag");
}

void VkApplication::createGraphicsPipeline()
{
    auto logicalDevice = _ctx.getLogicDevice();

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = _vsShaderModule;
    vertShaderStageInfo.pName = "main";
    vertShaderStageInfo.pSpecializationInfo = nullptr;

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = _fsShaderModule;
    fragShaderStageInfo.pName = "main";
    fragShaderStageInfo.pSpecializationInfo = nullptr;

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // vao in opengl, this settings means no vao (vbo), create data in the vs directly

    // without vao rendering. ex: ssbo + vs.
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr;

    // match location with shader
    //    layout (location = 0) in vec3 inPos;
    //    layout (location = 1) in vec2 inUV;
    //    layout (location = 2) in vec3 inNormal;

    VkPipelineVertexInputStateCreateInfo vao{};
    std::vector<VkVertexInputBindingDescription> vertexInputBindings(1);
    vertexInputBindings[0].binding = 0;
    vertexInputBindings[0].stride = sizeof(VertexDef1);
    vertexInputBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    std::vector<VkVertexInputAttributeDescription> vertexInputAttributes(3);

    // pos
    vertexInputAttributes[0].location = 0;
    vertexInputAttributes[0].binding = 0;
    vertexInputAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT; // std::array<float, 3> pos
    vertexInputAttributes[0].offset = 0;
    // uv
    vertexInputAttributes[1].location = 1;
    vertexInputAttributes[1].binding = 0;
    vertexInputAttributes[1].format = VK_FORMAT_R32G32_SFLOAT; // std::array<float, 2> uv;
    vertexInputAttributes[1].offset = 3 * sizeof(float);
    // normal
    vertexInputAttributes[2].location = 2;
    vertexInputAttributes[2].binding = 0;
    vertexInputAttributes[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexInputAttributes[2].offset = 5 * sizeof(float);

    vao.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vao.vertexBindingDescriptionCount = vertexInputBindings.size();
    vao.pVertexBindingDescriptions = vertexInputBindings.data();
    vao.vertexAttributeDescriptionCount = vertexInputAttributes.size();
    vao.pVertexAttributeDescriptions = vertexInputAttributes.data();

    // for indirect-draw
    vao.vertexBindingDescriptionCount = vao.vertexAttributeDescriptionCount = 0;

    // topology of input data
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    // Pipeline Dynamic State for viewport and scissor, not setting it here
    // no hardcode the viewport/scissor options,
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f; // Optional
    rasterizer.depthBiasClamp = 0.0f;          // Optional
    rasterizer.depthBiasSlopeFactor = 0.0f;    // Optional
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    // disable alpha blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1; // for MRT
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    // only one set layout
    //    // example of two setlayout will be:
    //    layout(set = 0, binding = 0) uniform Transforms
    //    layout(set = 1, binding = 0) uniform ObjectProperties

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    // multiple set layouts binded to the graphics pipeline
    pipelineLayoutInfo.setLayoutCount = (uint32_t)_descriptorSetLayouts.size();
    pipelineLayoutInfo.pSetLayouts = _descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    VK_CHECK(vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr,
                                    &_pipelineLayout));

    std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT,
                                                       VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicStateCI{};
    dynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateCI.pDynamicStates = dynamicStateEnables.data();
    dynamicStateCI.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    // with vao
    // pipelineInfo.pVertexInputState = &vao;
    // without vao
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = VK_NULL_HANDLE; // Optional
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicStateCI;
    pipelineInfo.layout = _pipelineLayout;
    pipelineInfo.renderPass = _swapChainRenderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex = -1;              // Optional

    VK_CHECK(vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo,
                                       nullptr, &_graphicsPipeline));
    // vkDestroyShaderModule(_logicalDevice, fragShaderModule, nullptr);
    // vkDestroyShaderModule(_logicalDevice, vertShaderModule, nullptr);
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

    // auto logicalDevice = _ctx.getLogicDevice();

    // _swapChainFramebuffers.resize(_swapChainImageViews.size());
    // VkFramebufferCreateInfo framebufferInfo{};
    // framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    // framebufferInfo.renderPass = _swapChainRenderPass;
    // framebufferInfo.attachmentCount = 1;
    // framebufferInfo.width = _swapChainExtent.width;
    // framebufferInfo.height = _swapChainExtent.height;
    // framebufferInfo.layers = 1;
    // for (size_t i = 0; i < _swapChainImageViews.size(); i++)
    // {
    //     VkImageView attachments[] = {_swapChainImageViews[i]};
    //     framebufferInfo.pAttachments = attachments;
    //     VK_CHECK(vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr,
    //                                  &_swapChainFramebuffers[i]));
    // }
}

void VkApplication::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t swapChainImageIndex)
{
    auto swapChainExtent = _ctx.getSwapChainExtent();

    // VkCommandBufferBeginInfo beginInfo{};
    // beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    // //  command buffer will be reset and recorded again between each submissio
    // beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    // beginInfo.pInheritanceInfo = nullptr;

    // VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
    

    // Begin Render Pass, only 1 render pass
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
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline);
    // resource and ds to the shaders of this pipeline
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipelineLayout, 0, 1,
                            &_descriptorSets[&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::UBO]][_currentFrameId],
                            0,
                            nullptr);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipelineLayout, 1, 1,
                            &_descriptorSets[&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::COMBO_VERT]][0],
                            0,
                            nullptr);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipelineLayout, 2, 1,
                            &_descriptorSets[&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::COMBO_IDR]][0],
                            0,
                            nullptr);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipelineLayout, 3, 1,
                            &_descriptorSets[&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::TEX_SAMP]][0],
                            0, nullptr);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipelineLayout, 4, 1,
                            &_descriptorSets[&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::COMBO_MAT]][0],
                            0, nullptr);
    vkCmdBindIndexBuffer(commandBuffer, std::get<0>(_compositeIB), 0, VK_INDEX_TYPE_UINT32);
    // how many draws are dependent on how many meshes in the scene.
    vkCmdDrawIndexedIndirect(commandBuffer, std::get<0>(_indirectDrawB), 0, _numMeshes,
                             sizeof(IndirectDrawForVulkan));
    vkCmdEndRenderPass(commandBuffer);
    // VK_CHECK(vkEndCommandBuffer(commandBuffer));
}

void VkApplication::createPerFrameSyncObjects()
{
    auto logicalDevice = _ctx.getLogicDevice();

    _imageCanAcquireSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    _imageRendereredSemaphores.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        VK_CHECK(vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr,
                                   &_imageCanAcquireSemaphores[i]));
        VK_CHECK(vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr,
                                   &_imageRendereredSemaphores[i]));
    }
}

// create shared _uploadCmd and begin
void VkApplication::preHostDeviceIO()
{
    auto cmdBuffersForIO = _cmdBuffers[COMMAND_SEMANTIC::IO];
    ASSERT(cmdBuffersForIO.size() == 1, "Only use 1 cmdBuffer for IO")

    auto logicalDevice = _ctx.getLogicDevice();
    const auto uploadCmdBuffer = std::get<1>(cmdBuffersForIO[0]);

    _ctx.BeginRecordCommandBuffer(cmdBuffersForIO[0]);
}

// end recording of buffer.
// wait for completion using fence
void VkApplication::postHostDeviceIO()
{
    auto logicalDevice = _ctx.getLogicDevice();
    auto graphicsQueue = _ctx.getGraphicsQueue();
    auto vmaAllocator = _ctx.getVmaAllocator();

    auto cmdBuffersForIO = _cmdBuffers[COMMAND_SEMANTIC::IO];
    ASSERT(cmdBuffersForIO.size() == 1, "Only use 1 cmdBuffer for IO");
    const auto uploadCmdBuffer = std::get<1>(cmdBuffersForIO[0]);
    const auto uploadCmdBufferFence = std::get<2>(cmdBuffersForIO[0]);

    _ctx.EndRecordCommandBuffer(cmdBuffersForIO[0]);

    const VkPipelineStageFlags flags = VK_PIPELINE_STAGE_TRANSFER_BIT;

    VkSubmitInfo submitInfo{};
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.pWaitSemaphores = VK_NULL_HANDLE;
    // only useful when having waitsemaphore
    submitInfo.pWaitDstStageMask = &flags;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &uploadCmdBuffer;
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores = VK_NULL_HANDLE;

    // must resetfence of waiting
    VK_CHECK(vkResetFences(logicalDevice, 1, &uploadCmdBufferFence));
    VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, uploadCmdBufferFence));

    const auto result = vkWaitForFences(logicalDevice, 1, &uploadCmdBufferFence, VK_TRUE,
                                        DEFAULT_FENCE_TIMEOUT);
    if (result == VK_TIMEOUT)
    {
        vkDeviceWaitIdle(logicalDevice);
    }
    // clean all the staging resources
    if (_stagingVb != VK_NULL_HANDLE)
        vkDestroyBuffer(logicalDevice, _stagingVb, nullptr);
    if (_stagingIb != VK_NULL_HANDLE)
        vkDestroyBuffer(logicalDevice, _stagingIb, nullptr);

    // for texture
    if (_stagingImageBuffer != VK_NULL_HANDLE)
        vkDestroyBuffer(logicalDevice, _stagingImageBuffer, nullptr);

    // for glb Scene
    for (size_t i = 0; i < _stagingVbForMesh.size(); ++i)
    {
        vmaDestroyBuffer(vmaAllocator, std::get<0>(_stagingVbForMesh[i]), std::get<1>(_stagingVbForMesh[i]));
    }
    for (size_t i = 0; i < _stagingIbForMesh.size(); ++i)
    {
        vmaDestroyBuffer(vmaAllocator, std::get<0>(_stagingIbForMesh[i]), std::get<1>(_stagingIbForMesh[i]));
    }
    // for indirectDrawBuffer
    {
        vmaDestroyBuffer(vmaAllocator, std::get<0>(_stagingIndirectDrawBuffer), std::get<1>(_stagingIndirectDrawBuffer));
    }
    // for material buffer
    {
        vmaDestroyBuffer(vmaAllocator, std::get<0>(_stagingMatBuffer), std::get<1>(_stagingMatBuffer));
    }
    // for textures in glb
    for (size_t i = 0; i < _glbImageStagingBuffers.size(); ++i)
    {
        vmaDestroyBuffer(vmaAllocator, std::get<0>(_glbImageStagingBuffers[i]), std::get<1>(_glbImageStagingBuffers[i]));
    }
}

// // cull face be careful
// // Interleaved vertex attributes
// void VkApplication::loadVao()
// {
//     std::vector<VertexDef1> vertices = {
//         {{1.0f, -1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
//         {{1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
//         {{-1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
//         {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
//     };

//     std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};

//     _indexCount = indices.size();

//     // vao
//     // staging buffer:
//     // 1. usage: VK_BUFFER_USAGE_TRANSFER_SRC_BIT
//     // 2. VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
//     const auto vbByteSize = vertices.size() * sizeof(VertexDef1);
//     const auto ebByteSize = indices.size() * sizeof(uint32_t);
//     {
//         // create _stagingVb
//         VmaAllocation vmaStagingBufferAllocation{nullptr};
//         VkBufferCreateInfo bufferCreateInfo{
//             .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
//             .size = vbByteSize,
//             .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//             .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
//         };
//         const VmaAllocationCreateInfo stagingAllocationCreateInfo = {
//             .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
//                      VMA_ALLOCATION_CREATE_MAPPED_BIT,
//             .usage = VMA_MEMORY_USAGE_CPU_ONLY,
//             .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
//                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
//         VK_CHECK(vmaCreateBuffer(_vmaAllocator, &bufferCreateInfo, &stagingAllocationCreateInfo,
//                                  &_stagingVb,
//                                  &vmaStagingBufferAllocation, nullptr));
//         void *mappedMemory{nullptr};
//         VK_CHECK(vmaMapMemory(_vmaAllocator, vmaStagingBufferAllocation, &mappedMemory));
//         memcpy(mappedMemory, vertices.data(), vbByteSize);
//         vmaUnmapMemory(_vmaAllocator, vmaStagingBufferAllocation);
//     }

//     {
//         // create stagingEb
//         VmaAllocation vmaStagingBufferAllocation{nullptr};
//         VkBufferCreateInfo bufferCreateInfo{
//             .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
//             .size = ebByteSize,
//             .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//             .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
//         };
//         const VmaAllocationCreateInfo stagingAllocationCreateInfo = {
//             .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
//                      VMA_ALLOCATION_CREATE_MAPPED_BIT,
//             .usage = VMA_MEMORY_USAGE_CPU_ONLY,
//             .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
//                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
//         VK_CHECK(vmaCreateBuffer(_vmaAllocator, &bufferCreateInfo, &stagingAllocationCreateInfo,
//                                  &_stagingIb,
//                                  &vmaStagingBufferAllocation, nullptr));
//         void *mappedMemory{nullptr};
//         VK_CHECK(vmaMapMemory(_vmaAllocator, vmaStagingBufferAllocation, &mappedMemory));
//         memcpy(mappedMemory, indices.data(), ebByteSize);
//         vmaUnmapMemory(_vmaAllocator, vmaStagingBufferAllocation);
//     }

//     // device buffer:
//     // 1. usage: VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
//     // 2. VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
//     {
//         // create vbo
//         VmaAllocation vmaDeviceBufferAllocation{nullptr};
//         VkBufferCreateInfo bufferCreateInfo{
//             .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
//             .size = vbByteSize,
//             .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
//             .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
//         };
//         const VmaAllocationCreateInfo bufferAllocationCreateInfo = {
//             .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
//                      VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
//             .usage = VMA_MEMORY_USAGE_GPU_ONLY,
//             .preferredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
//                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
//         VK_CHECK(vmaCreateBuffer(_vmaAllocator, &bufferCreateInfo, &bufferAllocationCreateInfo,
//                                  &_deviceVb,
//                                  &vmaDeviceBufferAllocation, nullptr));

//         // src: bytesOffset, dst: bytesOffset
//         VkBufferCopy region{.srcOffset = 0, .dstOffset = 0, .size = vbByteSize};
//         vkCmdCopyBuffer(_uploadCmd, _stagingVb, _deviceVb, 1, &region);
//     }

//     {
//         // create ebo
//         VmaAllocation vmaDeviceBufferAllocation{nullptr};
//         VkBufferCreateInfo bufferCreateInfo{
//             .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
//             .size = ebByteSize,
//             .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
//             .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
//         };
//         const VmaAllocationCreateInfo bufferAllocationCreateInfo = {
//             .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
//                      VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
//             .usage = VMA_MEMORY_USAGE_GPU_ONLY,
//             .preferredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
//                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
//         VK_CHECK(vmaCreateBuffer(_vmaAllocator, &bufferCreateInfo, &bufferAllocationCreateInfo,
//                                  &_deviceIb,
//                                  &vmaDeviceBufferAllocation, nullptr));

//         // src: bytesOffset, dst: bytesOffset
//         VkBufferCopy region{.srcOffset = 0, .dstOffset = 0, .size = ebByteSize};
//         vkCmdCopyBuffer(_uploadCmd, _stagingIb, _deviceIb, 1, &region);
//     }
// }

// void VkApplication::loadTextures()
// {
//     // std::string filename = getAssetPath() + "metalplate01_rgba.ktx";
//     std::string filename = getAssetPath() + "lavaplanet_color_rgba.ktx";
//     VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
//     ktxResult result;
//     ktxTexture *ktxTexture;

// #if defined(__ANDROID__)
//     AAsset *asset = AAssetManager_open(_assetManager, filename.c_str(), AASSET_MODE_STREAMING);
//     if (!asset)
//     {
//         FATAL("Could not load texture from " + filename, -1);
//     }
//     size_t size = AAsset_getLength(asset);
//     ASSERT(size > 0, "asset size should larger then 0");

//     ktx_uint8_t *textureData = new ktx_uint8_t[size];
//     AAsset_read(asset, textureData, size);
//     AAsset_close(asset);
//     result = ktxTexture_CreateFromMemory(textureData, size, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
//                                          &ktxTexture);
//     delete[] textureData;
// #else
//     // To Do
// #endif
//     ASSERT(result == KTX_SUCCESS, "ktxTexture_CreateFromMemory failed");
//     auto textureWidth = ktxTexture->baseWidth;
//     auto textureHeight = ktxTexture->baseHeight;
//     auto textureMipLevels = ktxTexture->numLevels;
//     ktx_uint8_t *ktxTextureData = ktxTexture_GetData(ktxTexture);
//     ktx_size_t ktxTextureSize = ktxTexture_GetSize(ktxTexture);

//     // Linear tiled images
//     // Optimal tiled images: not accessible by the host, requires some sort of data copy,
//     // either from a buffer or	a linear tiled image
// #if defined(LINEAR_TILED_IMAGES)
//     VmaAllocation vmaImageAllocation{nullptr};

//     // linear texture
//     VkImageCreateInfo imageCreateInfo{};
//     imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
//     imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
//     imageCreateInfo.format = format;
//     imageCreateInfo.mipLevels = 1;
//     imageCreateInfo.arrayLayers = 1;
//     imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
//     // linear tiling
//     imageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
//     // if it is optim tiling, usage will be VK_IMAGE_USAGE_TRANSFER_SRC_BIT, a convert is needed
//     // other flag: VK_IMAGE_USAGE_TRANSFER_DST_BIT
//     // VK_IMAGE_USAGE_SAMPLED_BIT: directly used by shader
//     imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
//     imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
//     // for optimal   .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
//     // VK_IMAGE_LAYOUT_PREINITIALIZED is only useful with linear images
//     imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
//     imageCreateInfo.extent = {textureWidth, textureHeight, 1};

//     // create a image, allocate memory for it, and bind them together, all in one call
//     // memoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT: use VMA_MEMORY_USAGE_AUTO_PREFER_HOST
//     // https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/group__group__alloc.html#ggaa5846affa1e9da3800e3e78fae2305cca9b422585242160b8ed3418310ee6664d
//     // VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT is needed for VMA_MEMORY_USAGE_AUTO_PREFER_HOST
//     // to map/unmap
//     const VmaAllocationCreateInfo allocCreateInfo = {
//         .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
//                  VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
//         .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
//         .priority = 1.0f,
//     };

//     VK_CHECK(vmaCreateImage(_vmaAllocator, &imageCreateInfo, &allocCreateInfo, &image,
//                             &vmaImageAllocation, nullptr));

//     if (vmaImageAllocation != nullptr)
//     {
//         VmaAllocationInfo imageAllocationInfo;
//         vmaGetAllocationInfo(_vmaAllocator, vmaImageAllocation, &imageAllocationInfo);

//         //        // Map image memory
//         //        void *data;
//         //        VK_CHECK_RESULT(vkMapMemory(device, mappableMemory, 0, memReqs.size, 0, &data));
//         //        // Copy image data of the first mip level into memory
//         //        memcpy(data, ktxTextureData, memReqs.size);
//         //        vkUnmapMemory(device, mappableMemory);

//         void *mappedMemory{nullptr};
//         VK_CHECK(vmaMapMemory(_vmaAllocator, vmaImageAllocation, &mappedMemory));
//         memcpy(mappedMemory, ktxTextureData, imageAllocationInfo.size);
//         vmaUnmapMemory(_vmaAllocator, vmaImageAllocation);

//         // commandbuffer to submit the texture data
//         // image memory barrier transfer image to shader read layout
//         // transition image layout
//         VkCommandBuffer copyCmd;
//         VkCommandBufferAllocateInfo allocInfo{};
//         allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
//         allocInfo.commandPool = _commandPool;
//         allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
//         allocInfo.commandBufferCount = 1;
//         VK_CHECK(vkAllocateCommandBuffers(_logicalDevice, &allocInfo, &copyCmd));
//         // recording buffer
//         VkCommandBufferBeginInfo cmdBufferBeginInfo{};
//         cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
//         VK_CHECK(vkBeginCommandBuffer(copyCmd, &cmdBufferBeginInfo));

//         VkImageSubresourceRange subresourceRange = {};
//         subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//         subresourceRange.baseMipLevel = 0;
//         subresourceRange.levelCount = 1;
//         subresourceRange.layerCount = 1;

//         VkImageMemoryBarrier imageMemoryBarrier{};
//         imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
//         imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//         imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//         imageMemoryBarrier.image = image;
//         imageMemoryBarrier.subresourceRange = subresourceRange;
//         // from cpu write
//         imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
//         // to gpu read
//         imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
//         // match creation
//         imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
//         imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//         vkCmdPipelineBarrier(
//             copyCmd,
//             VK_PIPELINE_STAGE_HOST_BIT,
//             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
//             0,
//             0, nullptr,
//             0, nullptr,
//             1, &imageMemoryBarrier);
//         VK_CHECK(vkEndCommandBuffer(copyCmd));

//         VkFenceCreateInfo fenceInfo = {
//             .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
//             .pNext = nullptr,
//             .flags = 0,
//         };
//         VkFence fence;
//         VK_CHECK(vkCreateFence(_logicalDevice, &fenceInfo, nullptr, &fence));
//         VkSubmitInfo submitInfo{};
//         submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
//         submitInfo.commandBufferCount = 1;
//         submitInfo.pCommandBuffers = &copyCmd;
//         VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submitInfo, fence));
//         // Wait for the fence to signal that command buffer has finished executing
//         VK_CHECK(vkWaitForFences(_logicalDevice, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));
//         vkDestroyFence(_logicalDevice, fence, nullptr);
//         vkFreeCommandBuffers(_logicalDevice, _commandPool, 1, &copyCmd);
//     }
// #else
//     // This buffer is used as a transfer source for the buffer copy
//     VmaAllocation vmaStagingBufferAllocation{nullptr};
//     VkBufferCreateInfo bufferCreateInfo{
//         .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
//         .size = size,
//         .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT, // for staging buffer
//         .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
//     };
//     const VmaAllocationCreateInfo stagingAllocationCreateInfo = {
//         .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
//                  VMA_ALLOCATION_CREATE_MAPPED_BIT,
//         .usage = VMA_MEMORY_USAGE_CPU_ONLY,
//     };
//     VK_CHECK(vmaCreateBuffer(_vmaAllocator, &bufferCreateInfo, &stagingAllocationCreateInfo,
//                              &_stagingImageBuffer,
//                              &vmaStagingBufferAllocation, nullptr));
//     if (vmaStagingBufferAllocation != nullptr)
//     {
//         VmaAllocationInfo stagingBufferAllocationInfo;
//         vmaGetAllocationInfo(_vmaAllocator, vmaStagingBufferAllocation,
//                              &stagingBufferAllocationInfo);
//         // copy to staging buffer (visible both host and device)
//         void *mappedMemory{nullptr};
//         VK_CHECK(vmaMapMemory(_vmaAllocator, vmaStagingBufferAllocation, &mappedMemory));
//         memcpy(mappedMemory, ktxTextureData, ktxTextureSize);
//         vmaUnmapMemory(_vmaAllocator, vmaStagingBufferAllocation);
//         VK_CHECK(vmaFlushAllocation(_vmaAllocator, vmaStagingBufferAllocation, 0, ktxTextureSize));

//         // for image
//         // diff1: textureMipLevels,
//         // diff2: VK_IMAGE_TILING_OPTIMAL,
//         // diff3: usage has 1 more flag: VK_IMAGE_USAGE_TRANSFER_DST_BIT
//         // diff4: initial layout: VK_IMAGE_LAYOUT_UNDEFINED

//         VkImageCreateInfo imageCreateInfo{};
//         imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
//         imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
//         imageCreateInfo.format = format;
//         imageCreateInfo.mipLevels = textureMipLevels;
//         imageCreateInfo.arrayLayers = 1;
//         imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
//         imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
//         imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
//         imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
//         imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//         imageCreateInfo.extent = {textureWidth, textureHeight, 1};
//         // no need for VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, cpu does not need access
//         const VmaAllocationCreateInfo allocCreateInfo = {
//             .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
//             .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
//             .priority = 1.0f,
//         };
//         VK_CHECK(vmaCreateImage(_vmaAllocator, &imageCreateInfo, &allocCreateInfo, &_image,
//                                 &_vmaImageAllocation, nullptr));

//         std::vector<VkBufferImageCopy> bufferCopyRegions;
//         uint32_t offset = 0;

//         for (uint32_t i = 0; i < textureMipLevels; ++i)
//         {
//             ktx_size_t offsetForMipMapLevel;
//             auto ret = ktxTexture_GetImageOffset(ktxTexture, i, 0, 0, &offsetForMipMapLevel);
//             ASSERT(ret == KTX_SUCCESS, "ktxTexture_GetImageOffset failed");
//             // Setup a buffer image copy structure for the current mip level
//             VkBufferImageCopy bufferCopyRegion = {};
//             // regarding mipmap
//             bufferCopyRegion.bufferOffset = offsetForMipMapLevel;
//             // could be depth, stencil and color
//             bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//             bufferCopyRegion.imageSubresource.mipLevel = i;
//             bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
//             bufferCopyRegion.imageSubresource.layerCount = 1;
//             // primad mipmap hierachy
//             bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth >> i;
//             bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight >> i;
//             bufferCopyRegion.imageExtent.depth = 1;
//             bufferCopyRegions.push_back(bufferCopyRegion);
//         }

//         // now has mipmap
//         // barrier based on mip level, array layers
//         VkImageSubresourceRange subresourceRange = {};
//         subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//         subresourceRange.baseMipLevel = 0;
//         subresourceRange.levelCount = textureMipLevels;
//         subresourceRange.layerCount = 1;

//         // transition layout
//         VkImageMemoryBarrier imageMemoryBarrier{};
//         imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
//         imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//         imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//         imageMemoryBarrier.image = _image;
//         imageMemoryBarrier.subresourceRange = subresourceRange;
//         imageMemoryBarrier.srcAccessMask = 0;
//         imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//         imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//         // VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: written into
//         imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

//         // it defines a memory dependency between commands that were
//         //  submitted to the same queue before it, and those submitted to the same queue after it.
//         //  ensure the image layout is VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, which could be write to.
//         vkCmdPipelineBarrier(
//             _uploadCmd,
//             VK_PIPELINE_STAGE_HOST_BIT,
//             VK_PIPELINE_STAGE_TRANSFER_BIT,
//             0,
//             0, nullptr,
//             0, nullptr,
//             1, &imageMemoryBarrier);
//         // now image layout(usage) is writable
//         // staging buffer to device-local(image is device local memory)
//         vkCmdCopyBufferToImage(
//             _uploadCmd,
//             _stagingImageBuffer,
//             _image,
//             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
//             static_cast<uint32_t>(bufferCopyRegions.size()),
//             bufferCopyRegions.data());

//         // image layout(usage) from dst -> shader read
//         imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//         imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
//         imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
//         imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//         vkCmdPipelineBarrier(
//             _uploadCmd,
//             VK_PIPELINE_STAGE_TRANSFER_BIT,
//             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
//             0,
//             0, nullptr,
//             0, nullptr,
//             1, &imageMemoryBarrier);

//         //        // Done with the staging buffer
//         //        // vmaDestroyBuffer to replace the following
//         //        // Clean up staging resources
//         //        // vkFreeMemory(_logicalDevice, stagingBuffer, nullptr);
//         //        //vmaFreeMemory(_vmaAllocator, vmaImageAllocation);
//         //
//         //        vkDestroyBuffer(_logicalDevice, stagingBuffer, nullptr);
//         //        // cannot free memory bounded to the image.
//         //        // vmaDestroyBuffer(_vmaAllocator, stagingBuffer, vmaImageAllocation);
//     }
// #endif
//     // done with the cpu texture
//     ktxTexture_Destroy(ktxTexture);
//     // image view

//     // inteprete images's size, location and format except layout (image barrier)
//     VkImageViewCreateInfo imageViewInfo = {};
//     imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
//     imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
//     imageViewInfo.format = format;
//     // subresource range could limit miplevel and layer ranges, here all are open to access
//     imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//     imageViewInfo.subresourceRange.baseMipLevel = 0;
//     imageViewInfo.subresourceRange.baseArrayLayer = 0;
//     imageViewInfo.subresourceRange.layerCount = 1;
// #if defined(LINEAR_TILED_IMAGES)
//     imageViewInfo.subresourceRange.levelCount = 1;
// #else
//     imageViewInfo.subresourceRange.levelCount = textureMipLevels;
// #endif
//     imageViewInfo.image = _image;
//     VK_CHECK(vkCreateImageView(_logicalDevice, &imageViewInfo, nullptr, &_imageView));

//     // sampler
//     VkSamplerCreateInfo samplerCreateInfo = {};
//     samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
//     samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
//     samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
//     samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
//     samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
//     samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
//     samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

//     samplerCreateInfo.mipLodBias = 0.0f;
//     samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
//     samplerCreateInfo.minLod = 0.0f;
// #if defined(LINEAR_TILED_IMAGES)
//     samplerCreateInfo.maxLod = 0.0f;
// #else
//     samplerCreateInfo.maxLod = textureMipLevels;
// #endif
//     // Enable anisotropic filtering
//     if (_enabledDeviceFeatures.features.samplerAnisotropy)
//     {
//         // Use max. level of anisotropy for this example
//         samplerCreateInfo.maxAnisotropy = _physicalDevicesProp1.limits.maxSamplerAnisotropy;
//         samplerCreateInfo.anisotropyEnable = VK_TRUE;
//     }
//     else
//     {
//         samplerCreateInfo.maxAnisotropy = 1.0;
//         samplerCreateInfo.anisotropyEnable = VK_FALSE;
//     }
//     samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
//     VK_CHECK(vkCreateSampler(_logicalDevice, &samplerCreateInfo, nullptr, &_sampler));
// }

// // minimize round-trip between cpu-gpu
// void buildCompositeBuffer(const Scene &scene,
//                           std::vector<VkBuffer> &outBuffers,
//                           std::vector<VkSampler> &outSamplers)
// {
//     // Create vbo

//     //
//     //    buffers.emplace_back(context.createBuffer(
//     //            model.totalVertexSize,
//     // #if defined(VK_KHR_buffer_device_address) && defined(_WIN32)
//     //            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
//     // #endif
//     //            VK_BUFFER_USAGE_TRANSFER_DST_BIT | (makeBuffersSuitableForAccelStruct
//     //                                                ? VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
//     //                                                : 0) |
//     //            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
//     //            VMA_MEMORY_USAGE_GPU_ONLY, "vertex"));
// }

void VkApplication::loadGLB()
{
    const auto vk12FeatureCaps = _ctx.getVk12FeatureCaps();
    auto logicalDevice = _ctx.getLogicDevice();
    auto vmaAllocator = _ctx.getVmaAllocator();
    const auto selectedPhysicalDevice = _ctx.getSelectedPhysicalDevice();
    const auto selectedPhysicalDeviceProp = _ctx.getSelectedPhysicalDeviceProp();

    auto cmdBuffersForIO = _cmdBuffers[COMMAND_SEMANTIC::IO];
    const auto uploadCmdBuffer = std::get<1>(cmdBuffersForIO[0]);
    const auto uploadCmdBufferFence = std::get<2>(cmdBuffersForIO[0]);

    std::string filename = getAssetPath() + "\\" + _model;

    std::vector<char> glbContent;

#if defined(__ANDROID__)
    // Load GLB
    AAsset *glbAsset = AAssetManager_open(_assetManager, filename.c_str(), AASSET_MODE_BUFFER);
    size_t glbByteSize = AAsset_getLength(glbAsset);
    glbContent.resize(glbByteSize);
    AAsset_read(glbAsset, glbContent.data(), glbByteSize);
#else
    glbContent = readFile(filename, true);
#endif
    GltfBinaryIOReader reader;
    std::shared_ptr<Scene> scene = reader.read(glbContent);
    _numMeshes = scene->meshes.size();

    // check device feature supported
    if (vk12FeatureCaps.bufferDeviceAddress)
    {
        {
            // ssbo for vertices
            auto bufferByteSize = scene->totalVerticesByteSize;
            _compositeVBSizeInByte = bufferByteSize;
            _compositeVB = _ctx.createDeviceLocalBuffer(
                "Device Vertices Buffer Combo",
                bufferByteSize,
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        }

        {
            // ssbo for ib
            auto bufferByteSize = scene->totalIndexByteSize;
            _compositeIBSizeInByte = bufferByteSize;
            _compositeIB = _ctx.createDeviceLocalBuffer(
                "Device Indices Buffer Combo",
                bufferByteSize,
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                    VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                    VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
        }

        // upload data to buffer
        uint32_t currentVertexStartingIndex = 0u;
        uint32_t currentIndicesStartingIndex = 0u;
        // firstIndex of the composite index buffer for current draw
        uint32_t firstIndex = 0u;
        // offset into composite vertice buffer
        uint32_t vertexOffset = 0u;
        std::vector<IndirectDrawForVulkan> indirectDrawParams;
        indirectDrawParams.reserve(scene->meshes.size());
        uint32_t deviceCompositeVertexBufferOffsetInBytes = 0u;
        uint32_t deviceCompositeIndicesBufferOffsetInBytes = 0u;
        size_t meshId = 0;
        for (const auto &mesh : scene->meshes)
        {
            auto vertexByteSizeMesh = sizeof(Vertex) * mesh.vertices.size();
            auto vertexBufferPtr = reinterpret_cast<const void *>(mesh.vertices.data());

            _stagingVbForMesh.emplace_back(_ctx.createStagingBuffer(
                "Staging Vertices Buffer Mesh " + std::to_string(meshId),
                vertexByteSizeMesh));

            _ctx.writeBuffer(
                _stagingVbForMesh.back(),
                _compositeVB,
                cmdBuffersForIO[0],
                vertexBufferPtr,
                vertexByteSizeMesh,
                0,
                deviceCompositeVertexBufferOffsetInBytes);

            deviceCompositeVertexBufferOffsetInBytes += vertexByteSizeMesh;

            // copy ib from host to device
            auto indicesByteSizeMesh = sizeof(uint32_t) * mesh.indices.size();
            auto indicesBufferPtr = reinterpret_cast<const void *>(mesh.indices.data());

            _stagingIbForMesh.emplace_back(_ctx.createStagingBuffer(
                "Staging Indices Buffer Mesh  " + std::to_string(meshId),
                indicesByteSizeMesh));

            _ctx.writeBuffer(
                _stagingIbForMesh.back(),
                _compositeIB,
                cmdBuffersForIO[0],
                indicesBufferPtr,
                indicesByteSizeMesh,
                0,
                deviceCompositeIndicesBufferOffsetInBytes);

            deviceCompositeIndicesBufferOffsetInBytes += indicesByteSizeMesh;
            // reserve still needs push_back/emplace_back
            indirectDrawParams.emplace_back(IndirectDrawForVulkan{
                .indexCount = uint32_t(mesh.indices.size()),
                .instanceCount = 1,
                .firstIndex = firstIndex,
                .vertexOffset = static_cast<int>(vertexOffset),
                .firstInstance = 0,
                .meshId = static_cast<uint32_t>(meshId),
                .materialIndex = static_cast<uint32_t>(mesh.materialIdx),
            });
            vertexOffset += mesh.vertices.size();
            firstIndex += mesh.indices.size();
            ++meshId;
        }
        // textures
        // 1. create image
        // 2. create image view
        // 3. upload through stage buffer
        size_t textureId = 0;
        for (const auto &texture : scene->textures)
        {
            // 1. create Image
            const auto textureMipLevels = getMipLevelsCount(texture->width,
                                                            texture->height);
            const uint32_t textureLayoutCount = 1;
            const VkExtent3D textureExtent = {static_cast<uint32_t>(texture->width),
                                              static_cast<uint32_t>(texture->height), 1};
            const bool generateMipmaps = true;
            const auto imageEntity = _ctx.createImage("glb_tex_" + std::to_string(textureId),
                                                      VK_IMAGE_TYPE_2D,
                                                      VK_FORMAT_R8G8B8A8_UNORM,
                                                      textureExtent,
                                                      textureMipLevels,
                                                      textureLayoutCount,
                                                      VK_SAMPLE_COUNT_1_BIT,
                                                      // usage here: both dst and src as mipmap generation
                                                      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                                      generateMipmaps);

            _glbImageEntities.emplace_back(imageEntity);

            // write raw data from cpu to the mipmap level 0 of image
            const auto stagingBufferSizeForImage = std::get<3>(imageEntity).size;
            _glbImageStagingBuffers.emplace_back(_ctx.createStagingBuffer(
                "Staging Buffer Texture " + std::to_string(textureId),
                stagingBufferSizeForImage));

            _ctx.writeImage(
                _glbImageEntities.back(),
                _glbImageStagingBuffers.back(),
                cmdBuffersForIO[0],
                texture->data);

            _ctx.generateMipmaps(
                _glbImageEntities.back(),
                _glbImageStagingBuffers.back(),
                cmdBuffersForIO[0]);

            ++textureId;
        }

        // sampler
        _glbSamplerEntities.emplace_back(_ctx.createSampler("sampler0"));

        // packing materials into composite buffer
        const auto materialByteSize = sizeof(Material) * scene->materials.size();
        {
            // create device buffer
            auto bufferByteSize = materialByteSize;
            _compositeMatBSizeInByte = bufferByteSize;
            _compositeMatB = _ctx.createDeviceLocalBuffer(
                "Device Material Buffer Combo",
                bufferByteSize,
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        }
        {
            // create staging buffer
            auto materialBufferPtr = reinterpret_cast<const void *>(scene->materials.data());

            // staging buffer for matBuffer
            _stagingMatBuffer = _ctx.createStagingBuffer("Staging Material Buffer", materialByteSize);
            _ctx.writeBuffer(
                _stagingMatBuffer,
                _compositeMatB,
                cmdBuffersForIO[0],
                materialBufferPtr,
                materialByteSize,
                0,
                0);
        }

        // packing for indirectDrawBuffer
        const auto indirectDrawBufferByteSize = sizeof(IndirectDrawForVulkan) * indirectDrawParams.size();
        {
            // create device buffer for indirectDraw
            auto bufferByteSize = indirectDrawBufferByteSize;
            _indirectDrawBSizeInByte = bufferByteSize;
            _indirectDrawB = _ctx.createDeviceLocalBuffer(
                "Device IndirectDraw Buffer Combo",
                bufferByteSize,
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                    VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
        }
        {
            // create staging buffer
            auto indirectDrawBufferPtr = reinterpret_cast<const void *>(indirectDrawParams.data());
            // staging buffer for indirectDrawBuffer
            _stagingIndirectDrawBuffer = _ctx.createStagingBuffer("Staging Indirect Draw Buffer", indirectDrawBufferByteSize);

            _ctx.writeBuffer(
                _stagingIndirectDrawBuffer,
                _indirectDrawB,
                cmdBuffersForIO[0],
                indirectDrawBufferPtr,
                indirectDrawBufferByteSize,
                0,
                0);
        }
    }
}