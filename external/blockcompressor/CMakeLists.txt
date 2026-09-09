include(FetchContent)

set(BLOCKCOMPRESSOR_ENABLE_ZSTD ON CACHE BOOL
    "Enable Zstd support in BlockCompressor" FORCE
)

FetchContent_Declare(blockcompressor
  GIT_REPOSITORY https://github.com/AlixRegnier/BlockCompressor.git
  GIT_TAG dev
)
FetchContent_MakeAvailable(blockcompressor)

add_library(blockcompressor_ext ALIAS blockcompressor)

