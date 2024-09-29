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
#include <glb.h>
#include <context.h>
#include <queuethreadsafe.h>
#include <future> //packaged_task<>

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
    enum DESC_LAYOUT_SEMANTIC : int
    {
        UBO = 0,
        COMBO_VERT,
        COMBO_IDR,
        TEX_SAMP,
        COMBO_MAT,
        DESC_LAYOUT_SEMANTIC_SIZE
    };

public:
    VkApplication() = delete;
    VkApplication(
        VkContext &ctx,
        const CameraBase &camera,
        const std::string &model)
        : _ctx(ctx), _camera(camera), _model(model)
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
    void recordCommandBuffer(
        uint32_t currentFrameId,
        VkCommandBuffer commandBuffer,
        uint32_t imageIndex);
    void createPerFrameSyncObjects();

    // app-specific
    void preHostDeviceIO();
    void loadVao();
    void loadTextures();

    // io reader
    void preloadGLB();
    void loadGLB();
    void loadGLBTextureAsync();
    void postHostDeviceIO();

    VkContext &_ctx;
    const CameraBase &_camera;
    std::string _model;

    // ownership of resource
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
    std::vector<VkDescriptorSetLayout> _descriptorSetLayouts;

    VkDescriptorPool _descriptorSetPool{VK_NULL_HANDLE};
    std::unordered_map<VkDescriptorSetLayout *, std::vector<VkDescriptorSet>> _descriptorSets;

    // resource
    std::vector<BufferEntity> _uniformBuffers;

    // graphics pipeline
    std::tuple<VkPipeline, VkPipelineLayout> _graphicsPipelineEntity;

    // vao, vbo, index buffer
    uint32_t _indexCount{0};
    // for vkCmdBindVertexBuffers and vkCmdBindIndexBuffer
    VkBuffer _deviceVb{VK_NULL_HANDLE}, _deviceIb{VK_NULL_HANDLE};
    // need to destroy staging buffer when io is completed
    VkBuffer _stagingVb{VK_NULL_HANDLE}, _stagingIb{VK_NULL_HANDLE};

    // texture
    VkImageView _imageView{VK_NULL_HANDLE};
    VkImage _image{VK_NULL_HANDLE};
    VkSampler _sampler{VK_NULL_HANDLE};
    VmaAllocation _vmaImageAllocation{VK_NULL_HANDLE};
    VkBuffer _stagingImageBuffer{VK_NULL_HANDLE};

    // glb scene
    std::shared_ptr<Scene> _scene; 
    // a lot of stageBuffers
    std::vector<BufferEntity> _stagingVbForMesh;
    std::vector<BufferEntity> _stagingIbForMesh;
    BufferEntity _stagingIndirectDrawBuffer;
    BufferEntity _stagingMatBuffer;

    // device buffer
    BufferEntity _compositeVB;
    BufferEntity _compositeIB;
    BufferEntity _compositeMatB;
    BufferEntity _indirectDrawB;

    // each buffer's size is needed when bindResourceToDescriptorSet
    uint32_t _compositeVBSizeInByte;
    uint32_t _compositeIBSizeInByte;
    uint32_t _compositeMatBSizeInByte;
    uint32_t _indirectDrawBSizeInByte;
    // number of meshes in the scene
    uint32_t _numMeshes;
    uint32_t _numTextures;

    // textures in the glb scene
    std::vector<ImageEntity> _glbImageEntities;
    std::vector<BufferEntity> _glbImageStagingBuffers;

    // samplers in the glb scene
    std::vector<std::tuple<VkSampler>> _glbSamplerEntities;

    using uploadTextureFn = void(void);
    QueueThreadSafe<std::packaged_task<uploadTextureFn>> _asyncTaskQueue;
    std::vector<std::future<void>> _asyncUploadTextureTaskFutures;
    std::future<void> _handleUploadTextureTaskFuture;

    QueueThreadSafe<std::packaged_task<void(void)>> _asyncTaskQueueForGenMipmaps;
    std::future<void> _handleTextureGenMipmapTaskFuture;

    // a release and acquire pair is performed by a VkSemaphore
    std::vector<VkSemaphore> _asyncTransferSemaphorePool;
};