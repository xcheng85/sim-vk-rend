#ifndef IDR_COMMON_GLSL
#define IDR_COMMON_GLSL

layout (set = 0, binding = 0) uniform UBO {
    //mat4 projection;
    //mat4 model;
    mat4 mvp;
    // camera
    vec3 viewPos;
    float lodBias;
} ubo;

layout(set = 1, binding = 0) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(set = 2, binding = 0) readonly buffer IndirectDrawBuffer {
    IndirectDrawDef1 indirectDraws[];
};
layout(set = 3, binding = 0) uniform texture2D BindlessImage2D[];
layout(set = 3, binding = 1) uniform sampler BindlessSampler;
layout(set = 4, binding = 0) readonly buffer MaterialBuffer {
    Material materials[];
};

#endif