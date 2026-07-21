include(FetchContent)

FetchContent_Declare(block_compressor
  GIT_REPOSITORY https://github.com/AlixRegnier/BlockCompressor.git
  GIT_TAG exp_wlog
)
FetchContent_MakeAvailable(block_compressor)

add_library(block_compressor_ext ALIAS block_compressor)

