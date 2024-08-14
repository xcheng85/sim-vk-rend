#pragma once

#include <vector>
#include <numeric>
#include <stb_image.h>
#include <vector.h>
#include <matrix.h>
#include <misc.h>
// mimic
//struct VkDrawIndexedIndirectCommand {
//    uint32_t    indexCount;
//    uint32_t    instanceCount;
//    uint32_t    firstIndex;
//    int32_t     vertexOffset;
//    uint32_t    firstInstance;
//};

struct IndirectDrawDef1 {
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    uint32_t vertexOffset;
    uint32_t firstInstance;
    uint32_t meshId;
    int materialIndex;
};

//
//struct Vertex {
//    vec3f pos;
//    vec2f texCoord;
//    uint32_t material;
//
//    void transform(const mat4x4f &m) {
//        auto newp = MatrixMultiplyVector4x4(m, vec4f(std::array<float, 4>{
//                pos[COMPONENT::X],
//                pos[COMPONENT::Y],
//                pos[COMPONENT::Z],
//                1.0
//        }));
//
//        pos = vec3f(std::array{
//                newp[COMPONENT::X],
//                newp[COMPONENT::Y],
//                newp[COMPONENT::Z]
//        });
//    }
//};

struct Vertex {
    float vx;
    float vy;
    float vz;
    float ux;
    float uy;
    uint32_t material;

    void transform(const mat4x4f &m) {
        auto newp = MatrixMultiplyVector4x4(m, vec4f(std::array<float, 4>{
                vx,
                vy,
                vz,
                1.0
        }));

        vx = newp[COMPONENT::X];
        vy = newp[COMPONENT::Y];
        vz = newp[COMPONENT::Z];
    }
};

struct Mesh {
    std::vector<Vertex> vertices{};
    std::vector<uint32_t> indices{};
    int32_t materialIdx{-1};

    vec3f minAABB{std::array{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                             std::numeric_limits<float>::max()}};
    vec3f maxAABB{std::array{-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(),
                             -std::numeric_limits<float>::max()}};
    vec3f extents;
    vec3f center;
};

// https://github.com/KhronosGroup/glTF/blob/2.0/specification/2.0/schema/material.schema.json
// struct Material : glTFChildOfRootProperty
// struct PBRMetallicRoughness : glTFProperty
struct Material {
    // refer to PBRMetallicRoughness
    int basecolorTextureId{-1};
    int basecolorSamplerId{-1};
    int metallicRoughnessTextureId{-1};
    vec4f basecolor;
};

struct Texture {
    Texture(const std::vector<uint8_t> &rawBuffer);

//    {
//        LOGI("rawBuffer Size: %d", rawBuffer.size());
//        ktxResult result = ktxTexture_CreateFromMemory(rawBuffer.data(), rawBuffer.size(),
//                                                       KTX_TEXTURE_CREATE_NO_FLAGS,
//                                             &ktxTexture);
//        ASSERT(result == KTX_SUCCESS, "ktxTexture_CreateFromMemory failed");
//    }
    ~Texture();
//    {
//        ktxTexture_Destroy(ktxTexture);
//    }
    //ktxTexture *ktxTexture{nullptr};

    void *data{nullptr};
    int width{0};
    int height{0};
    int channels{0};
};

struct Scene {
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<std::unique_ptr<Texture>> textures;
    std::vector<IndirectDrawDef1> indirectDraw;
    uint32_t totalVerticesByteSize{0};
    uint32_t totalIndexByteSize{0};
};