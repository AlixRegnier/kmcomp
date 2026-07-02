if(KMCOMP_BUILD_MAIN)
  include(FetchContent)

  FetchContent_Declare(cxxopts
    GIT_REPOSITORY "https://github.com/jarro2783/cxxopts.git"
    GIT_TAG v3.3.1
  )
  FetchContent_MakeAvailable(cxxopts)

  add_library(cxxopts_ext ALIAS cxxopts)
endif()