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
#include <camera.h>
#include <glb.h>

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
public:
    VkContext() = delete;
    VkContext(
        const Window &window,
        const std::vector<const char *> &instanceValidationLayers,
        const std::set<std::string> &instanceExtensions,
        const std::vector<const char *> deviceExtensions,
        const Camera &camera,
        const std::string &model)
        : _window(window),
          _instanceValidationLayers(instanceValidationLayers),
          _instanceExtensions(instanceExtensions),
          _deviceExtensions(deviceExtensions),
          _camera(camera),
          _model(model)
    {
    }
    VkContext(const VkContext &) = delete;
    VkContext &operator=(const VkContext &) = delete;
    VkContext(VkContext &&) noexcept = default;
    VkContext &operator=(VkContext &&) noexcept = default;

    ~VkContext(){
        
    }

private:
    const Window &_window;
    const Camera &_camera;
    const std::vector<const char *> _instanceValidationLayers;
    const std::set<std::string> &_instanceExtensions;
    std::string _model;
    const std::vector<const char *> _deviceExtensions;
};
