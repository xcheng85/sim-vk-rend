#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : require

/**
    rayPayloadInEXT                Closest-hit, any-hit and miss shader only. Storage associated
                                   with the incoming ray payload that can be read to and written
                                   in the stages invoked by traceRayEXT().
**/

/**
   hitAttributeEXT                Intersection, any-hit, and closest-hit shader only.
                                   Storage associated with attributes of geometry
                                   intersected by a ray.

    For the case of triangle geometry with no custom intersection shaders, any-hit and
    closest-hit shaders can access barycentric weights of the point of intersection of
    ray with triangle by declaring a hitAttributeEXT variable of two 32-bit floating
    point elements

    For example, (either of)
        hitAttributeEXT vec2 baryCoord;
        hitAttributeEXT block { vec2 baryCoord; }
        hitAttributeEXT float baryCoord[2];

**/

layout(location = 0) rayPayloadInEXT vec3 hitValue;
hitAttributeEXT vec2 baryCoord;

void main()
{
  const vec3 barycentricCoords = vec3(1.0f - baryCoord.x - baryCoord.y, baryCoord.x, baryCoord.y);
  hitValue = barycentricCoords;
}
