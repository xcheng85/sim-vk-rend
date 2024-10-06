#pragma once

#include <assert.h>
#include <string>
#include <iostream>
#include <array>
#include <utility>
#include <any>
#if defined(__ANDROID__)
#include <vulkan/vulkan.h>
#endif

#ifndef __ANDROID__
#define VK_NO_PROTOTYPES // for volk
#include "volk.h"
#endif

#include <vector.h>
#include <matrix.h>
#include <quaternion.h>
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

enum Level
{
    Info,
    Warn,
    Error,
    Fatal
};
template <typename... ARGS>
void log(Level level, ARGS &&...args)
{
    static const char *levelText[] = {"INFO", "WARNING", "ERROR", "FATAL"};
    std::cerr << levelText[level] << ": ";
    (std::cerr << ... << args);
    std::cerr << '\n';
    if (level == Fatal)
        std::cerr << "Program aborted!";
}
#define ASSERT(expr, message) \
    {                         \
        void(message);        \
        assert(expr);         \
    }

#define VK_CHECK(x)                              \
    do                                           \
    {                                            \
        VkResult err = x;                        \
        if (err)                                 \
        {                                        \
            log(Error, "Detected Vulkan error"); \
            abort();                             \
        }                                        \
    } while (0)

std::string getAssetPath();

struct VertexDef1
{
    std::array<float, 3> pos;
    std::array<float, 2> uv;
    std::array<float, 3> normal;
};

struct VertexDef2
{
    std::array<float, 4> pos;
};

struct VertexDef3
{
    std::array<float, 3> pos;
    std::array<float, 2> uv;
    float material;
};

// corresponding to glsl definition
struct UniformDataDef0
{
    std::array<float, 16> mvp;
};

struct UniformDataDef1
{
    // mat4x4f projection;
    // mat4x4f modelView;
    // mat4x4f mvp;
    // vec3f viewPos;
    glm::mat4 mvp;
    glm::vec3 viewPos;
    float lodBias = 0.0f;
};

struct SpecializationDataDef1
{
    uint32_t lightingModel{0};
};

//// mimic vao in opengl
// struct VAO {
//     VkBuffer vertexBuffer;
//     VkBuffer indexBuffer;
// };

// IndirectDrawDef1: agonostic to graphics api
//
struct IndirectDrawForVulkan
{
    // VkDrawIndexedIndirectCommand vkDrawCmd;
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t vertexOffset;
    uint32_t firstInstance;
    uint32_t meshId;
    uint32_t materialIndex;
};

inline uint32_t getMipLevelsCount(uint32_t w, uint32_t h)
{
    return static_cast<uint32_t>(std::floor(std::log2(std::max(w, h)))) + 1;
}

template <size_t CHAIN_SIZE = 18>
class VkStructChain
{
public:
    VkStructChain() = default;
    VkStructChain(const VkStructChain &) = delete;
    VkStructChain &operator=(const VkStructChain &) = delete;
    VkStructChain(VkStructChain &&) noexcept = default;
    VkStructChain &operator=(VkStructChain &&) noexcept = default;

    auto &push(auto newVKStruct)
    {
        ASSERT(_currentIndex < CHAIN_SIZE, "VkFeatureChain is full");
        _vkStructs[_currentIndex] = newVKStruct;

        auto &newHeader = std::any_cast<decltype(newVKStruct) &>(_vkStructs[_currentIndex]);
        // pNext is vulkan thing
        // Replaces the value of obj with new_value and returns the old value of obj.
        newHeader.pNext = std::exchange(_header, &newHeader);
        _currentIndex++;

        return newHeader;
    }

    [[nodiscard]] void *header() const { return _header; };

private:
    std::array<std::any, CHAIN_SIZE> _vkStructs;
    int _currentIndex{0};
    void *_header{VK_NULL_HANDLE};
};

std::vector<char> readFile(const std::string &filePath, bool isBinary = true);

VkShaderModule createShaderModule(
    VkDevice logicalDevice,
    const std::string &filePath,
    const std::string &entryPoint,
    const std::string &correlationId);

std::vector<VkPipelineShaderStageCreateInfo> gatherPipelineShaderStageCreateInfos(const std::unordered_map<VkShaderStageFlagBits, std::tuple<VkShaderModule, const char *, const VkSpecializationInfo *>> &shaderModuleEntities);

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

VkImageViewType getImageViewType(VkImageType imageType);
uint32_t get2DImageSizeInBytes(VkExtent2D extent, VkFormat imageType);
uint32_t get3DImageSizeInBytes(VkExtent3D extent, VkFormat imageType);

inline vec2f screenSpace2Ndc(vec2i screenspaceCoord, vec2i screenDimension)
{
    return vec2f(std::array<float, 2>({static_cast<float>(screenspaceCoord[COMPONENT::X]) * 2.f / screenDimension[COMPONENT::X] - 1.f,
                                       1.f - 2.f * static_cast<float>(screenspaceCoord[COMPONENT::Y]) / screenDimension[COMPONENT::Y]}));
}

// math
using Plane = glm::vec4;
struct Fustrum
{
    static constexpr uint32_t sNumPlanes{6};
    alignas(16) glm::vec4 planes[sNumPlanes];
};

inline glm::vec3 calculatePlaneNormal(const glm::vec3 &p1, const glm::vec3 &p2, const glm::vec3 &p3)
{
    const auto e1 = p2 - p1;
    const auto e2 = p3 - p1;
    return glm::normalize(glm::cross(e1, e2));
}

inline Plane createPlaneFromPoints(const glm::vec3 &p1, const glm::vec3 &p2, const glm::vec3 &p3)
{
    // d = -n . p;
    const auto n = calculatePlaneNormal(p1, p2, p3);
    const auto d = -glm::dot(n, p1);
    return Plane(n, d);
}
