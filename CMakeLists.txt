cmake_minimum_required(VERSION 3.16)

project(
    kmcomp
    VERSION 1.0.0
    LANGUAGES CXX
)

# -----------------------------------------------------------------------------
# Options
# -----------------------------------------------------------------------------

option(KMCOMP_BUILD_MAIN "Build the kmcomp command-line executable" ON)
option(KMCOMP_METRICS "Enable metrics support" OFF)

# -----------------------------------------------------------------------------
# Global C++ settings
# -----------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -----------------------------------------------------------------------------
# Dependencies used by the kmcomp library
# -----------------------------------------------------------------------------

# Header-only dependencies required by the core library.
add_subdirectory(external)

# -----------------------------------------------------------------------------
# kmcomp static library
# -----------------------------------------------------------------------------

add_library(kmcomp STATIC
    src/kmcomp.cpp
    src/tsp.cpp
)

add_library(kmcomp::kmcomp ALIAS kmcomp)

target_include_directories(kmcomp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(kmcomp
    PUBLIC
        blockcompressor_ext
        vptree_ext
        external_zstd
)

if(KMCOMP_METRICS)
    target_link_libraries(kmcomp
        PRIVATE
            json_ext
    )

    target_compile_definitions(kmcomp
        PRIVATE
            KMCOMP_METRICS
    )
endif()

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" KMCOMP_ARCH)

set(KMCOMP_SIMD_FLAGS "")
if(KMCOMP_ARCH MATCHES "^(x86_64|amd64|i386|i686)$")
    # Add any compiler/CPU configuration here if these files define
    # functions/macros used below.
    include(cmake/sse2.cmake)
    include(cmake/avx2.cmake)

    check_for_sse2()
    check_for_avx2()

    if(HAS_AVX2)
        message(STATUS "AVX2 support detected.")
        set(KMCOMP_SIMD_FLAGS -DKMCOMP_USE_AVX2 -msse2 -mavx2)
    elseif(HAS_SSE2)
        message(STATUS "SSE2 support detected.")
        set(KMCOMP_SIMD_FLAGS -DKMCOMP_USE_SSE2 -msse2)
    else()
        message(STATUS "AVX2 not supported, use fallback without SIMD.")
    endif()

elseif(KMCOMP_ARCH MATCHES "^(arm64|aarch64)$")
    message(STATUS "ARM NEON support assumed (baseline on aarch64).")
    set(KMCOMP_SIMD_FLAGS -DKMCOMP_ARCH_NEON)
else()
    message(STATUS "Unknown architecture, use fallback without SIMD.")
endif()

# -----------------------------------------------------------------------------
# Command-line executable
# -----------------------------------------------------------------------------

if(KMCOMP_BUILD_MAIN OR KMCOMP_METRICS)
    add_executable(kmcomp_cli
        src/main_kmcomp.cpp
    )

    # Keep the produced binary named "kmcomp", while the CMake target has
    # a different name from the kmcomp library.
    set_target_properties(kmcomp_cli PROPERTIES
        OUTPUT_NAME kmcomp
    )

    target_link_libraries(kmcomp_cli
        PRIVATE
            kmcomp::kmcomp
            cxxopts_ext
            external_zstd
    )

    # JSON is needed only by the executable when metrics are enabled.
    if(KMCOMP_METRICS)
        target_link_libraries(kmcomp_cli
            PRIVATE
                json_ext
        )

        target_compile_definitions(kmcomp_cli
            PRIVATE
                KMCOMP_METRICS
        )
    endif()

endif()
