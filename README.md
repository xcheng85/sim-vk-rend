# sim-vk-rend

## How to design architecture of misc rendering passes 

### What are the components of a rendering pass

1. shader modules
2. input
3. output

mixing class

## vulkan small tips 

https://docs.vulkan.org/guide/latest/push_constants.html

computing pipeline and graphics pipeline have independent resource binding

## push constant
https://docs.vulkan.org/guide/latest/push_constants.html#pc-shader-code

A small bank of values writable via the API and accessible in shaders. Push constants allow the application to set values used in shaders without creating buffers or modifying and binding descriptor sets for each update.

### update push constant

vkCmdPushConstants

### Specialization Constants VS push constant
specialization constants: are set before pipeline creation meaning these values are known during shader compilation,

push constant: update at runtime (after shader compilation)