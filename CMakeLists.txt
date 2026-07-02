cmake_minimum_required(VERSION 3.14)
project(kmcomp VERSION 1.0.0 LANGUAGES CXX)

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

if(KMCOMP_METRICS)
    set(KMCOMP_BUILD_MAIN true)
endif()

add_subdirectory(external)

add_library(kmcomp_build_type_flags INTERFACE)
if(KMCOMP_METRICS)
    target_compile_options(kmcomp_build_type_flags INTERFACE
    $<$<CONFIG:Debug>:-O -g -DKMCOMP_METRICS -march=native -msse2 -mavx2>
    $<$<CONFIG:Release>:-O3 -DNDEBUG -DKMCOMP_METRICS -march=native -msse2 -mavx2>
    $<$<CONFIG:Profile>:-O3 -ggdb3 -DNDEBUG -DKMCOMP_METRICS -fno-inline -march=native -msse2 -mavx2>
    $<$<CONFIG:Coverage>:-O0 -g -DKMCOMP_METRICS --coverage -march=native -msse2 -mavx2>
)
else()
    target_compile_options(kmcomp_build_type_flags INTERFACE
    $<$<CONFIG:Debug>:-O -g -march=native -msse2 -mavx2>
    $<$<CONFIG:Release>:-O3 -DNDEBUG -march=native -msse2 -mavx2>
    $<$<CONFIG:Profile>:-O3 -ggdb3 -DNDEBUG -fno-inline -march=native -msse2 -mavx2>
    $<$<CONFIG:Coverage>:-O0 -g --coverage -march=native -msse2 -mavx2>
)
endif()

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
