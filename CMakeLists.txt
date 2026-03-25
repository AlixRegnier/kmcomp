cmake_minimum_required(VERSION 3.14)
project(BitmatrixShuffle VERSION 0.1.0 LANGUAGES CXX)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(cmake/avx.cmake)
check_for_avx2()

if(NOT HAS_AVX2)
    message(FATAL_ERROR "AVX2 support is required to compile this project.")
else()
    message(STATUS "AVX2 support detected.")
endif()

add_subdirectory(external)

add_library(bms_build_type_flags INTERFACE)
target_compile_options(bms_build_type_flags INTERFACE
  $<$<CONFIG:Debug>:-O -g -march=native -msse2 -mavx2>
  $<$<CONFIG:Release>:-O3 -DNDEBUG -march=native -msse2 -mavx2>
  $<$<CONFIG:Profile>:-O3 -ggdb3 -DNDEBUG -fno-inline -march=native -msse2 -mavx2>
  $<$<CONFIG:Coverage>:-O0 -g --coverage -march=native -msse2 -mavx2>
)

include_directories(
    ${PROJECT_SOURCE_DIR}/include
)

set(BMS_SRC_DIR ${PROJECT_SOURCE_DIR}/src)
set(BMS_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)

add_library(bitmatrixshuffle)
target_sources(bitmatrixshuffle PRIVATE
    ${BMS_SRC_DIR}/fast_median.cpp
    ${BMS_SRC_DIR}/rng.cpp
    ${BMS_SRC_DIR}/bitmatrixshuffle.cpp
    ${BMS_SRC_DIR}/tsp.cpp
)

target_link_libraries(bitmatrixshuffle PRIVATE bms_build_type_flags sdsl-lite zstd_ext json_ext block_compressor_ext)
target_include_directories(bitmatrixshuffle PUBLIC
    $<BUILD_INTERFACE:${BMS_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:include>
)
add_dependencies(bitmatrixshuffle block_compressor_ext json_ext)

if(BMS_BUILD_MAIN)
    add_executable(main_bitmatrixshuffle
        ${BMS_SRC_DIR}/main_bitmatrixshuffle.cpp
    )
    target_link_libraries(main_bitmatrixshuffle PRIVATE bms_build_type_flags)
    target_include_directories(main_bitmatrixshuffle PUBLIC
        $<BUILD_INTERFACE:${BMS_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(main_bitmatrixshuffle PRIVATE cxxopts_ext json_ext zstd_ext sdsl-lite block_compressor_ext bitmatrixshuffle)
    add_dependencies(main_bitmatrixshuffle bitmatrixshuffle block_compressor_ext cxxopts_ext json_ext )
endif()
