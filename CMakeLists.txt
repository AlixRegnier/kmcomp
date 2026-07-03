cmake_minimum_required(VERSION 3.14)
project(kmcomp VERSION 1.0.0 LANGUAGES CXX)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" KMCOMP_ARCH)

if(KMCOMP_ARCH MATCHES "^(x86_64|amd64|i386|i686)$")
    include(cmake/avx.cmake)
    check_for_avx2()

    if(HAS_AVX2)
        message(STATUS "AVX2 support detected.")
        set(KMCOMP_SIMD_FLAGS -DKMCOMP_USE_AVX2 -msse2 -mavx2)
    else()
        message(STATUS "AVX2 not supported.")
        set(KMCOMP_SIMD_FLAGS "")
    endif()

elseif(KMCOMP_ARCH MATCHES "^(arm64|aarch64)$")
    message(STATUS "ARM NEON support assumed (baseline on aarch64).")
    set(KMCOMP_SIMD_FLAGS -DKMCOMP_ARCH_NEON)
else()
    message(FATAL_ERROR "Unsupported architecture '${CMAKE_SYSTEM_PROCESSOR}': kmcomp requires x86 (AVX2) or ARM (NEON).")
endif()

if(KMCOMP_METRICS)
    set(KMCOMP_BUILD_MAIN true)
    set(KMCOMP_METRICS_FLAG -DKMCOMP_METRICS)
else()
    set(KMCOMP_METRICS_FLAG "")
endif()

add_subdirectory(external)

add_library(kmcomp_build_type_flags INTERFACE)

target_compile_options(kmcomp_build_type_flags INTERFACE
    $<$<CONFIG:Debug>:-O -g -march=native ${KMCOMP_SIMD_FLAGS} ${KMCOMP_METRICS_FLAG}>
    $<$<CONFIG:Release>:-O3 -DNDEBUG -march=native ${KMCOMP_SIMD_FLAGS} ${KMCOMP_METRICS_FLAG}>
    $<$<CONFIG:Profile>:-O3 -ggdb3 -DNDEBUG -fno-inline -march=native ${KMCOMP_SIMD_FLAGS} ${KMCOMP_METRICS_FLAG}>
    $<$<CONFIG:Coverage>:-O0 -g --coverage -march=native ${KMCOMP_SIMD_FLAGS} ${KMCOMP_METRICS_FLAG}>
)

include_directories(
    ${PROJECT_SOURCE_DIR}/include
)

set(KMCOMP_SRC_DIR ${PROJECT_SOURCE_DIR}/src)
set(KMCOMP_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)

add_library(kmcomp)
target_sources(kmcomp PRIVATE
    ${KMCOMP_SRC_DIR}/fast_median.cpp
    ${KMCOMP_SRC_DIR}/rng.cpp
    ${KMCOMP_SRC_DIR}/kmcomp.cpp
    ${KMCOMP_SRC_DIR}/tsp.cpp
)

target_link_libraries(kmcomp PRIVATE kmcomp_build_type_flags sdsl-lite zstd_ext block_compressor_ext)
target_include_directories(kmcomp PUBLIC
    $<BUILD_INTERFACE:${KMCOMP_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:include>
)
add_dependencies(kmcomp block_compressor_ext)

if(KMCOMP_METRICS)
    target_link_libraries(kmcomp PRIVATE json_ext)
    add_dependencies(kmcomp json_ext)
endif()


if(KMCOMP_BUILD_MAIN)
    add_executable(main_kmcomp
        ${KMCOMP_SRC_DIR}/main_kmcomp.cpp
    )
    target_link_libraries(main_kmcomp PRIVATE kmcomp_build_type_flags)
    target_include_directories(main_kmcomp PUBLIC
        $<BUILD_INTERFACE:${KMCOMP_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(main_kmcomp PRIVATE cxxopts_ext zstd_ext sdsl-lite block_compressor_ext kmcomp)
    add_dependencies(main_kmcomp kmcomp block_compressor_ext cxxopts_ext)

    if(KMCOMP_METRICS)
        target_link_libraries(main_kmcomp PRIVATE json_ext)
        add_dependencies(main_kmcomp json_ext)
    endif()

    set_target_properties(main_kmcomp PROPERTIES OUTPUT_NAME "kmcomp")
endif()
