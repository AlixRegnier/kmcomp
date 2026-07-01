# kmcomp

## About

This library is made to permute columns in a binary matrix file (a cell is a bit) in order to make it more compressible.

It has been designed to be highly scalable as it can deal with big matrices. It can also reduce the computations by partitioning the columns that can be permuted.

## How to compile  

### Requirements  

* GNU C/CPP compiler (inprogress: Clang)
* Intel SIMD (inprogress: handling ARM instruction set)
* CMake

```bash
#Direct usage
./build.sh 
```

Otherwise, you can include this project in  a CMake project by using function like FetchContent.

## Help

There is a binary that is compiled by default when using build script. It allows some operations without having to create your own main file to use the library.

### Usage

```
./kmcomp -i <path> -c <columns> [-b <blocksize>] [--compress-to <path>] [-f <path> [-r]] [-g <groupsize>] [--header <headersize>] [-s <subsamplesize>] [-t <path>]
```


### Arguments
Shortname|Longname|Arg|Description
--|--|--|--
-b|--block-size|int|Targeted block size in bytes {8388608}
-c|--columns|int|Number of columns
-z|--compress-to|str|Write out permuted and compressed matrix to path
-f|--from-order|str|Load permutation file from path
-g|--group-size|int|Partition column reordering into groups of given size {0}
&nbsp;|--header|int|Input matrix header size {0}
-h|--help|-|Print help
-n|--no-reorder|-|Ignore reordering flags, program will do nothing if ``-z`` is not used
-p|--preset|int|Require ``--compress-to``. Compression preset level [1-22] {3}
-i|--input|str|Input matrix file path.
-r|--reverse|-|Require ``-f``. Invert permutation (retrieve original matrix)
-s|--subsample-size|int|Number of rows to use for distance computation {20000}
-t|--to-order|str|Write out permutation file to path
&nbsp;|--threshold|float|Reorder only if permutation would improve compression more than given percent. Uses a precomputed linear regression to predict reordering improvement.