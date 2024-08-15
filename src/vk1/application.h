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

template <typename T>
void setCorrlationId(T handle, VkDevice logicalDevice, VkObjectType type, const std::string &name)
{
    const VkDebugUtilsObjectNameInfoEXT objectNameInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = type,
        .objectHandle = reinterpret_cast<uint64_t>(handle),
        .pObjectName = name.c_str(),
    };

    VK_CHECK(vkSetDebugUtilsObjectNameEXT(logicalDevice, &objectNameInfo));
}

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
        "VK_KHR_portability_enumeration",
        // "VK_GOOGLE_surfaceless_query",
        "VK_LUNARG_direct_driver_loading",
        "VK_EXT_layer_settings"};
    return instanceExtensions;
}

class Window;
class VkApplication
{
public:
    VkApplication() = delete;
    VkApplication(const Window &window, const Camera &camera, const std::string& model) 
    : _window(window), _camera(camera), _model(model)
    {
    }
    void init();
    void teardown();
    // render loop will call this per-frame
    void renderPerFrame();

private:
    void createInstance();
    void createSurface();
    void selectPhysicalDevice();
    void queryPhysicalDeviceCaps();
    void selectQueueFamily();
    // feature chain
    void selectFeatures();
    void createLogicDevice();
    void cacheCommandQueue();
    void createVMA();
    // only depends on vk surface, one time deal
    void prepareSwapChainCreation();
    // called every resize();
    void createSwapChain();
    void createSwapChainImageViews();
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
    // create resource needed for shader.
    void createPersistentBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        const std::string &name,
        VkBuffer &buffer,
        VmaAllocation &vmaAllocation,
        VmaAllocationInfo &vmaAllocationInfo);
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
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void createPerFrameSyncObjects();

    // device cap check
    bool checkValidationLayerSupport();

    // vkGetPhysicalDeviceFeatures2 + linkedlist to fill in
    inline bool checkRayTracingSupport()
    {
        return (_accelStructFeature.accelerationStructure &&
                _rayTracingFeature.rayTracingPipeline && _rayQueryFeature.rayQuery);
    }

    inline bool isMultiviewSupported() const
    {
        return _vk11features.multiview;
    }

    inline bool isFragmentDensityMapSupported() const
    {
        return _fragmentDensityMapFeature.fragmentDensityMap == VK_TRUE;
    }

    // app-specific
    void preHostDeviceIO();
    void loadVao();
    void loadTextures();

    // io reader
    void loadGLB();
    void postHostDeviceIO();

    const Window &_window;
    const Camera &_camera;
    std::string _model;
    bool _initialized{false};
    bool _enableValidationLayers{true};
    const std::vector<const char *> _validationLayers = {
        "VK_LAYER_KHRONOS_validation"};

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

    const std::vector<const char *> _deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        // VkPhysicalDeviceVulkan11Features::shaderDrawParameters nees to be set VK_TRUE
        VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME, 
        VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_NV_MESH_SHADER_EXTENSION_NAME,            // mesh_shaders_extension_present
        VK_KHR_MULTIVIEW_EXTENSION_NAME,             // multiview_extension_present
        VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME, // fragment_shading_rate_present
        VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
        VK_KHR_MAINTENANCE2_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, // ray_tracing_present
        VK_KHR_SPIRV_1_4_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, // for ray tracing
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, // for ray tracing
        VK_KHR_RAY_QUERY_EXTENSION_NAME,                // ray query needed for raytracing
        VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,
        VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
    };

#if defined(__ANDROID__)
    // android specific
    std::unique_ptr<ANativeWindow, AndroidNativeWindowDeleter> _osWindow;
    AAssetManager *_assetManager;
