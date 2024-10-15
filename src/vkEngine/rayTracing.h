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
        auto rayGenerationShader = createShaderModule(
            logicalDevice,
            rayGenerationShaderPath,
            "main",
            "rayGeneration.rgen");
        // ray miss intersection
        const auto rayMissShaderPath = shadersPath + "/rayMiss.rmiss";
        auto rayMissShader = createShaderModule(
            logicalDevice,
            rayMissShaderPath,
            "main",
            "rayMiss.rmiss");
        // closet intersection
        const auto rayClosestHitShaderPath = shadersPath + "/rayClosestHit.rchit";
        auto rayClosestHitShader = createShaderModule(
            logicalDevice,
            rayClosestHitShaderPath,
            "main",
            "rayClosestHit.rchit");
    }

    void createDescriptorSetLayout()
    {
        ASSERT(_ctx, "vk context should be defined");
    }

    void initRTPipeline()
    {
    }

    void allocateDescriptorSets()
    {
        ASSERT(_ctx, "vk context should be defined");
        ASSERT(_dsPool, "descriptorset pool should be defined");
    }

    void createSBT()
    {
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
};