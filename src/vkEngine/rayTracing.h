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

    // algorithm specific
    inline void setCompositeVerticeBuffer(BufferEntity *vb)
    {
        _compositeVB = vb;
    }

    inline void setCompositeIndicesBuffer(BufferEntity *ib)
    {
        _compositeIB = ib;
    }

    inline void setCompositeMaterialBuffer(BufferEntity *mb)
    {
        _compositeMatB = mb;
    }

    inline void setIndirectDrawBuffer(BufferEntity *idb)
    {
        _indirectDrawB = idb;
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
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    }

    void initBLAS()
    {
        // real geometry data
        ASSERT(_ctx, "vk context should be defined");
        ASSERT(_scene, "scene should be defined");
        auto logicalDevice = _ctx->getLogicDevice();
        // a graphics command buffer is needed for build AS
        auto cmdBuffersForIO = _ctx->getCommandBufferForIO();
        // where the command buffer submitted to.
        auto graphicsComputeQueue = _ctx->getGraphicsComputeQueue();

        size_t meshId = 0;
        for (const auto &mesh : _scene->meshes)
        {
            // from the composite vb and composite ib, I need to fetch the range of vb and ib for this mesh
            // auto vertexByteSizeMesh = sizeof(Vertex) * mesh.vertices.size();
            // auto vertexBufferPtr = reinterpret_cast<const void *>(mesh.vertices.data());
            // auto indicesByteSizeMesh = sizeof(uint32_t) * mesh.indices.size();
            // auto indicesBufferPtr = reinterpret_cast<const void *>(mesh.indices.data());

            auto vbDeviceStartingAddress = std::get<BUFFER_ENTITY_UID::DEVICE_HOST_ADDRESS>(*_compositeVB).deviceAddress;
            auto vbOffsetInByteForMesh = _scene->indirectDraw[meshId].vertexOffset * sizeof(Vertex);
            VkDeviceOrHostAddressConstKHR vbDeviceAddressForMesh{
                .deviceAddress = vbDeviceStartingAddress + vbOffsetInByteForMesh,
            };

            const auto numVertices = mesh.vertices.size();

            auto ibDeviceStartingAddress = std::get<BUFFER_ENTITY_UID::DEVICE_HOST_ADDRESS>(*_compositeIB).deviceAddress;
            auto ibOffsetInByteForMesh = _scene->indirectDraw[meshId].firstIndex * sizeof(uint32_t);
            VkDeviceOrHostAddressConstKHR ibDeviceAddressForMesh{
                .deviceAddress = ibDeviceStartingAddress + ibOffsetInByteForMesh,
            };

            VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
            accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            // VK_GEOMETRY_OPAQUE_BIT_KHR indicates that this geometry does not invoke the any-hit shaders even if present in a hit group.
            accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
            accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
            accelerationStructureGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
            accelerationStructureGeometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
            accelerationStructureGeometry.geometry.triangles.vertexData = vbDeviceAddressForMesh;
            accelerationStructureGeometry.geometry.triangles.maxVertex = numVertices;
            accelerationStructureGeometry.geometry.triangles.vertexStride = sizeof(Vertex);
            accelerationStructureGeometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
            accelerationStructureGeometry.geometry.triangles.indexData = ibDeviceAddressForMesh;
            // no per-vertex transform needed here, done in the glb.cpp, see line334. vertex.transform(m);
            accelerationStructureGeometry.geometry.triangles.transformData.hostAddress = nullptr;

            // 1. geometry (mesh, triangle), step1, just to get the size
            VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
            accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
            // BLAS
            accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
            accelerationStructureBuildGeometryInfo.geometryCount = 1;
            accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

            auto numTriangles = static_cast<uint32_t>(mesh.indices.size() / 3);
            // fill in this structure, scratch buffer: pre-allocated memory
            VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
            accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
            vkGetAccelerationStructureBuildSizesKHR(
                logicalDevice,
                VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                &accelerationStructureBuildGeometryInfo,
                &numTriangles,
                &accelerationStructureBuildSizesInfo);

            // VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR specifies that the buffer is suitable for storage space for a VkAccelerationStructureKHR.
            // device local buffer
            // for build operation: requires two buffers. 1. for as, 2. for build op itself
            // for update operation:
            const auto blasBufferSizeInBytes = accelerationStructureBuildSizesInfo.accelerationStructureSize;
            const auto blasBuildBufferSizeInBytes = accelerationStructureBuildSizesInfo.buildScratchSize;
            const auto blasBuffer = _ctx->createDeviceLocalBuffer(
                "BLAS Buffer",
                blasBufferSizeInBytes,
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
            const auto blasBuildBuffer = _ctx->createDeviceLocalBuffer(
                "BLAS Buffer for build op",
                blasBuildBufferSizeInBytes,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

            // once the buffer is ready, you can create the actual AS
            // a bottom-level acceleration structure containing a set of triangles.
            VkAccelerationStructureKHR blasForTriangles;
            VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
            accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
            accelerationStructureCreateInfo.buffer = std::get<BUFFER_ENTITY_UID::BUFFER>(blasBuffer);
            accelerationStructureCreateInfo.size = blasBufferSizeInBytes;
            accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            vkCreateAccelerationStructureKHR(logicalDevice, &accelerationStructureCreateInfo, nullptr, &blasForTriangles);

            //step2: ready to build, blas here for the geometry (triangles)
            VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
            accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
            accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
            accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
            accelerationBuildGeometryInfo.dstAccelerationStructure = blasForTriangles;
            accelerationBuildGeometryInfo.geometryCount = 1;
            accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
            accelerationBuildGeometryInfo.scratchData.deviceAddress = std::get<BUFFER_ENTITY_UID::DEVICE_HOST_ADDRESS>(blasBuildBuffer).deviceAddress;

            VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
            accelerationStructureBuildRangeInfo.primitiveCount = numTriangles;
            accelerationStructureBuildRangeInfo.primitiveOffset = 0;
            accelerationStructureBuildRangeInfo.firstVertex = 0;
            accelerationStructureBuildRangeInfo.transformOffset = 0;
            std::vector<VkAccelerationStructureBuildRangeInfoKHR *> accelerationBuildStructureRangeInfos = {&accelerationStructureBuildRangeInfo};

            // submit command to gpu to build the blas
            const auto commandBufferToBuildAS = std::get<COMMAND_BUFFER_ENTITY_OFFSET::COMMAND_BUFFER>(cmdBuffersForIO);
            const auto fenceToBuildAS = std::get<COMMAND_BUFFER_ENTITY_OFFSET::FENCE>(cmdBuffersForIO);
            _ctx->BeginRecordCommandBuffer(cmdBuffersForIO);
            vkCmdBuildAccelerationStructuresKHR(commandBufferToBuildAS,
                                                1, &accelerationBuildGeometryInfo,
                                                accelerationBuildStructureRangeInfos.data());
            _ctx->EndRecordCommandBuffer(cmdBuffersForIO);

            // wait for no body
            VkSubmitInfo submitInfo{};
            submitInfo.waitSemaphoreCount = 0;
            submitInfo.pWaitSemaphores = VK_NULL_HANDLE;
            // only useful when having waitsemaphore
            submitInfo.pWaitDstStageMask = VK_NULL_HANDLE;
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBufferToBuildAS;
            // no body needs to be notified
            submitInfo.signalSemaphoreCount = 0;
            submitInfo.pSignalSemaphores = VK_NULL_HANDLE;

            VK_CHECK(vkResetFences(logicalDevice, 1, &fenceToBuildAS));
            VK_CHECK(vkQueueSubmit(graphicsComputeQueue, 1, &submitInfo, fenceToBuildAS));
            const auto result = vkWaitForFences(logicalDevice, 1, &fenceToBuildAS, VK_TRUE,
                                                100000000000);
            if (result == VK_TIMEOUT)
            {
                vkDeviceWaitIdle(logicalDevice);
            }

            // connection between blas and tlas
            // 64-bit address: which can be used for device and shader operations 
            VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
            accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
            accelerationDeviceAddressInfo.accelerationStructure = blasForTriangles;
            const VkDeviceAddress blasAddress = vkGetAccelerationStructureDeviceAddressKHR(logicalDevice, &accelerationDeviceAddressInfo);

            const auto blasAddress2 = std::get<BUFFER_ENTITY_UID::DEVICE_HOST_ADDRESS>(blasBuffer).deviceAddress;


            _blasEntity = std::make_tuple(blasBuffer, blasForTriangles, blasAddress);
            ++meshId;
        }
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

    // to build blas, it needs following:
    BufferEntity *_compositeVB;
    BufferEntity *_compositeIB;
    BufferEntity *_compositeMatB;
    BufferEntity *_indirectDrawB;

    std::tuple<BufferEntity, VkAccelerationStructureKHR, VkDeviceAddress> _blasEntity;
};