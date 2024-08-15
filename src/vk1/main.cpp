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
double gDt{0};
uint64_t gLastFrame{0};

int main(int argc, char **argv)
{
    // BoxTextured.glb
    // Camera _camera{
    //     vec3f(std::array{0.f, 0.f, 6.f}),    // pos
    //     vec3f(std::array{0.f, 0.f, 0.f}),    // target -z
    //     vec3f(std::array{0.0f, 1.0f, 1.0f}), // initial world up
    //     0.0f,                                // initial pitch
    //     -97.f                                // initial yaw
    // };

    // // Avocado.glb
    // Camera _camera{
    //     vec3f(std::array{0.0f, 0.031400f, 0.1f}),           // pos
    //     vec3f(std::array{0.000000f, 0.031400f, 0.000000f}), // target -z
    //     vec3f(std::array{0.0f, 1.0f, 0.0f}),                // initial world up
    //     0.0f,                                               // initial pitch
    //     -97.f                                               // initial yaw
    // };

    // // Duck.glb
    // Camera _camera{
    //     vec3f(std::array{13.440697f, 86.949684f, 400.0f}),     // pos
    //     vec3f(std::array{13.440697f, 86.949684f, -3.701500f}), // target -z
    //     vec3f(std::array{0.0f, 1.0f, 0.0f}),                   // initial world up
    //     0.0f,                                                  // initial pitch
    //     -97.f                                                  // initial yaw
    // };

    // // AnisotropyBarnLamp
    // Camera _camera{
    //     vec3f(std::array{-0.009215f, -0.057677f, 1.f}),       // pos
    //     vec3f(std::array{-0.009215f, -0.057677f, 0.113268f}), // target -z
    //     vec3f(std::array{0.0f, 1.0f, 0.0f}),                  // initial world up
    //     0.0f,                                                 // initial pitch
    //     -90.f                                                 // initial yaw
    // };

    // // AntiqueCamera
    // Camera _camera{
    //     vec3f(std::array{-0.327268f, -2.70912f, 5.f}),      // pos
    //     vec3f(std::array{-0.327268f, -2.70912f, 0.19766f}), // target -z
    //     vec3f(std::array{0.0f, 1.0f, 0.0f}),                 // initial world up
    //     0.0f,                                                // initial pitch
    //     -90.f                                                // initial yaw
    // };

    // // duck
    // Camera _camera{
    //     vec3f(std::array{13.440697f, 86.949684f, 200.f}),      // pos
    //     vec3f(std::array{13.440697f, 86.949684f, -3.701500f}), // target -z
    //     vec3f(std::array{0.0f, 1.0f, 0.0f}),                   // initial world up
    //     0.0f,                                                  // initial pitch
    //     -90.f                                                  // initial yaw
    // };

    // ////// MaterialsVariantsShoe.glb
    // Camera _camera{
    //     vec3f(std::array{0.0f, 0.f, 7.f}),   // pos
    //     vec3f(std::array{0.0f, 0.f, 0.f}),   // target -z
    //     vec3f(std::array{0.0f, 1.0f, 0.0f}), // initial world up
    //     0.0f,                                // initial pitch
    //     -90.f                                // initial yaw
    // };

    //// BoomBox
    // Camera _camera{
    //     vec3f(std::array{0.0f, 0.0f, 0.1f}),             // pos
    //     vec3f(std::array{0.000000f, 0.000f, 0.000000f}), // target -z
    //     vec3f(std::array{0.0f, 1.0f, 0.0f}),             // initial world up
    //     0.0f,                                            // initial pitch
    //     -97.f                                            // initial yaw
    // };

    // BarramundiFish
    Camera _camera{
        vec3f(std::array{0.00238983f, -0.142875f, 0.6f}),        // pos
        vec3f(std::array{0.00238983f, -0.142875f, 0.00381729f}), // target -z
        vec3f(std::array{0.0f, 0.0f, 1.0f}),                     // initial world up
        0.0f,                                                    // initial pitch
        -97.f                                                    // initial yaw
    };

    WindowConfig cfg{1920, 1080, "demo"s};

    Window window(&cfg, _camera);

    VK_CHECK(volkInitialize());
    // c api
    // glslang_initialize_process();
    glslang::InitializeProcess();

    VkApplication vkApp(window, _camera, "BarramundiFish.glb"s);
    vkApp.init();

    // init();
    gLastFrame = SDL_GetPerformanceCounter();
    // gLastFrame = SDL_GetTicks();
    while (gRunning)
    {
        auto currTime = SDL_GetPerformanceCounter();
        // auto currTime = SDL_GetTicks();
        // gDt = (currTime - gLastFrame) / 1000.0; // Convert to seconds.
        //  gDt = currTime - gLastFrame;
        gDt = static_cast<double>(
            (currTime - gLastFrame) / static_cast<double>(SDL_GetPerformanceFrequency()));
        gLastFrame = currTime;

        vkApp.renderPerFrame();
        window.pollEvents();
    }

    vkApp.teardown();
    window.shutdown();
    // glslang_finalize_process();
    glslang::FinalizeProcess();
    return 0;
}