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
class Camera;
class VkContext;
class VkApplication
{
public:
    VkApplication() = delete;
    VkApplication(
      VkContext &ctx, 
    const Camera& camera,
    const std::string& model) 
    : _ctx(ctx), _camera(camera), _model(model)
    {
    }
    void init();
    void teardown();
    // render loop will call this per-frame
    void renderPerFrame();

private:
    // // only depends on vk surface, one time deal
    // void prepareSwapChainCreation();
    // called every resize();
    // void createSwapChain();
    // void createSwapChainImageViews();
    
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


    // app-specific
    void preHostDeviceIO();
    void loadVao();
    void loadTextures();

    // io reader
    void loadGLB();
    void postHostDeviceIO();

    VkContext &_ctx;
    const Camera &_camera;
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
    // std::vector<VkBuffer> _uniformBuffers;
    // std::vector<VmaAllocation> _vmaAllocations;
    // std::vector<VmaAllocationInfo> _vmaAllocationInfos;

    std::vector<std::tuple<VkBuffer, VmaAllocation, VmaAllocationInfo>> _uniformBuffers;


    // graphics pipeline
    // for multiple sets + bindings
    VkPipelineLayout _pipelineLayout;
    VkPipeline _graphicsPipeline;

    // cmdpool and cmdbuffers
    std::vector<std::tuple<VkCommandPool, VkCommandBuffer, VkFence>> _cmdBuffersForRendering;
    std::vector<std::tuple<VkCommandPool, VkCommandBuffer, VkFence>> _cmdBuffersForIO;

    // GPU-CPU SYNC
    std::vector<VkSemaphore> _imageCanAcquireSemaphores;
    std::vector<VkSemaphore> _imageRendereredSemaphores;
    // 0, 1, 2, 0, 1, 2, ...
    uint32_t _currentFrameId = 0;

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
    // a lot of stageBuffers
    std::vector<VkBuffer> _stagingVbForMesh;
    std::vector<VkBuffer> _stagingIbForMesh;
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
};