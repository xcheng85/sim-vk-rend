cmake_minimum_required(VERSION 3.29)
# C: for ktx
project(sim-vk-rend LANGUAGES CUDA C CXX VERSION 1.0.0 )
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

#include cmake script
include(CHECK_INSOURCE_BUILD)
include(BOOST)
include(CUDA)

# c++ version

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# following will break cuda23 cmake error
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CUDA_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DCMAKE_EXPORT_COMPILE_COMMANDS=1)

find_package(Vulkan REQUIRED SPIRV-Tools)

include(FetchContent)

# tag: vulkan-sdk-1.3.296.0
# build the glslang on the fly
if (VulkanHeaderVersion GREATER_EQUAL 275)
  message(STATUS "VulkanHeaderVersion: ${VulkanHeaderVersion}-${CMAKE_BUILD_TYPE}")
  FetchContent_Declare(glslang
      GIT_REPOSITORY https://github.com/KhronosGroup/glslang
      GIT_TAG vulkan-sdk-1.3.${VulkanHeaderVersion}.0)
  if (NOT glslang_POPULATED)
    set(ENABLE_OPT OFF)
  endif()
  set(GLSLANG_LIB "glslang")
  FetchContent_MakeAvailable(glslang)
else()
  find_package(Vulkan REQUIRED SPIRV-Tools glslang)
endif()

include_directories(${Vulkan_INCLUDE_DIR})
message(STATUS "Vulkan_INCLUDE_DIR: ${Vulkan_INCLUDE_DIR}")
link_directories(${Vulkan_INCLUDE_DIR}/../lib)

FetchContent_Declare(
  SDL2
  GIT_REPOSITORY "https://github.com/libsdl-org/SDL.git"
  GIT_TAG release-2.30.3
)
if(NOT SDL2_POPULATED)
  message(STATUS "SDL2: FetchContent_MakeAvailable")
  FetchContent_MakeAvailable(SDL2)
  message(STATUS "SDL2_INCLUDE_DIR: ${SDL2_INCLUDE_DIR}")
  include_directories(${SDL2_SOURCE_DIR}/include)
endif()

#  linux
set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_XLIB_KHR)
if (WIN32)
message(NOTICE "Fetching volk from https://github.com/zeux/volk.git ...")
set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
endif()

FetchContent_Declare(
        volk
        GIT_REPOSITORY https://github.com/zeux/volk.git
        GIT_TAG 1.3.270
)
if(NOT volk_POPULATED)
  FetchContent_MakeAvailable(volk)
  include_directories(${volk_SOURCE_DIR})
endif()

# v3.2.0 support windows handle which is required for cuda interop
FetchContent_Declare(
        vma
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG v3.2.0
)
if(NOT vma_POPULATED)
  FetchContent_MakeAvailable(vma)
  include_directories(${vma_SOURCE_DIR}/include)
endif()

# set(VMA_STATIC_VULKAN_FUNCTIONS OFF CACHE BOOL "" FORCE)
# set(VMA_DYNAMIC_VULKAN_FUNCTIONS ON CACHE BOOL "" FORCE)

FetchContent_Declare(
  gltfsdk
  GIT_REPOSITORY https://github.com/microsoft/glTF-SDK.git
  GIT_TAG        r1.9.6.0
)

if(NOT gltfsdk_POPULATED)
  set(ENABLE_UNIT_TESTS OFF CACHE BOOL "" FORCE)
  set(ENABLE_SAMPLES OFF CACHE BOOL "" FORCE)
  set(RAPIDJSON_BUILD_DOC OFF CACHE BOOL "" FORCE)
  set(RAPIDJSON_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(RAPIDJSON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  FetchContent_Populate(gltfsdk)
  message(${gltfsdk_SOURCE_DIR})
  add_subdirectory(${gltfsdk_SOURCE_DIR})
endif()

FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
)
if(NOT stb_POPULATED)
  FetchContent_Populate(stb)
  FetchContent_MakeAvailable(stb)
  message(${stb_SOURCE_DIR})
endif()

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 0.9.9.8
)
if(NOT glm_POPULATED)
  FetchContent_MakeAvailable(glm)
  message(${glm_SOURCE_DIR})
  include_directories(${glm_SOURCE_DIR})
endif()

