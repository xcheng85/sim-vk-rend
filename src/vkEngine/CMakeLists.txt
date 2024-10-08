MESSAGE(STATUS "${PLATFORM}")

file(GLOB BASE_SRC "*.cpp" "*.hpp" "*.h")
file(GLOB BASE_HEADERS "*.hpp" "*.h")

add_library(vkEngine ${BASE_SRC} ${BASE_HEADERS})
target_include_directories(vkEngine PUBLIC .)

message(${stb_SOURCE_DIR})

target_link_libraries(
    vkEngine
    GLTFSDK
    ${Vulkan_LIBRARIES} volk_headers ${RequiredVulkanSDKLIBS}
)

#glm
target_compile_definitions(vkEngine PUBLIC -DGLM_ENABLE_EXPERIMENTAL)

target_include_directories(vkEngine PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/core/utility>"
  ${gltfsdk_SOURCE_DIR}/GLTFSDK/Inc ${stb_SOURCE_DIR}
)

get_target_property(dirs vkEngine INTERFACE_INCLUDE_DIRECTORIES)
foreach(dir IN LISTS dirs)
    message(STATUS "dir='${dir}'")
endforeach()