#define VK_NO_PROTOTYPES // for volk
#define VOLK_IMPLEMENTATION

 #include "volk.h"
#include <window.h>
#include <assert.h>
#include <iostream>
#include <format>
#include <vector>
#include <algorithm>
#include <iterator>
#include <numeric>
#include <array>
#include <filesystem> // for shader

// To do it properly:

// Include "vk_mem_alloc.h" file in each CPP file where you want to use the library. This includes declarations of all members of the library.
// In exactly one CPP file define following macro before this include. It enables also internal definitions.

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h> //c
#include <glslang/Public/ResourceLimits.h>    //c++
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>


#include <application.h>

bool gRunning = true;

int main(int argc, char **argv)
{
    // BoxTextured.glb
    Camera _camera{
        vec3f(std::array{0.f, 0.f, 6.f}),    // pos
        vec3f(std::array{0.f, 0.f, 0.f}),    // target -z
        vec3f(std::array{0.0f, 1.0f, 1.0f}), // initial world up
        0.0f,                                // initial pitch
        -97.f                                // initial yaw
    };

    WindowConfig cfg{
        1920,
        1080,
        "demo"s};

    Window window(&cfg, _camera);

    VK_CHECK(volkInitialize());
    // c api
    // glslang_initialize_process();
    glslang::InitializeProcess();

    VkApplication vkApp(window, _camera);
    vkApp.init();

   // init();

    while (gRunning)
    {
        vkApp.renderPerFrame();
        window.pollEvents();
    }

    vkApp.teardown();
    window.shutdown();
    // glslang_finalize_process();
    glslang::FinalizeProcess();
    return 0;
}