FetchContent_Declare (
  tracy
  GIT_REPOSITORY https://github.com/wolfpld/tracy.git
  GIT_TAG v0.11.1
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)
if(NOT tracy_POPULATED)
  FetchContent_MakeAvailable(tracy)
  message(${tracy_SOURCE_DIR})
endif()

message(NOTICE "Fetching LibKTX from https://github.com/KhronosGroup/KTX-Software ...")
set(KTX_FEATURE_STATIC_LIBRARY ON CACHE BOOL "Build KTX as a static library" FORCE)
# gtest issue on windows
set(KTX_FEATURE_TESTS OFF)
FetchContent_Declare(
        fetch_ktx
        GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software
        GIT_TAG        v4.3.2
)
if(NOT fetch_ktx_POPULATED)
  FetchContent_MakeAvailable(fetch_ktx)
  message(${fetch_ktx_SOURCE_DIR})  
endif()

# FetchContent_Declare(
#   GPU_ENGINE_XC
#   GIT_REPOSITORY "https://github.com/xcheng85/gpu-engine-xc.git"
#   GIT_TAG        v2.0.0
# )
# FetchContent_MakeAvailable(GPU_ENGINE_XC)

# CMAKE_CURRENT_SOURCE_DIR: C:/Users/cheng/github.com/xcheng85/sim-vk-rend
message("CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")  
message("CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")

# if (NOT ANDROID)
# FetchContent_Declare(
#     imgui
#     GIT_REPOSITORY https://github.com/ocornut/imgui.git
#     GIT_TAG v1.91.6
# )
# FetchContent_MakeAvailable(tracy)

# add_Library(imgui STATIC
# ${imgui_SOURCE_DIR}/imgui.cpp
# ${imgui_SOURCE_DIR}/imgui_draw.cpp
# ${imgui_SOURCE_DIR}/imgui_demo.cpp
# ${imgui_SOURCE_DIR}/imgui_tables.cpp
# ${imgui_SOURCE_DIR}/imgui_widgets.cpp
# ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
# ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)
# # automatically include all the header files
# target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})
# target_compile_definitions(imgui PUBLIC IMGUI_IMPL_VULKAN_NO_PROTOTYPES)

# endif ()

# for experimental/future
#GCC provides experimental support for the upcoming ISO C++ standard, C++0x. 
#This support can be enabled with the -std=c++0x or -std=gnu++0x compiler options; 
#the former disables GNU extensions.
# set(CMAKE_CXX_FLAGS "-std=c++0x -lstdc++fs")


# only windows has postfix d
# if(WIN32)
#     set(CMAKE_DEBUG_POSTFIX "d")
#     add_definitions(-DGLSLANG_OSINCLUDE_WIN32)
# elseif(UNIX OR ANDROID)
#     add_definitions(-DGLSLANG_OSINCLUDE_UNIX)
# else()
#     message("unknown platform")
# endif()

if (WIN32)
set(RequiredVulkanSDKLIBS 
debug SDL2d optimized SDL2
debug SDL2maind optimized SDL2main
debug OSDependentd optimized OSDependent
debug MachineIndependentd optimized MachineIndependent
debug GenericCodeGend optimized GenericCodeGen
debug glslangd optimized glslang
debug SPIRVd optimized SPIRV
debug SPIRV-Toolsd optimized SPIRV-Tools
debug SPIRV-Tools-optd optimized SPIRV-Tools-opt
debug glslang-default-resource-limitsd optimized glslang-default-resource-limits
debug spirv-cross-cored optimized spirv-cross-core
debug spirv-cross-glsld optimized spirv-cross-glsl
debug spirv-cross-reflectd optimized spirv-cross-reflect)
else()
set(RequiredVulkanSDKLIBS 
SDL2::SDL2main 
SDL2::SDL2-static 
debug OSDependent 
debug MachineIndependent 
debug GenericCodeGen 
debug glslang 
debug SPIRV 
debug SPIRV-Tools 
debug SPIRV-Tools-opt 
debug glslang-default-resource-limits 
debug spirv-cross-core 
debug spirv-cross-glsl 
debug spirv-cross-reflect 
)
endif()

add_subdirectory(src bin)
# add_subdirectory(volk)
# add_subdirectory(VulkanMemoryAllocator)
# add_subdirectory(test)