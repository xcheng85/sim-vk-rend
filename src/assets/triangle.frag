#version 460 // gl_BaseVertex and gl_DrawID
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_debug_printf : enable

// layout(location = 0) in vec4 inColor;
// layout(location = 0) out vec4 outColor;


layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outFragColor;

void main() {
  outFragColor = vec4(1, 0, 0, 1);
}