#endif
    VkInstance _instance{VK_NULL_HANDLE};
    VkSurfaceKHR _surface{VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT _debugMessenger;

    VkPhysicalDevice _selectedPhysicalDevice{VK_NULL_HANDLE};
    VkPhysicalDeviceProperties _physicalDevicesProp1;

    VkStructChain<> _featureChain;

    // features supported by the selected physical device
    // KHR, ext, not in core features
    // VkPhysicalDeviceAccelerationStructureFeaturesKHR
    // VkPhysicalDeviceRayQueryFeaturesKHR
    // nv specific
    // VkPhysicalDeviceMeshShaderFeaturesNV
    VkPhysicalDeviceFragmentDensityMapOffsetFeaturesQCOM _fragmentDensityMapOffsetFeature{
        .sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_OFFSET_FEATURES_QCOM,
        .pNext = nullptr,
    };

    VkPhysicalDeviceFragmentDensityMapFeaturesEXT _fragmentDensityMapFeature{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT,
        .pNext = &_fragmentDensityMapOffsetFeature,
    };

    VkPhysicalDeviceMeshShaderFeaturesNV _meshShaderFeature{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV,
        .pNext = (void *)&_fragmentDensityMapFeature,
    };

    VkPhysicalDeviceRayQueryFeaturesKHR _rayQueryFeature{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
        .pNext = (void *)&_meshShaderFeature,
    };

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR _rayTracingFeature{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
        .pNext = (void *)&_rayQueryFeature,
    };

    VkPhysicalDeviceAccelerationStructureFeaturesKHR _accelStructFeature{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .pNext = (void *)&_rayTracingFeature,
    };

    VkPhysicalDeviceVulkan11Features _vk11features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .pNext = (void *)&_accelStructFeature,
    };
    VkPhysicalDeviceVulkan12Features _vk12features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = (void *)&_vk11features,
    };
    VkPhysicalDeviceVulkan13Features _vk13features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = (void *)&_vk12features,
    };
    VkPhysicalDeviceFeatures2 _physicalFeatures2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = (void *)&_vk13features,
    };
    // included in v1.1
    // VkPhysicalDeviceMultiviewFeatures
    // included in v1.2
    // VkPhysicalDeviceBufferDeviceAddressFeatures
    // VkPhysicalDeviceDescriptorIndexingFeatures
    // VkPhysicalDeviceTimelineSemaphoreFeatures

    // device properties
    VkPhysicalDeviceProperties2 _properties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        //.pNext = &rayTracingPipelineProperties_,
    };
    // header of linked list to create logic device
    VkPhysicalDeviceFeatures2 _enabledDeviceFeatures;

    VkDevice _logicalDevice{VK_NULL_HANDLE};
    bool _bindlessSupported{false};
    bool _protectedMemory{false};

    uint32_t _graphicsComputeQueueFamilyIndex{std::numeric_limits<uint32_t>::max()};
    uint32_t _computeQueueFamilyIndex{std::numeric_limits<uint32_t>::max()};
    uint32_t _transferQueueFamilyIndex{std::numeric_limits<uint32_t>::max()};
    // family queue of discrete gpu support the surface of native window
    uint32_t _presentQueueFamilyIndex{std::numeric_limits<uint32_t>::max()};

    // each queue family have many queues (16)
    // I choose 0 as graphics, 1 as compute
    uint32_t _graphicsQueueIndex{std::numeric_limits<uint32_t>::max()};
    uint32_t _computeQueueIndex{std::numeric_limits<uint32_t>::max()};

    VkQueue _graphicsQueue{VK_NULL_HANDLE};
    VkQueue _computeQueue{VK_NULL_HANDLE};
    VkQueue _transferQueue{VK_NULL_HANDLE};
    VkQueue _presentationQueue{VK_NULL_HANDLE};
    VkQueue _sparseQueues{VK_NULL_HANDLE};

    VmaAllocator _vmaAllocator{VK_NULL_HANDLE};

    VkExtent2D _swapChainExtent;
    const uint32_t _swapChainImageCount{3};

    // constexpr VkFormat _swapChainFormat{VK_FORMAT_B8G8R8A8_UNORM};
    //  gamma correction.
    //  Using a swapchain with VK_FORMAT_B8G8R8A8_SRGB
    //  leverages the ability to to apply gamma correction as the final step in your render pipeline
    //  If your swapchain does the gamma correction, you do not need todo it in your shaders
    const VkFormat _swapChainFormat{VK_FORMAT_R8G8B8A8_SRGB};
    const VkColorSpaceKHR _colorspace{VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    VkSurfaceTransformFlagBitsKHR _pretransformFlag;
    VkSwapchainKHR _swapChain{VK_NULL_HANDLE};
    std::vector<VkImageView> _swapChainImageViews;
    // fbo for swapchain
    std::vector<VkFramebuffer> _swapChainFramebuffers;

    VkRenderPass _swapChainRenderPass{VK_NULL_HANDLE};

    VkShaderModule _vsShaderModule{VK_NULL_HANDLE};
    VkShaderModule _fsShaderModule{VK_NULL_HANDLE};

    // for shader data pass-in
    // for all the layout(set=_, binding=_) in all the shader stage
    // refactoring to use _descriptorSetLayout per set
    // 0: ubo, 1: texture + sampler, 2: glb: ssbo
    vector<VkDescriptorSetLayout> _descriptorSetLayouts;
    VkDescriptorSetLayout _descriptorSetLayoutForUbo;
    // combined textures and sampler
    VkDescriptorSetLayout _descriptorSetLayoutForTextureSampler;
    // for vb
    VkDescriptorSetLayout _descriptorSetLayoutForComboVertexBuffer;
    // for indirectDrawBuffer
    VkDescriptorSetLayout _descriptorSetLayoutForIndirectDrawBuffer;
    // for glb textures + sampler (binding1 and binding2)
    VkDescriptorSetLayout _descriptorSetLayoutForTextureAndSampler;
    // for glb samplers
    // VkDescriptorSetLayout _descriptorSetLayoutForSamplers;
    // for glb materials
    VkDescriptorSetLayout _descriptorSetLayoutForComboMaterialBuffer;
    // bindful glb texture + sampler
    VkDescriptorSetLayout _descriptorSetLayoutForBindfulTextureSamplers;

    VkDescriptorPool _descriptorSetPool{VK_NULL_HANDLE};
    // why vector ? triple-buffer
    std::vector<VkDescriptorSet> _descriptorSetsForUbo;
    VkDescriptorSet _descriptorSetsForTextureSampler;
    // for vb
    VkDescriptorSet _descriptorSetsForVerticesBuffer;
    // for indirectDrawBuffer
    VkDescriptorSet _descriptorSetsForIndirectDrawBuffer;
    // for glb textures
    VkDescriptorSet _descriptorSetsForTextureAndSampler;
    // for glb samplers
    // VkDescriptorSet _descriptorSetsForSampler;
    // for glb materials
    VkDescriptorSet _descriptorSetsForMaterialBuffer;
    // for bind resource to descriptor sets
    std::vector<VkWriteDescriptorSet> _writeDescriptorSetBundle;

    // resource
    // to implement in entity Buffer.
    std::vector<VkBuffer> _uniformBuffers;
    std::vector<VmaAllocation> _vmaAllocations;
    std::vector<VmaAllocationInfo> _vmaAllocationInfos;
    // graphics pipeline
    // for multiple sets + bindings
    VkPipelineLayout _pipelineLayout;
    VkPipeline _graphicsPipeline;

    // cmd
    VkCommandPool _commandPool;
    std::vector<VkCommandBuffer> _commandBuffers;

    // GPU-CPU SYNC
    std::vector<VkSemaphore> _imageCanAcquireSemaphores;
    std::vector<VkSemaphore> _imageRendereredSemaphores;
    // host
    std::vector<VkFence> _inFlightFences;
    // 0, 1, 2, 0, 1, 2, ...
    uint32_t _currentFrameId = 0;

    // vao, vbo, index buffer
    uint32_t _indexCount{0};
    // for vkCmdBindVertexBuffers and vkCmdBindIndexBuffer
    VkBuffer _deviceVb{VK_NULL_HANDLE}, _deviceIb{VK_NULL_HANDLE};
    // need to destroy staging buffer when io is completed
    VkBuffer _stagingVb{VK_NULL_HANDLE}, _stagingIb{VK_NULL_HANDLE};

    // host-device io (during initialization)
    VkCommandBuffer _uploadCmd;
    VkFence _ioFence;

    // texture
    VkImageView _imageView{VK_NULL_HANDLE};
    VkImage _image{VK_NULL_HANDLE};
    VkSampler _sampler{VK_NULL_HANDLE};
    VmaAllocation _vmaImageAllocation{VK_NULL_HANDLE};
    VkBuffer _stagingImageBuffer{VK_NULL_HANDLE};

    // glb scene
    // a lot of stageBuffers
    vector<VkBuffer> _stagingVbForMesh;
    vector<VkBuffer> _stagingIbForMesh;
    VkBuffer _stagingIndirectDrawBuffer{VK_NULL_HANDLE};
    VkBuffer _stagingMatBuffer{VK_NULL_HANDLE};

    // device buffer
    VkBuffer _compositeVB{VK_NULL_HANDLE};
    VkBuffer _compositeIB{VK_NULL_HANDLE};
    VkBuffer _compositeMatB{VK_NULL_HANDLE};
    VkBuffer _indirectDrawB{VK_NULL_HANDLE};
    // each buffer's size is needed when bindResourceToDescriptorSet
    uint32_t _compositeVBSizeInByte;
    uint32_t _compositeIBSizeInByte;
    uint32_t _compositeMatBSizeInByte;
    uint32_t _indirectDrawBSizeInByte;
    // number of meshes in the scene
    uint32_t _numMeshes;

    // textures in the glb scene
    std::vector<VkImage> _glbImages;
    std::vector<VkImageView> _glbImageViews;
    std::vector<VmaAllocation> _glbImageAllocation;
    std::vector<VkBuffer> _glbImageStagingBuffer;

    // samplers in the glb scene
    std::vector<VkSampler> _glbSamplers;



    //    Camera _camera{
    //            vec3f(std::array{0.079639f, 0.511857f, 4.f}), // pos
    //            vec3f(std::array{0.079639f, 0.511857f, 0.483869f}), // target -z
    //            vec3f(std::array{0.0f, 1.0f, 0.0f}),  // initial world up
    //            0.0f,                                 // initial pitch
    //            -97.f                                 // initial yaw
    //    };


};