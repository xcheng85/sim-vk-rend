set(APP vkJulia)

find_package(Vulkan REQUIRED SPIRV-Tools)

enable_language(CUDA)

message(${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES})

include_directories(${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES}) 

file(GLOB_RECURSE SRC_FILES *.cpp CMAKE_CONFIGURE_DEPENDS)

add_executable(${APP} ${SRC_FILES})
target_include_directories(${APP} PUBLIC .)
target_include_directories(${APP} PUBLIC ${SDL2_INCLUDE_DIRS} )
target_link_libraries(${APP} ${Vulkan_LIBRARIES} volk_headers ${RequiredVulkanSDKLIBS} vkEngine cudaEngine)
target_compile_definitions(${APP} PUBLIC -DGLM_ENABLE_EXPERIMENTAL -DVK_DYNAMIC_RENDERING -DVK_PRERECORD_COMMANDS)