#define VK_NO_PROTOTYPES // for volk
#define VOLK_IMPLEMENTATION
#include "volk.h"
#include <window.h>
#include <assert.h>
#include <iostream>
#include <format>
#include <vector>
#include <algorithm>
#include <iterator>
#include <numeric>
#include <array>
#include <filesystem> // for shader

#include <cuDevice.h>
#include <cuPredefines.h>

// To do it properly:

// Include "vk_mem_alloc.h" file in each CPP file where you want to use the library. This includes declarations of all members of the library.
// In exactly one CPP file define following macro before this include. It enables also internal definitions.

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h> //c
#include <glslang/Public/ResourceLimits.h>    //c++
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>

#include <application.h>
#include <context.h>
#include <arcballCamera.h>

using namespace cudaEngine;

bool gRunning = true;
double gDt{0};
uint64_t gLastFrame{0};

inline const std::set<std::string> &getInstanceExtensions()
{
    static std::set<std::string> instanceExtensions = {
        "VK_KHR_surface",
        "VK_KHR_display",
#if defined(VK_USE_PLATFORM_XLIB_KHR)
        "VK_KHR_xlib_surface",
#endif /*VK_USE_PLATFORM_XLIB_KHR*/
#if defined(VK_USE_PLATFORM_XCB_KHR)
        "VK_KHR_xcb_surface",
#endif /*VK_USE_PLATFORM_XCB_KHR*/
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
        "VK_KHR_wayland_surface",
#endif /*VK_USE_PLATFORM_WAYLAND_KHR*/
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
        "VK_KHR_android_surface",
#endif /*VK_USE_PLATFORM_ANDROID_KHR*/
#if defined(VK_USE_PLATFORM_WIN32_KHR)
        "VK_KHR_win32_surface",
#endif /*VK_USE_PLATFORM_WIN32_KHR*/
        "VK_EXT_debug_report",
        // "VK_NV_external_memory_capabilities",
        // VK_KHR_get_physical_device_properties2 required for dynamic rendering feature
        "VK_KHR_get_physical_device_properties2",
        //"VK_EXT_validation_flags", //deprecated by VK_EXT_layer_settings
        "VK_KHR_device_group_creation",
        "VK_KHR_external_memory_capabilities",
        "VK_KHR_external_semaphore_capabilities",
        "VK_EXT_direct_mode_display",
        // "VK_EXT_display_surface_counter",
        "VK_EXT_swapchain_colorspace",
        "VK_KHR_external_fence_capabilities",
        "VK_KHR_get_surface_capabilities2",
        "VK_KHR_get_display_properties2",
        "VK_EXT_debug_utils",
        // "VK_KHR_surface_protected_capabilities",
        "VK_EXT_validation_features",
        // "VK_EXT_headless_surface",
        "VK_EXT_surface_maintenance1",
        // "VK_EXT_acquire_drm_display",
        //"VK_KHR_portability_enumeration", // crash when renderdoc launch
        // "VK_GOOGLE_surfaceless_query",
        //"VK_LUNARG_direct_driver_loading", // crash when renderdoc launch
        // retrieving debug info from shaders
        // "VK_EXT_layer_settings", // crash when renderdoc launch
        // Multiple APIs, cuda here
        VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
        VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME
        };
    return instanceExtensions;
}

int main(int argc, char **argv)
{
    selectDevice();
    
    ArcballCamera _orbitCamera{
        glm::vec3(0.0038983f, -0.142875f, 0.6f),        // pos
        glm::vec3(0.00238983f, -0.142875f, 0.1381729f), // target -z
        glm::vec3(0.0f, 1.f, 0.f),                      // initial world up
        0.01f,
        4000.f,
        60.f,
        1280.0 / 720.0};

    WindowConfig cfg{1280, 720, "vkJulia"};
    WindowEntity window(&cfg, _orbitCamera);

    VK_CHECK(volkInitialize());
    // c api
    // glslang_initialize_process();
    glslang::InitializeProcess();

    const std::vector<const char *> instanceValidationLayers = {
        "VK_LAYER_KHRONOS_validation"};
    const auto &instanceExtensions = getInstanceExtensions();
    const std::vector<const char *> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        // VkPhysicalDeviceVulkan11Features::shaderDrawParameters nees to be set VK_TRUE
        VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
        VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        // VK_NV_MESH_SHADER_EXTENSION_NAME,            // mesh_shaders_extension_present, nvidia specific ext, renderdoc crash
        VK_KHR_MULTIVIEW_EXTENSION_NAME,             // multiview_extension_present
        VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME, // fragment_shading_rate_present
        VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
        VK_KHR_MAINTENANCE2_EXTENSION_NAME,
        // renderdoc crash
        // VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, // providing ray tracing shader stages and pipelines
        VK_KHR_SPIRV_1_4_EXTENSION_NAME,
        // renderdoc crash
        // VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, // for ray tracing: which provides functionality for acceleration structure building and management
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        // renderdoc crash
        // VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, // for ray tracing
        // VK_KHR_RAY_QUERY_EXTENSION_NAME, // providing ray query intrinsics for all shader stages
        VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,
        VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
        // cuda interop
        VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
#ifdef _WIN64
        VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME,
#else
        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
#endif
    };

    VkContext ctx(window,
                  instanceValidationLayers,
                  instanceExtensions,
                  deviceExtensions);

    VkApplication vkApp(ctx);
    vkApp.init();

    // init();
    gLastFrame = SDL_GetPerformanceCounter();
    // gLastFrame = SDL_GetTicks();
    while (gRunning)
    {
        auto currTime = SDL_GetPerformanceCounter();
        // auto currTime = SDL_GetTicks();
        // gDt = (currTime - gLastFrame) / 1000.0; // Convert to seconds.
        //  gDt = currTime - gLastFrame;
        gDt = static_cast<double>(
            (currTime - gLastFrame) / static_cast<double>(SDL_GetPerformanceFrequency()));
        gLastFrame = currTime;
        window.pollEvents();
        vkApp.renderPerFrame();
    }

    vkApp.teardown();
    window.shutdown();
    // glslang_finalize_process();
    glslang::FinalizeProcess();
    return 0;
}