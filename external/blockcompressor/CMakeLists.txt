include(FetchContent)

FetchContent_Declare(block_compressor
  GIT_REPOSITORY https://github.com/AlixRegnier/BlockCompressor.git
  GIT_TAG aabb2c200fffbf92c662123651c07ba3326903c6
)
FetchContent_MakeAvailable(block_compressor)

add_library(block_compressor_ext ALIAS block_compressor)

