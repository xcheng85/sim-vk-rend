#pragma once

#if defined(__ANDROID__)
#include <android/asset_manager.h>
#include <android/log.h>
// os window, glfw, sdi, ...
#include <android/native_window.h>
#include <android/native_window_jni.h>
// To use volk, you have to include volk.h instead of vulkan/vulkan.h;
// this is necessary to use function definitions from volk.
#include <vulkan/vulkan.h>
#endif

#include <array>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <numeric>
#include <assert.h>
#include <iostream>
#include <format>
#include <vector>
#include <algorithm>
#include <iterator>
#include <numeric>
#include <array>
#include <filesystem> // for shader

// must ahead of <vk_mem_alloc.h>, or else it will crash on vk functions
#ifndef __ANDROID__
#define VK_NO_PROTOTYPES // for volk
#include "volk.h"
#endif
// To do it properly:
//
// Include "vk_mem_alloc.h" file in each CPP file where you want to use the library. This includes declarations of all members of the library.
// In exactly one CPP file define following macro before this include. It enables also internal definitions.
#include <vk_mem_alloc.h>

#include <misc.h>
#include <context.h>

#if defined(__ANDROID__)
// functor for custom deleter for unique_ptr
struct AndroidNativeWindowDeleter
{
    void operator()(ANativeWindow *window) { ANativeWindow_release(window); }
};
#endif

#ifdef _WIN32
#if !defined(VK_USE_PLATFORM_WIN32_KHR)
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#endif

#ifdef __linux__
#if !defined(VK_USE_PLATFORM_XLIB_KHR)
#define VK_USE_PLATFORM_XLIB_KHR
#endif
#endif

class Window;
class CameraBase;
class VkContext;

class VkApplication
{
public:
    VkApplication() = delete;
    VkApplication(VkContext &ctx)
        : _ctx(ctx)
    {
    }
    void init();
    void teardown();
    // render loop will call this per-frame
    void renderPerFrame();

private:
    void createSwapChainRenderPass();
    // when resize occurs;
    void recreateSwapChain();
    // when resize and app tear down
    void deleteSwapChain();
    // specify sets and types of bindings in a set
    void createDescriptorSetLayout();
    // to allocate set (match glsl)
    void createDescriptorPool();
    // allocate ds from the pool
    void allocateDescriptorSets();
    void createUniformBuffers();
    // called inside renderPerFrame(); some shader data is updated per-frame
    void updateUniformBuffer(int currentFrameId);
    // bind resource to ds
    void bindResourceToDescriptorSets();
    void createShaderModules();
    void createGraphicsPipeline();
    void createSwapChainFramebuffers();
    void createCommandPool();
    void createCommandBuffer();

    // pre-record all the commandbuffers when VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT is OFF
    void recordCommandBuffersForAllSwapChainImage();
    void recordCommandBufferForOneSwapChainImage(
        uint32_t currentFrameId,
        VkCommandBuffer commandBuffer,
        uint32_t imageIndex);
    void createPerFrameSyncObjects();

    // app-specific
    void preHostDeviceIO();
    void postHostDeviceIO();

    void loadVao();
    void loadTextures();

    VkContext &_ctx;

    // ownership of resource
    // std::vector<VkImageView> _swapChainImageViews;
    // fbo for swapchain
    std::vector<VkFramebuffer> _swapChainFramebuffers;
    VkRenderPass _swapChainRenderPass{VK_NULL_HANDLE};

    // for shaders
    // 1. all the shader modules for the pipeline
    VkShaderModule _vsShaderModule{VK_NULL_HANDLE};
    VkShaderModule _fsShaderModule{VK_NULL_HANDLE};

    // 2. for shader data pass-in
    // for all the layout(set=_, binding=_) in all the shader stage
    // refactoring to use _descriptorSetLayout per set
    // 0: ubo, 1: texture + sampler, 2: glb: ssbo
    std::vector<VkDescriptorSetLayout> _descriptorSetLayouts;
    VkDescriptorPool _descriptorSetPool{VK_NULL_HANDLE};
    std::unordered_map<VkDescriptorSetLayout *, std::vector<VkDescriptorSet>> _descriptorSets;

    // graphics pipeline
    std::tuple<std::unordered_map<GRAPHICS_PIPELINE_SEMANTIC, VkPipeline>, VkPipelineLayout> _graphicsPipelineEntity;
};