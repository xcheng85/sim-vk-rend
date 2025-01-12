set(LIB cudaEngine)
enable_language(CUDA)
find_package(CUDAToolkit)

file(GLOB BASE_SRC "*.cpp" "*.hpp" "*.h" "*.cu")
file(GLOB BASE_HEADERS "*.hpp" "*.h")
add_library(${LIB} ${BASE_SRC} ${BASE_HEADERS})

# under windows, Assuming you want separable compilation, you must enable resolving device symbols:
set_target_properties(${LIB}
                       PROPERTIES CUDA_SEPARABLE_COMPILATION ON
                       CUDA_ARCHITECTURES "90" # needed for ubuntu
                       CUDA_RESOLVE_DEVICE_SYMBOLS ON
                       )
target_compile_options(${LIB} PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:
                      --allow-unsupported-compiler
                      -G
                      -gencode arch=compute_90,code=sm_90
                      >)

# https://cmake.org/cmake/help/latest/module/FindCUDAToolkit.html
target_link_libraries(${LIB} PRIVATE CUDA::cudart     
  ${Vulkan_LIBRARIES} 
  volk )

target_include_directories(${LIB} PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${LIB}>"
)
