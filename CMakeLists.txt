cmake_minimum_required(VERSION 3.29)
project(sim-vk-rend LANGUAGES CUDA CXX VERSION 1.0.0 )
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

#include cmake script
include(CHECK_INSOURCE_BUILD)
# include(BOOST)
# include(CUDA)

# c++ version

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED SPIRV-Tools)

include(FetchContent)

if (VulkanHeaderVersion GREATER_EQUAL 275)
  FetchContent_Declare(glslang
      GIT_REPOSITORY https://github.com/KhronosGroup/glslang
      GIT_TAG vulkan-sdk-1.3.${VulkanHeaderVersion}.0)
  if (NOT glslang_POPULATED)
    set(ENABLE_OPT OFF)
  endif()
  set(GLSLANG_LIB "glslang") 
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
FetchContent_MakeAvailable(SDL2)

FetchContent_Declare(
        volk
        GIT_REPOSITORY https://github.com/zeux/volk.git
        GIT_TAG 1.3.270
)
FetchContent_MakeAvailable(volk)

FetchContent_Declare(
        vma
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG v3.0.1
)
FetchContent_MakeAvailable(vma)
include_directories(${SDL2_SOURCE_DIR}/include)
include_directories(${vma_SOURCE_DIR}/include)
include_directories(${volk_SOURCE_DIR})

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
endif()
message(${gltfsdk_SOURCE_DIR})
add_subdirectory(${gltfsdk_SOURCE_DIR})

FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
)
FetchContent_Populate(stb)
FetchContent_MakeAvailable(stb)


FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 0.9.9.8
)
FetchContent_MakeAvailable(glm)
include_directories(${glm_SOURCE_DIR})

# for experimental/future
#GCC provides experimental support for the upcoming ISO C++ standard, C++0x. 
#This support can be enabled with the -std=c++0x or -std=gnu++0x compiler options; 
#the former disables GNU extensions.
# set(CMAKE_CXX_FLAGS "-std=c++0x -lstdc++fs")

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

add_subdirectory(src bin)
# add_subdirectory(volk)
# add_subdirectory(VulkanMemoryAllocator)
# add_subdirectory(test)