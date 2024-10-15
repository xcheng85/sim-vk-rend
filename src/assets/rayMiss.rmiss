#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : require

/**
    rayPayloadInEXT                Closest-hit, any-hit and miss shader only. Storage associated
                                   with the incoming ray payload that can be read to and written
                                   in the stages invoked by traceRayEXT().
**/


layout(location = 0) rayPayloadInEXT vec3 hitValue;

void main()
{
    hitValue = vec3(0.0, 0.0, 0.2);
}