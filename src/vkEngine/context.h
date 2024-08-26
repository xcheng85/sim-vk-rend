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
#include <utility>
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
class VkContext
{
    class Impl;

public:
    VkContext() = delete;
    VkContext(
        const Window &window,
        const std::vector<const char *> &instanceValidationLayers,
        const std::set<std::string> &instanceExtensions,
        const std::vector<const char *> deviceExtensions);
    VkContext(const VkContext &) = delete;
    VkContext &operator=(const VkContext &) = delete;
    VkContext(VkContext &&) noexcept = default;
    VkContext &operator=(VkContext &&) noexcept = default;

    ~VkContext();

    void createSwapChain();

    // generic to swapchain image and non-swapchain images
    VkRenderPass createSwapChainRenderPass();
    // renderPass, imagecount
    VkFramebuffer createFramebuffer(
        const std::string &name,
        VkRenderPass renderPass,
        const std::vector<VkImageView> &colors,
        const VkImageView depth,
        const VkImageView stencil,
        uint32_t width,
        uint32_t height);

    std::vector<std::tuple<VkCommandPool, VkCommandBuffer, VkFence>> createGraphicsCommandBuffers(
        const std::string &name,
        uint32_t count,
        uint32_t inflightCount,
        VkFenceCreateFlags flags);

    std::vector<std::tuple<VkCommandPool, VkCommandBuffer, VkFence>> createTransferCommandBuffers(
        const std::string &name,
        uint32_t count,
        uint32_t inflightCount,
        VkFenceCreateFlags flags);

    std::tuple<VkBuffer, VmaAllocation, VmaAllocationInfo> createPersistentBuffer(
        const std::string &name,
        VkDeviceSize bufferSizeInBytes,
        VkBufferUsageFlags bufferUsageFlag);

    std::tuple<VkBuffer, VmaAllocation, VmaAllocationInfo> createStagingBuffer(
        const std::string &name,
        VkDeviceSize bufferSizeInBytes);

    // device local buffer
    std::tuple<VkBuffer, VmaAllocation, VmaAllocationInfo> createDeviceLocalBuffer(
        const std::string &name,
        VkDeviceSize bufferSizeInBytes,
        VkBufferUsageFlags bufferUsageFlag);

    VkInstance getInstance() const;
    VkDevice getLogicDevice() const;
    VmaAllocator getVmaAllocator() const;
    VkQueue getGraphicsQueue() const;
    VkQueue getPresentationQueue() const;

    VkPhysicalDevice getSelectedPhysicalDevice() const;
    VkPhysicalDeviceProperties getSelectedPhysicalDeviceProp() const;

    VkSurfaceKHR getSurfaceKHR() const;

    uint32_t getGraphicsComputeQueueFamilyIndex() const;
    uint32_t getPresentQueueFamilyIndex() const;

    VkPhysicalDeviceVulkan12Features getVk12FeatureCaps() const;

    VkSwapchainKHR getSwapChain() const;
    VkExtent2D getSwapChainExtent() const;

    const std::vector<VkImageView> &getSwapChainImageViews() const;

    // features chains
    // now is to toggle features selectively
    // enable features
    static VkPhysicalDeviceFeatures sPhysicalDeviceFeatures; // cannot fly without sPhysicalDeviceFeatures2
    static VkPhysicalDeviceFeatures2 sPhysicalDeviceFeatures2;

    static VkPhysicalDeviceVulkan11Features sEnable11Features;
    static VkPhysicalDeviceVulkan12Features sEnable12Features;
    static VkPhysicalDeviceVulkan13Features sEnable13Features;
    static VkPhysicalDeviceFragmentDensityMapFeaturesEXT sFragmentDensityMapFeatures;
    // for ray-tracing
    static VkPhysicalDeviceAccelerationStructureFeaturesKHR sAccelStructFeatures;
    static VkPhysicalDeviceRayTracingPipelineFeaturesKHR sRayTracingPipelineFeatures;
    static VkPhysicalDeviceRayQueryFeaturesKHR sRayQueryFeatures;

private:
    std::unique_ptr<Impl> _pimpl;
};
