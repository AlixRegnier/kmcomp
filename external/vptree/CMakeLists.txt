include(FetchContent)

FetchContent_Declare(vptree
GIT_REPOSITORY "git@github.com:AlixRegnier/VPTree.git" #"https://github.com/AlixRegnier/VPTree.git"
GIT_TAG main
)

FetchContent_MakeAvailable(vptree)

add_library(vptree_ext ALIAS vptree)