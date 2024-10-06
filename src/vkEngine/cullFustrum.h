#pragma once

#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include <misc.h>
#include <renderPassBase.h>

class CullFustrum : public RenderPassBase,
                    public VkContextAccessor,
                    public SceneAccessor,
                    public CameraAccessor
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

    virtual void setScene(std::shared_ptr<Scene> scene)
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

    virtual void setCamera(const CameraBase *camera)
    {
        _camera = camera;
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
        initFustrumBuffer();
        initMeshBoundingBoxBuffer();
        initCulledIndirectDrawBuffer();
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
            buffers.emplace_back(_ctx->createDeviceLocalBuffer(
                "Fustrum Buffer" + std::to_string(i),
                numFramesInFlight * sizeof(Fustrum),
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
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

    VkShaderModule _csShaderModule{VK_NULL_HANDLE};

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
};