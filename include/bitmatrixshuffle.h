#ifndef BITMATRIXSHUFFLE_H
#define BITMATRIXSHUFFLE_H

#include <cstring>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <random>
#include <vector>

#include <utils.h>
#include <tsp.h>
#include <zstd/BlockCompressorZSTD.h>


//Global JSON object for storing metrics
#include <nlohmann/json.hpp>
extern nlohmann::json metrics;

#include <chrono>
#define DECLARE_TIMER std::chrono::time_point<std::chrono::high_resolution_clock> __start_timer, __stop_timer; std::size_t __integral_time

#define START_TIMER __start_timer = std::chrono::high_resolution_clock::now(); \
                    std::cout << std::flush

#define END_TIMER __stop_timer = std::chrono::high_resolution_clock::now(); \
                  __integral_time = static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(__stop_timer - __start_timer).count())


#define GET_TIMER (__integral_time / 1000.0)
#define SHOW_TIMER std::cout << std::setprecision(3) << GET_TIMER << "s" << std::endl

#define BMS_REGRESSION_SLOPE 0.413619691
#define BMS_REGRESSION_INTERCEPT 0.596332685

namespace bms
{
    std::size_t target_block_nb_rows(const std::size_t NB_COLS, const std::size_t BLOCK_TARGET_SIZE);

    std::size_t target_block_size(const std::size_t NB_COLS, const std::size_t BLOCK_TARGET_SIZE);

    //Return how much the compression will be improved according to metric returned by 'compute_order_from_matrix_columns'

    double get_entropy_ratio(const std::string& MATRIX_PATH, const unsigned HEADER, const std::size_t NB_COLS, const std::size_t NB_ROWS, const std::vector<std::uint64_t>& ORDER, std::size_t SAMPLED_BYTES = 8388608);

    constexpr double predict_metric_from_threshold(double threshold)
    {
        return threshold * BMS_REGRESSION_SLOPE + BMS_REGRESSION_INTERCEPT;
    }

    constexpr double predict_threshold_from_metric(double metric)
    {
        return (metric - BMS_REGRESSION_INTERCEPT) / BMS_REGRESSION_SLOPE;
    }

    //Start multiple path TSP instances to be solved using Nearest-Neighbor, need
    double compute_order_from_matrix_columns(const std::string& MATRIX_PATH, const unsigned HEADER, const std::size_t NB_COLS, const std::size_t NB_ROWS, const std::size_t GROUPSIZE, const std::size_t SUBSAMPLED_ROWS, std::vector<std::uint64_t>& order);

    //Reorder matrix columns (bit-swapping on memory-mapped file)
    void reorder_matrix_columns(const std::string& MATRIX_PATH, const unsigned HEADER, const std::size_t NB_COLS, const std::size_t NB_ROWS, const std::vector<std::uint64_t>& ORDER, const std::size_t BLOCK_TARGET_SIZE);

    //Reorder matrix columns (bit-swapping on memory-mapped file)
    void reorder_matrix_columns_and_compress(const std::string& MATRIX_PATH, const std::string& OUTPUT_PATH, const std::string& OUTPUT_EF_PATH, const std::string& CONFIG_PATH, const unsigned HEADER, const std::size_t NB_COLS, const std::size_t NB_ROWS, const std::vector<std::uint64_t>& ORDER, const std::size_t BLOCK_TARGET_SIZE, unsigned WLOG);

    //Reorder matrix rows (row-swapping on memory-mapped file)
    void reorder_matrix_rows(char* mapped_file, const unsigned HEADER, const std::size_t ROW_LENGTH, const std::vector<std::uint64_t>& ORDER);

    //Get an order that can be used to retrieve original matrix
    void reverse_order(const std::vector<std::uint64_t>& ORDER, std::vector<std::uint64_t>& reversed_order);
};

#endif
