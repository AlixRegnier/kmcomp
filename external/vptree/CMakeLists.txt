include(FetchContent)

FetchContent_Declare(vptree
  GIT_REPOSITORY "https://github.com/AlixRegnier/VPTree.git"
  GIT_TAG "exp_no_masking"
)

FetchContent_MakeAvailable(vptree)

add_library(vptree_ext ALIAS vptree)
