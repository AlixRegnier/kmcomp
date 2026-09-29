include(FetchContent)

set(BLOCK_COMPRESSOR_ENABLE_ZSTD ON CACHE BOOL
  "Enable Zstd support in block_compressor" FORCE
)

FetchContent_Declare(block_compressor
  GIT_REPOSITORY https://github.com/AlixRegnier/block_compressor.git
  GIT_TAG dev
)
FetchContent_MakeAvailable(block_compressor)

add_library(block_compressor_ext ALIAS block_compressor)

