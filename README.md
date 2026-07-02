# kmcomp

## About

This library is made to permute columns in a binary matrix file (a cell is a bit) in order to make it more compressible.

It has been designed to be highly scalable as it can deal with big matrices. It can also reduce the computations by partitioning the columns that can be permuted.

## How to compile  

### Requirements  

* Unix system
* GNU C/CPP compiler
* Intel CPU with AVX2 instructions
* CMake

### Compilation

For a classic usage, you can build using command below. It will compile library and a CLI interface.
```bash
#Standard compilation
./build.sh 
```

### Other compilation options
**Library only:**
```bash
mkdir build
cd ./build
cmake .. -DKMCOMP_BUILD_MAIN=false
cd -
```
**Interface with metrics enabled**
```bash
mkdir build
cd ./build
cmake .. -DKMCOMP_METRICS=true
cd -
```

## Usage

```
./kmcomp -i <path> -c <columns> [-b <blocksize>] [-z <path> --config-path <path> [-p <level>]] [-f <path> [-r]] [-g <groupsize>] [--header <headersize>] [-s <subsamplesize>] [-t <path>]
```

## Arguments
Shortname|Longname|Arg|Description
--|--|--|--
-b|--block-size|int|Targeted block size in bytes {8388608}
-c|--columns|int|Number of columns
&nbsp;|--config-path|str|Mandatory if ``-z`` is used. Configuration path to use. If it exists, it will be loaded.
-f|--from-order|str|Load permutation file from path
-g|--group-size|int|Partition column reordering into groups of given size {0}
&nbsp;|--header|int|Input matrix header size {0}
-h|--help|-|Print help
-i|--input|str|Input matrix file path
-j|--json|str|Output JSON file path to store metrics (only available when compiled with ``-DKMCOMP_METRICS``)
-n|--no-reorder|-|Ignore reordering flags, program will do nothing if ``-z`` is not used
-p|--preset|int|Require ``-z``. Compression preset level [1-22] {3}
-r|--reverse|-|Require ``-f``. Invert permutation (retrieve original matrix)
-s|--subsample-size|int|Number of rows to use for distance computation {20000}
&nbsp;|--threshold|int|Reorder only if permutation would improve compression more than given percent (%)
-t|--to-order|str|Write out permutation file to path
-z|--compress-to|str|Write out permuted and compressed matrix to path
