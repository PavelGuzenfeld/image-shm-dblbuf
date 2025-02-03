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
find_package(single-task-runner REQUIRED)
find_package(double-buffer-swapper REQUIRED)
find_package(flat-type REQUIRED)

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
    enable_sanitizers(Share_memory_image_producer_consumer)
else()
    set_release_options(Share_memory_image_producer_consumer)
endif()

install(TARGETS Share_memory_image_producer_consumer
    DESTINATION lib/python${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}/site-packages
)

# -------------------------------
# Nanobind module for C++ bindings (shared memory)
nanobind_add_module(Share_memory_image_producer_consumer_nb
    NB_STATIC
    src/flat_shm_img_producer_consumer_nb.cpp
)

target_include_directories(Share_memory_image_producer_consumer_nb PRIVATE include)
target_link_libraries(Share_memory_image_producer_consumer_nb PRIVATE fmt single-task-runner::single-task-runner double-buffer-swapper::double-buffer-swapper)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set_debug_options(Share_memory_image_producer_consumer_nb)
    enable_sanitizers(Share_memory_image_producer_consumer_nb)
else()
    set_release_options(Share_memory_image_producer_consumer_nb)
endif()

install(TARGETS Share_memory_image_producer_consumer_nb DESTINATION lib/python${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}/site-packages)


# # -------------------------------
# Test executable
add_executable(flat_shm_test test/flat_shm_test.cpp)
target_include_directories(flat_shm_test PRIVATE include)
target_link_libraries(flat_shm_test PRIVATE fmt flat-type::flat-type)
set_debug_options(flat_shm_test)
enable_sanitizers(flat_shm_test)
install(TARGETS flat_shm_test DESTINATION bin)


