#version 460 // gl_BaseVertex and gl_DrawID
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "common.glsl"

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in flat uint inMeshId;
layout(location = 2) in flat int inMaterialId;

layout(location = 0) out vec4 outFragColor;

layout(constant_id = 0) const int LIGHTING_MODEL = 0;

void main()
{
  int basecolorTextureId = 0;
  int basecolorSamplerId = 0;

  if (inMaterialId != -1) {
    Material mat = materials[inMaterialId];
    basecolorTextureId = mat.basecolorTextureId;
    basecolorSamplerId = mat.basecolorSamplerId;
  }

  // -1 is from glb io reader
  if (basecolorTextureId != -1) {
    switch (LIGHTING_MODEL) {
      case 0: // texture			
      {
        // create the sampler2D first
        outFragColor = texture(sampler2D(BindlessImage2D[basecolorTextureId], BindlessSampler), inTexCoord);
        break;
      }
      case 1: // flat			
      {
        outFragColor = vec4(0.5, 0.5, 0.5, 1.0);
        break;
      }
    }
  } else {
    outFragColor = vec4(0.5, .5, 0.5, 1.0);
  }
}
