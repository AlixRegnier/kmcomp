add_subdirectory(blockcompressor)
add_subdirectory(vptree)

if(KMCOMP_BUILD_MAIN)
    add_subdirectory(cxxopts)
endif()

if(KMCOMP_METRICS)
    add_subdirectory(json)
endif()
