#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_debug_printf : enable
#extension GL_GOOGLE_include_directive : require

// layout(location = 0) out vec4 outColor;

// // in ndc, no vao or ssbo
// // face culling: VK_CULL_MODE_BACK_BIT
// // clock wise
// vec2 positionsInNdc[3] = vec2[](vec2(0.0, -0.5), vec2(-0.5, 0.5), vec2(0.5, 0.5));
// vec3 colors[3] = vec3[](vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, 1.0));

layout (location = 0) out vec2 outUV;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main() {
//   gl_Position = vec4(positionsInNdc[gl_VertexIndex], 0.0, 1.0);
//   outColor = vec4(colors[gl_VertexIndex], 1.0);

//     rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
//    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
// .yx 
	outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(outUV.yx * 2.0f - 1.0f, 0.0f, 1.0f);
}
