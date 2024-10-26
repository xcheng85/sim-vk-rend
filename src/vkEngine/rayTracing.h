#pragma once

#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include <misc.h>
#include <renderPassBase.h>

class RayTracing : public RenderPassBase,
                   public VkContextAccessor,
                   public SceneAccessor,
                   public DescriptorPoolAccessor,
                   public CameraAccessor
{
public:
    RayTracing()
    {
    }

    ~RayTracing()
    {
    }

    virtual void setContext(VkContext *ctx) override
    {
        _ctx = ctx;
    }

    virtual const VkContext &context() const override
    {
        return *_ctx;
    }

    virtual void setScene(std::shared_ptr<Scene> scene) override
    {
        _scene = scene;
    }

    virtual const Scene &scene() const override
    {
        return *_scene;
    }

    virtual const CameraBase &camera() const override
    {
        return *_camera;
    }

    virtual void setCamera(const CameraBase *camera) override
    {
        _camera = camera;
    }

    virtual void setDescriptorPool(const VkDescriptorPool dsPool) override
    {
        _dsPool = dsPool;
    }

    virtual const VkDescriptorPool descriptorPool() const override
    {
        return _dsPool;
    }

    virtual void finalizeInit() override
    {
        initShaderModules();
        createDescriptorSetLayout();
        initRTPipeline();
        allocateDescriptorSets();
        createSBT();
        initRTOutputImage();
        initUniformCameraPropBuffer();
        initBLAS();
        initTLAS();

        // in & out
        bindResourceToDescriptorSets();
    }

    void initShaderModules()
    {
        ASSERT(_ctx, "vk context should be defined");
        auto logicalDevice = _ctx->getLogicDevice();
        const auto shadersPath = getAssetPath();
        // ray generation
        const auto rayGenerationShaderPath = shadersPath + "/rayGeneration.rgen";
        _rtRayGenShaderModule = createShaderModule(
            logicalDevice,
            rayGenerationShaderPath,
            "main",
            "rayGeneration.rgen");
        // ray miss intersection
        const auto rayMissShaderPath = shadersPath + "/rayMiss.rmiss";
        _rtRayMissShaderModule = createShaderModule(
            logicalDevice,
            rayMissShaderPath,
            "main",
            "rayMiss.rmiss");
        // closet intersection
        const auto rayClosestHitShaderPath = shadersPath + "/rayClosestHit.rchit";
        _rtRayClosestHitShaderModule = createShaderModule(
            logicalDevice,
            rayClosestHitShaderPath,
            "main",
            "rayClosestHit.rchit");
    }

    enum DESC_LAYOUT_SEMANTIC : int
    {
        AS = 0, // accelerated structure
        OUTPUT_IMAGE = 1,
        CAMERA_PROP = 2,
        DESC_LAYOUT_SEMANTIC_SIZE
    };

    // layout(set = 0, binding = 0) uniform accelerationStructureEXT as;
    // // write to
    // layout(set = 1, binding = 0, rgba8) uniform image2D image;
    // // input
    // layout(set = 2, binding = 0) uniform CameraProperties
    void createDescriptorSetLayout()
    {
        ASSERT(_ctx, "vk context should be defined");
        // refer to rayGeneration.rgen

        std::vector<std::vector<VkDescriptorSetLayoutBinding>> setBindings(DESC_LAYOUT_SEMANTIC_SIZE);
        setBindings[DESC_LAYOUT_SEMANTIC::AS].resize(1);
        setBindings[DESC_LAYOUT_SEMANTIC::AS][0].binding = 0; // depends on the shader: set 0, binding = 0
        setBindings[DESC_LAYOUT_SEMANTIC::AS][0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        setBindings[DESC_LAYOUT_SEMANTIC::AS][0].descriptorCount = 1;
        setBindings[DESC_LAYOUT_SEMANTIC::AS][0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

        setBindings[DESC_LAYOUT_SEMANTIC::OUTPUT_IMAGE].resize(1);
        setBindings[DESC_LAYOUT_SEMANTIC::OUTPUT_IMAGE][0].binding = 0; // depends on the shader: set 0, binding = 0
        setBindings[DESC_LAYOUT_SEMANTIC::OUTPUT_IMAGE][0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        setBindings[DESC_LAYOUT_SEMANTIC::OUTPUT_IMAGE][0].descriptorCount = 1;
        setBindings[DESC_LAYOUT_SEMANTIC::OUTPUT_IMAGE][0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

        setBindings[DESC_LAYOUT_SEMANTIC::CAMERA_PROP].resize(1);
        setBindings[DESC_LAYOUT_SEMANTIC::CAMERA_PROP][0].binding = 0; // depends on the shader: set 0, binding = 0
        setBindings[DESC_LAYOUT_SEMANTIC::CAMERA_PROP][0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        setBindings[DESC_LAYOUT_SEMANTIC::CAMERA_PROP][0].descriptorCount = 1;
        setBindings[DESC_LAYOUT_SEMANTIC::CAMERA_PROP][0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

        _descriptorSetLayouts = _ctx->createDescriptorSetLayout(setBindings);
    }

    void initRTPipeline()
    {
        ASSERT(_ctx, "vk context should be defined");
        ASSERT(_rtRayGenShaderModule, "ray gen shader module should be defined");
        ASSERT(_rtRayMissShaderModule, "ray miss shader module should be defined");
        ASSERT(_rtRayClosestHitShaderModule, "ray closest hit shader module should be defined");

        const std::string entryPoint{"main"s};
        _rtPipelineEntity = _ctx->createRayTracingPipeline(
            {
                {VK_SHADER_STAGE_RAYGEN_BIT_KHR, make_tuple(_rtRayGenShaderModule, entryPoint.c_str(), nullptr)},
                {VK_SHADER_STAGE_MISS_BIT_KHR, make_tuple(_rtRayMissShaderModule, entryPoint.c_str(), nullptr)},
                {VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, make_tuple(_rtRayClosestHitShaderModule, entryPoint.c_str(), nullptr)},
            },
            _descriptorSetLayouts,
            {});
    }

    void allocateDescriptorSets()
    {
        ASSERT(_ctx, "vk context should be defined");
        ASSERT(_dsPool, "descriptorset pool should be defined");
        _descriptorSets = _ctx->allocateDescriptorSet(_dsPool,
                                                      {{&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::AS],
                                                        1},
                                                       {&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::OUTPUT_IMAGE],
                                                        1},
                                                       {&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::CAMERA_PROP],
                                                        1}});
    }

    // Shader Binding Table:
    // 1. It indicates the shaders that operate on each geometry in an acceleration structure.
    // 2. it contains the resources accessed by each shader, including indices of textures, buffer device addresses, and constants.
    // 3. The application allocates and manages shader binding tables as VkBuffer objects.
    void createSBT()
    {
        ASSERT(_ctx, "vk context should be defined");
        auto logicalDevice = _ctx->getLogicDevice();

        const auto &rtProperties = _ctx->getSelectedPhysicalDeviceRayTracingProperties();
        const uint32_t handleSizeInBytes = rtProperties.shaderGroupHandleSize;
        const uint32_t handleSizeAligned = alignedSize(handleSizeInBytes, rtProperties.shaderGroupHandleAlignment);

        const auto rtPipeline = std::get<0>(_rtPipelineEntity);
        const auto shaderGroupCount = std::get<2>(_rtPipelineEntity).size();
        const uint32_t sbtSizeInBytes = shaderGroupCount * handleSizeAligned;
        std::vector<uint8_t> shaderGroupHandles(sbtSizeInBytes);
        // filling the shaderGroupHandles buffer
        VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(
            logicalDevice,
            rtPipeline,
            0, // first Shader Group
            shaderGroupCount,
            sbtSizeInBytes,
            shaderGroupHandles.data()));
        {
            //  buffer is suitable for use as a Shader Binding Table.
            _rayGenSTBBuffer = _ctx->createShaderBindTableBuffer(
                "Ray Gen STB Buffer",
                handleSizeInBytes * 1, // 1 ray gen shader stage,
                handleSizeAligned * 1, // buffer size considering alignment
                handleSizeAligned,
                true // mapping
            );
            // copy into _rayGenSTBBuffer
            auto &dst = std::get<3>(std::get<0>(_rayGenSTBBuffer));
            memcpy(dst, shaderGroupHandles.data(), handleSizeInBytes);
        }

        {
            _rayMissSTBBuffer = _ctx->createShaderBindTableBuffer(
                "Ray Miss STB Buffer",
                handleSizeInBytes * 1,
                handleSizeAligned * 1, // buffer size considering alignment
                handleSizeAligned,
                true);
            auto &dst = std::get<3>(std::get<0>(_rayMissSTBBuffer));
            memcpy(dst, shaderGroupHandles.data() + handleSizeInBytes, handleSizeInBytes);
        }

        {
            _rayClosestHitSTBBuffer = _ctx->createShaderBindTableBuffer(
                "Ray Closest Hit STB Buffer",
                handleSizeInBytes * 1,
                handleSizeAligned * 1, // buffer size considering alignment
                handleSizeAligned,
                true);
            auto &dst = std::get<3>(std::get<0>(_rayClosestHitSTBBuffer));
            memcpy(dst, shaderGroupHandles.data() + handleSizeInBytes * 2, handleSizeInBytes);
        }
    }

    void initRTOutputImage()
    {
        ASSERT(_ctx, "vk context should be defined");
        const bool generateMipmaps = false;
        VkFormat swapChainFormat{VK_FORMAT_B8G8R8A8_UNORM};
        uint32_t textureMipLevels{1};
        uint32_t textureLayoutCount{1};
        auto extents = _ctx->getSwapChainExtent();
        _rtOutputImage = _ctx->createImage("rt output image",
                                           VK_IMAGE_TYPE_2D,
                                           swapChainFormat,
                                           {
                                               .width = extents.width,
                                               .height = extents.height,
                                               .depth = 1,
                                           },
                                           textureMipLevels,
                                           textureLayoutCount,
                                           VK_SAMPLE_COUNT_1_BIT,
                                           // usage here: both dst and src as mipmap generation
                                           VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                           generateMipmaps);
    }

    void initUniformCameraPropBuffer()
    {
        ASSERT(_ctx, "vk context should be defined");

        _uniformCameraPropBuffer = _ctx->createPersistentBuffer(
            "Uniform Camera Prop Buffer",
            sizeof(UniformCameraProp),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    }

    void initBLAS()
    {
    }

    void initTLAS()
    {
    }

    void bindResourceToDescriptorSets()
    {
        ASSERT(_ctx, "vk context should be defined");
    }

    virtual void execute(CommandBufferEntity cmd, int currentFrameId) override
    {
    }

private:
    // for pipeline and binding resource
    std::vector<VkDescriptorSetLayout> _descriptorSetLayouts;
    VkShaderModule _rtRayGenShaderModule{VK_NULL_HANDLE};
    VkShaderModule _rtRayMissShaderModule{VK_NULL_HANDLE};
    VkShaderModule _rtRayClosestHitShaderModule{VK_NULL_HANDLE};
    std::tuple<VkPipeline, VkPipelineLayout, std::vector<VkRayTracingShaderGroupCreateInfoKHR>> _rtPipelineEntity;
    std::unordered_map<VkDescriptorSetLayout *, std::vector<VkDescriptorSet>> _descriptorSets;

    // buffers for stb
    std::tuple<BufferEntity, VkStridedDeviceAddressRegionKHR> _rayGenSTBBuffer;
    std::tuple<BufferEntity, VkStridedDeviceAddressRegionKHR> _rayMissSTBBuffer;
    std::tuple<BufferEntity, VkStridedDeviceAddressRegionKHR> _rayClosestHitSTBBuffer;

    // image which rt output to
    // input/output of rt shaders
    ImageEntity _rtOutputImage;
    BufferEntity _uniformCameraPropBuffer;
};