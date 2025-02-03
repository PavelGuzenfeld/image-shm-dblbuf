cmake_minimum_required(VERSION 3.20)
project(image-shm-dblbuf)

# Require C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find dependencies
find_package(cmake-library REQUIRED)
find_package(fmt REQUIRED)
find_package(pybind11 REQUIRED)
find_package(Python3 COMPONENTS Interpreter Development)
find_package(Python REQUIRED COMPONENTS Interpreter Development)
find_package(nanobind CONFIG REQUIRED HINTS /usr/local/nanobind/cmake)

# Silence warnings in nanobind headers by marking as SYSTEM include
include_directories(SYSTEM /usr/local/nanobind/include)

# -------------------------------
# Nanobind module using high-level API
nanobind_add_module(Share_memory_image_producer_consumer
    NB_STATIC  # Build nanobind core as static library
    src/flat_shm_img_producer_consumer_py.cpp
)

target_include_directories(Share_memory_image_producer_consumer PRIVATE include)
target_link_libraries(Share_memory_image_producer_consumer PRIVATE fmt pybind11::module)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set_debug_options(Share_memory_image_producer_consumer)
else()
    set_release_options(Share_memory_image_producer_consumer)
endif()

install(TARGETS Share_memory_image_producer_consumer
    DESTINATION lib/python${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}/site-packages
)

# # -------------------------------
# # Nanobind module for C++ bindings (shared memory)
# nanobind_add_module(Share_memory_image_producer_consumer_nb
#     NB_STATIC
#     flat_shm_img_producer_consumer_nb.cpp
#     async_runner.cpp
# )

# target_link_libraries(Share_memory_image_producer_consumer_nb PRIVATE fmt)
# set_common_options(Share_memory_image_producer_consumer_nb)
# enable_sanitizers(Share_memory_image_producer_consumer_nb)

# install(TARGETS Share_memory_image_producer_consumer_nb DESTINATION lib/python${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}/site-packages)

# # -------------------------------
# # Nanobind module for general bindings
# nanobind_add_module(my_module NB_STATIC nanobind_example.cpp)
# set_target_properties(my_module PROPERTIES PREFIX "" SUFFIX ".so")
# install(TARGETS my_module DESTINATION lib/python${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}/site-packages)

# # -------------------------------
# # Test executable
# add_executable(flat_shm_test flat_shm_test.cpp)
# target_link_libraries(flat_shm_test PRIVATE fmt)
# set_common_options(flat_shm_test)
# enable_sanitizers(flat_shm_test)
# install(TARGETS flat_shm_test DESTINATION bin)

# add_executable(exception_test exception.cpp)
# target_link_libraries(exception_test PRIVATE fmt)
# set_common_options(exception_test)
# enable_sanitizers(exception_test)
# install(TARGETS exception_test DESTINATION bin)

# # -------------------------------
# # SIMD copy executable for ARM processors
# if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64")
#     add_executable(simd_copy simd_copy.cpp)
#     target_link_libraries(simd_copy)
#     set_common_options(simd_copy)
#     enable_sanitizers(simd_copy)
#     install(TARGETS simd_copy DESTINATION bin)
# endif()

# # -------------------------------
# # Double buffer swapper test
# add_executable(double_buffer_swapper_test double_buffer_swapper_test.cpp)
# target_link_libraries(double_buffer_swapper_test PRIVATE fmt)
# set_common_options(double_buffer_swapper_test)
# enable_sanitizers(double_buffer_swapper_test)
# install(TARGETS double_buffer_swapper_test DESTINATION bin)

# # -------------------------------
# # Async runner library and test
# add_library(async_runner async_runner.cpp)
# target_link_libraries(async_runner PRIVATE fmt)
# install(TARGETS async_runner DESTINATION lib)

# add_executable(async_runner_test async_runner_test.cpp)
# target_link_libraries(async_runner_test PRIVATE fmt async_runner)
# set_common_options(async_runner_test)
# enable_sanitizers(async_runner_test)
# install(TARGETS async_runner_test DESTINATION bin)
