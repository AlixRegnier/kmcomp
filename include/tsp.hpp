#ifndef KMCOMP_TSP_H
#define KMCOMP_TSP_H

#include <cstdint>
#include <vector>
#include <kmcomp.hpp>

namespace kmcomp
{
    //Build a path by iteratively add closest vertex (compare closest from tail and closest from head), less sensitive to the first chosen vertex
    #ifdef KMCOMP_METRICS
    std::size_t build_double_ended_NN(const char* const MATRIX, const std::size_t COLUMNS, const std::size_t SUBSAMPLED_ROWS, const std::size_t OFFSET, std::vector<std::uint64_t>& order, double error_factor = 0.0);
    #else
    void build_double_ended_NN(const char* const MATRIX, const std::size_t COLUMNS, const std::size_t SUBSAMPLED_ROWS, const std::size_t OFFSET, std::vector<std::uint64_t>& order, double error_factor = 0.0);
    #endif
    
    //Distance computation between two columns
    double columns_hamming_distance(const char* const MATRIX, const std::size_t NB_ROWS, const std::uint64_t COLUMN_A, const std::uint64_t COLUMN_B);

    //Hamming distance between two buffers
    std::size_t hamming_distance(const std::uint8_t* const BUFFER1, const std::uint8_t* const BUFFER2, const std::size_t LENGTH);
};

#endif