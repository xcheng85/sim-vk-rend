set(APP vkJulia)

find_package(Vulkan REQUIRED SPIRV-Tools)

enable_language(CUDA)

message(${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES})

# message("Julia: CMAKE_CURRENT_SOURCE_DIR='${CMAKE_CURRENT_SOURCE_DIR}'")
# message("Julia: CMAKE_SOURCE_DIR='${CMAKE_SOURCE_DIR}'")
# # Julia: CMAKE_CURRENT_SOURCE_DIR='C:/Users/cheng/github.com/xcheng85/sim-vk-rend/src/vkJulia'
# # Julia: CMAKE_SOURCE_DIR='C:/Users/cheng/github.com/xcheng85/sim-vk-rend'
# include_directories(
#     ${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES}
#     ${CMAKE_SOURCE_DIR}/../webrtc-checkout/src
#     ${CMAKE_SOURCE_DIR}/../webrtc-checkout/src/third_party/abseil-cpp
#     ${CMAKE_SOURCE_DIR}/../webrtc-checkout/src/third_party/libyuv/include
#     ) 

file(GLOB_RECURSE SRC_FILES *.cpp CMAKE_CONFIGURE_DEPENDS)

add_executable(${APP} ${SRC_FILES})
target_include_directories(${APP} PUBLIC .)
target_include_directories(${APP} PUBLIC ${SDL2_INCLUDE_DIRS} )
target_link_libraries(${APP} 
    ${Vulkan_LIBRARIES} 
    volk_headers 
    ${RequiredVulkanSDKLIBS} 
    vkEngine 
    cudaEngine
    #${CMAKE_SOURCE_DIR}/../webrtc-checkout/src/out/Debug/obj/webrtc.lib
)

target_compile_definitions(${APP} PUBLIC 
-DGLM_ENABLE_EXPERIMENTAL 
-DVK_DYNAMIC_RENDERING 
-DVK_PRERECORD_COMMANDS 
-DWEBRTC_WIN=1
-DWIN32_LEAN_AND_MEAN
-DNOMINMAX
-D__STDC_CONSTANT_MACROS=1 
)


get_target_property(dirs ${APP} INTERFACE_INCLUDE_DIRECTORIES)
foreach(dir IN LISTS dirs)
    message(STATUS "dir='${dir}'")
endforeach()