find_package(Vulkan REQUIRED SPIRV-Tools)

file(GLOB_RECURSE SRC_FILES *.cpp CMAKE_CONFIGURE_DEPENDS)

add_executable(vk1 ${SRC_FILES})
target_include_directories(vk1 PUBLIC .)
target_include_directories(vk1 PUBLIC ${SDL2_INCLUDE_DIRS} )
target_link_libraries(vk1 ${Vulkan_LIBRARIES} volk_headers ${RequiredVulkanSDKLIBS} vkEngine)

target_compile_definitions(vk1 PUBLIC -DGLM_ENABLE_EXPERIMENTAL -DVK_DYNAMIC_RENDERING)