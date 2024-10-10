#pragma once

#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include <misc.h>
#include <renderPassBase.h>

class CullFustrum : public RenderPassBase,
                    public VkContextAccessor,
                    public SceneAccessor,
                    public CameraAccessor,
                    public DescriptorPoolAccessor
{
public:
    CullFustrum()
    {
    }

    ~CullFustrum()
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
    // indirectDrawBuffer to be cull against
    inline void setIndirectDrawBuffer(BufferEntity *idb)
    {
        _indirectDrawBuffer = idb;
    }

    virtual void finalizeInit() override
    {
        initShaderModules();
        createDescriptorSetLayout();
        initComputePipeline();
        allocateDescriptorSets();

        initFustrumBuffer();
        initMeshBoundingBoxBuffer();
        initCulledIndirectDrawBuffer();

        bindResourceToDescriptorSets();
    }

    virtual void execute(CommandBufferEntity cmd, int frameIndex) override
    {
    }

private:
    void initShaderModules()
    {
        ASSERT(_ctx, "vk context should be defined");
        auto logicalDevice = _ctx->getLogicDevice();
        const auto shadersPath = getAssetPath();
        const auto computeShaderPath = shadersPath + "/cullFustrum.comp";
        _csShaderModule = createShaderModule(
            logicalDevice,
            computeShaderPath,
            "main",
            "cullFustrum.comp");
    }

    void initFustrumBuffer()
    {
        ASSERT(_ctx, "vk context should be defined");
        const auto numFramesInFlight = _ctx->getSwapChainImageViews().size();

        // sizeof(Fustrum) * numFramesInFlight
        std::vector<BufferEntity> buffers;
        buffers.reserve(numFramesInFlight);

        for (size_t i = 0; i < numFramesInFlight; ++i)
        {
            buffers.emplace_back(_ctx->createPersistentBuffer(
                "Uniform Fustrum Buffer" + std::to_string(i),
                numFramesInFlight * sizeof(Fustrum),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));
        }

        _fustrumBuffers = std::make_tuple(buffers, numFramesInFlight);
    }

    void initMeshBoundingBoxBuffer()
    {
        ASSERT(_ctx, "vk context should be defined");
        ASSERT(_scene, "scene should be defined");
        std::vector<BoundingBox> bb;
        bb.reserve(_scene->meshes.size());

        for (const auto &mesh : _scene->meshes)
        {
            bb.emplace_back(BoundingBox{
                .center = glm::vec4(
                    mesh.center[COMPONENT::X],
                    mesh.center[COMPONENT::Y],
                    mesh.center[COMPONENT::Z],
                    1.0f),
                .extents = glm::vec4(
                    mesh.extents[COMPONENT::X],
                    mesh.extents[COMPONENT::Y],
                    mesh.extents[COMPONENT::Z],
                    1.0f)});
        }
        // build up the combo buffer
        const auto bytesize = sizeof(BoundingBox) * bb.size();
        _meshBoundBoxComboBuffer = _ctx->createDeviceLocalBuffer(
            "Combo BoundingBox Buffer",
            bytesize,
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    }

    void initCulledIndirectDrawBuffer()
    {
        ASSERT(_ctx, "vk context should be defined");
        ASSERT(_indirectDrawBuffer, "indirect draw buffer should be defined");
        const auto bufferSizeInBytes = std::get<4>(*_indirectDrawBuffer);

        // VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT: specifies that the buffer can be used to retrieve a buffer device address via vkGetBufferDeviceAddress
        // and use that address to access the buffer’s memory from a shader.
        // VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT must be provided
        _culledIndirectDrawBuffer = _ctx->createDeviceLocalBuffer(
            "Culled Indirect Draw Buffer",
            bufferSizeInBytes,
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);

        _culledIndirectDrawCountBuffer = _ctx->createDeviceLocalBuffer(
            "Culled Indirect Draw Counter Buffer",
            sizeof(uint32_t),
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
    }

    // refer to section in cs
    // #define IDR_SETID 0
    // #define BOUNDINGBOX_SETID 1
    // #define FUSTRUMS_SETID 2
    // #define CULLED_IDR 3
    // #define CULLED_IDR_COUNTER 4

    enum DESC_LAYOUT_SEMANTIC : int
    {
        IDR = 0,
        BOUNDING_BOX,
        FUSTRUMS,
        CULLED_IDR,
        CULLED_IDR_COUNTER,
        DESC_LAYOUT_SEMANTIC_SIZE
    };

    void createDescriptorSetLayout()
    {
        ASSERT(_ctx, "vk context should be defined");
        std::vector<std::vector<VkDescriptorSetLayoutBinding>> setBindings(DESC_LAYOUT_SEMANTIC_SIZE);

        setBindings[DESC_LAYOUT_SEMANTIC::IDR].resize(1);
        setBindings[DESC_LAYOUT_SEMANTIC::IDR][0].binding = 0; // depends on the shader: set 0, binding = 0
        setBindings[DESC_LAYOUT_SEMANTIC::IDR][0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        setBindings[DESC_LAYOUT_SEMANTIC::IDR][0].descriptorCount = 1;
        setBindings[DESC_LAYOUT_SEMANTIC::IDR][0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        setBindings[DESC_LAYOUT_SEMANTIC::BOUNDING_BOX].resize(1);
        setBindings[DESC_LAYOUT_SEMANTIC::BOUNDING_BOX][0].binding = 0; // depends on the shader: set 0, binding = 0
        setBindings[DESC_LAYOUT_SEMANTIC::BOUNDING_BOX][0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        setBindings[DESC_LAYOUT_SEMANTIC::BOUNDING_BOX][0].descriptorCount = 1;
        setBindings[DESC_LAYOUT_SEMANTIC::BOUNDING_BOX][0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        setBindings[DESC_LAYOUT_SEMANTIC::FUSTRUMS].resize(1);
        setBindings[DESC_LAYOUT_SEMANTIC::FUSTRUMS][0].binding = 0; // depends on the shader: set 0, binding = 0
        setBindings[DESC_LAYOUT_SEMANTIC::FUSTRUMS][0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        setBindings[DESC_LAYOUT_SEMANTIC::FUSTRUMS][0].descriptorCount = 1;
        setBindings[DESC_LAYOUT_SEMANTIC::FUSTRUMS][0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        setBindings[DESC_LAYOUT_SEMANTIC::CULLED_IDR].resize(1);
        setBindings[DESC_LAYOUT_SEMANTIC::CULLED_IDR][0].binding = 0; // depends on the shader: set 0, binding = 0
        setBindings[DESC_LAYOUT_SEMANTIC::CULLED_IDR][0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        setBindings[DESC_LAYOUT_SEMANTIC::CULLED_IDR][0].descriptorCount = 1;
        setBindings[DESC_LAYOUT_SEMANTIC::CULLED_IDR][0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        setBindings[DESC_LAYOUT_SEMANTIC::CULLED_IDR_COUNTER].resize(1);
        setBindings[DESC_LAYOUT_SEMANTIC::CULLED_IDR_COUNTER][0].binding = 0; // depends on the shader: set 0, binding = 0
        setBindings[DESC_LAYOUT_SEMANTIC::CULLED_IDR_COUNTER][0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        setBindings[DESC_LAYOUT_SEMANTIC::CULLED_IDR_COUNTER][0].descriptorCount = 1;
        setBindings[DESC_LAYOUT_SEMANTIC::CULLED_IDR_COUNTER][0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        _descriptorSetLayouts = _ctx->createDescriptorSetLayout(setBindings);
    }

    void initComputePipeline()
    {
        const std::string entryPoint{"main"s};
        // layout(push_constant) uniform PushConsts {
        // 	uint count;
        // } MeshesToCull;
        _computePipelineEntity = _ctx->createComputePipeline(
            {{VK_SHADER_STAGE_COMPUTE_BIT,
              make_tuple(_csShaderModule, entryPoint.c_str(), nullptr)}},
            _descriptorSetLayouts,
            {{
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                .offset = 0,
                .size = sizeof(uint32_t),
            }});
    }

    void allocateDescriptorSets()
    {
        ASSERT(_ctx, "vk context should be defined");
        ASSERT(_dsPool, "descriptorset pool should be defined");
        const auto numFramesInFlight = _ctx->getSwapChainImageViews().size();
        _descriptorSets = _ctx->allocateDescriptorSet(_dsPool,
                                                      {{&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::IDR],
                                                        1},
                                                       {&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::BOUNDING_BOX],
                                                        1},
                                                       {&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::FUSTRUMS],
                                                        numFramesInFlight},
                                                       {&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::CULLED_IDR],
                                                        1},
                                                       {&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::CULLED_IDR_COUNTER],
                                                        1}});
    }

    void bindResourceToDescriptorSets()
    {
        ASSERT(_ctx, "vk context should be defined");
        auto logicalDevice = _ctx->getLogicDevice();
        const auto numFramesInFlight = _ctx->getSwapChainImageViews().size();

        // idr as input (readonly)
        {
            const auto &dstSets = _descriptorSets[&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::IDR]];
            ASSERT(dstSets.size() == 1, "IDR descriptor set size is 1");
            const auto bufferSizeInBytes = std::get<4>(*_indirectDrawBuffer);
            _ctx->bindBufferToDescriptorSet(
                std::get<0>(*_indirectDrawBuffer),
                0,
                bufferSizeInBytes,
                dstSets[0],
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                0);
        }

        // boundingbox buffer
        {
            const auto &dstSets = _descriptorSets[&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::BOUNDING_BOX]];
            ASSERT(dstSets.size() == 1, "boundingbox descriptor set size is 1");
            const auto bufferSizeInBytes = std::get<4>(_meshBoundBoxComboBuffer);
            _ctx->bindBufferToDescriptorSet(
                std::get<0>(_meshBoundBoxComboBuffer),
                0,
                bufferSizeInBytes,
                dstSets[0],
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                0);
        }

        // fustrum uniform buffer
        {
            const auto &dstSets = _descriptorSets[&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::FUSTRUMS]];
            ASSERT(dstSets.size() == numFramesInFlight, "FUSTRUMS descriptor set size should equal # of frames in flight");
            const auto& fustrumBuffers = std::get<0>(_fustrumBuffers);
            ASSERT(fustrumBuffers.size() == numFramesInFlight, "fustrumBuffers' size should equal # of frames in flight");
            
            for (size_t i = 0; i < numFramesInFlight; i++)
            {
                const auto bufferSizeInBytes = std::get<4>(fustrumBuffers[i]);
                _ctx->bindBufferToDescriptorSet(
                    std::get<0>(fustrumBuffers[i]),
                    0,
                    bufferSizeInBytes,
                    _descriptorSets[&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::FUSTRUMS]][i],
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    0);
            }
        }

        // culled idr buffer (writable)
        {
            const auto &dstSets = _descriptorSets[&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::CULLED_IDR]];
            ASSERT(dstSets.size() == 1, "culled idr descriptor set size is 1");
            const auto bufferSizeInBytes = std::get<4>(_culledIndirectDrawBuffer);
            _ctx->bindBufferToDescriptorSet(
                std::get<0>(_culledIndirectDrawBuffer),
                0,
                bufferSizeInBytes,
                dstSets[0],
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                0);
        }

        // culled idr counter buffer (writable)
        {
            const auto &dstSets = _descriptorSets[&_descriptorSetLayouts[DESC_LAYOUT_SEMANTIC::CULLED_IDR_COUNTER]];
            ASSERT(dstSets.size() == 1, "culled idr counter descriptor set size is 1");
            const auto bufferSizeInBytes = std::get<4>(_culledIndirectDrawCountBuffer);
            _ctx->bindBufferToDescriptorSet(
                std::get<0>(_culledIndirectDrawCountBuffer),
                0,
                bufferSizeInBytes,
                dstSets[0],
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                0);
        }
    }

    // ownership be careful
    BufferEntity *_indirectDrawBuffer{nullptr};
    BufferEntity _culledIndirectDrawBuffer;
    // vkCmdDrawIndexedIndirectCount vs vkCmdDrawIndexedIndirect
    // vkCmdDrawIndexedIndirectCount: extra buffer for draw counter, which is filled in in the gpu
    BufferEntity _culledIndirectDrawCountBuffer;
    // refer to frame in fight
    std::tuple<std::vector<BufferEntity>, size_t> _fustrumBuffers;
    // interleave all the bounding box of meshes into one big buffer.
    BufferEntity _meshBoundBoxComboBuffer;

    // for pipeline and binding resource
    std::vector<VkDescriptorSetLayout> _descriptorSetLayouts;
    VkShaderModule _csShaderModule{VK_NULL_HANDLE};
    std::tuple<VkPipeline, VkPipelineLayout> _computePipelineEntity;
    // do i use individual ds pool ?
    std::unordered_map<VkDescriptorSetLayout *, std::vector<VkDescriptorSet>> _descriptorSets;
};