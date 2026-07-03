#include <kmcomp.h>
#include <cstdint>
#include <cmath>

#if defined(KMCOMP_USE_AVX2)
//AVX2/SSE2
#include <immintrin.h>
#include <emmintrin.h>
#endif

#define GET_ROW_PTR(x) (mapped_file+HEADER+((std::size_t)(x))*ROW_LENGTH)
#define GET_BLOCK_PTR(x) (mapped_file+HEADER+((std::size_t)(x))*BLOCK_SIZE)

namespace kmcomp
{
#define INP(x, y) inp[(x)*ncols/8 + (y)/8]
#define OUT(x, y) out[(y)*nrows/8 + (x)/8]

// II is defined as either (i) or (i ^ 7); i for LSB first, i^7 for MSB first
#define II (i^7)

#if defined(KMCOMP_USE_AVX2)
    // Code from https://mischasan.wordpress.com/2011/10/03/the-full-sse2-bit-matrix-transpose-routine/
    void __sse2_trans(std::uint8_t const *inp, std::uint8_t *out, long nrows, long ncols)
    {
        ssize_t rr, cc, i, h;
        union
        {
            __m128i x;
            std::uint8_t b[16];
        } tmp;

        if(nrows % 8 != 0 || ncols % 8 != 0)
            throw std::invalid_argument("[ERROR] kmcomp::__sse2_trans : Number of columns and of rows must be both multiple of 8.");

        // Do the main body in 16x8 blocks:
        for ( rr = 0; rr + 16 <= nrows; rr += 16 )
        {
            for ( cc = 0; cc < ncols; cc += 8 )
            {
                for ( i = 0; i < 16; ++i )
                    tmp.b[i] = INP(rr + II, cc);
                for ( i = 8; --i >= 0; tmp.x = _mm_slli_epi64(tmp.x, 1))
                    *(std::uint16_t *) &OUT(rr, cc + II) = _mm_movemask_epi8(tmp.x);
            }
        }

        if ( nrows % 16 == 0 )
            return;
        rr = nrows - nrows % 16;

        // The remainder is a block of 8x(16n+8) bits (n may be 0).
        //  Do a PAIR of 8x8 blocks in each step:
        for ( cc = 0; cc + 16 <= ncols; cc += 16 )
        {
            for ( i = 0; i < 8; ++i )
            {
                tmp.b[i] = h = *(std::uint16_t const *) &INP(rr + II, cc);
                tmp.b[i + 8] = h >> 8;
            }
            for ( i = 8; --i >= 0; tmp.x = _mm_slli_epi64(tmp.x, 1))
            {
                OUT(rr, cc + II) = h = _mm_movemask_epi8(tmp.x);
                OUT(rr, cc + II + 8) = h >> 8;
            }
        }
        if ( cc == ncols )
            return;

        //  Do the remaining 8x8 block:
        for ( i = 0; i < 8; ++i )
            tmp.b[i] = INP(rr + II, cc);
        for ( i = 8; --i >= 0; tmp.x = _mm_slli_epi64(tmp.x, 1))
            OUT(rr, cc + II) = _mm_movemask_epi8(tmp.x);
    }

#else
    // Portable replacement for the SSE2 routine above: no NEON "movemask"
    // equivalent exists, so instead of emulating PMOVMSKB this transposes
    // one 8x8 bit block at a time with the classic delta-swap bit trick
    // (same algorithm family as a 64x64 word transpose, just at 8-bit
    // granularity -- no architecture-specific intrinsics needed at all).
    static void __kmcomp_transpose8x8(std::uint8_t* a)
    {
        int j = 4;
        std::uint8_t m = 0x0F;
        while (j != 0)
        {
            for (int k = 0; k < 8; k = ((k | j) + 1) & ~j)
            {
                std::uint8_t t = static_cast<std::uint8_t>((a[k] ^ (a[k | j] >> j)) & m);
                a[k] = static_cast<std::uint8_t>(a[k] ^ t);
                a[k | j] = static_cast<std::uint8_t>(a[k | j] ^ (t << j));
            }
            j >>= 1;
            m = static_cast<std::uint8_t>(m ^ (m << j));
        }
    }

    void __sse2_trans(std::uint8_t const *inp, std::uint8_t *out, long nrows, long ncols)
    {
        if(nrows % 8 != 0 || ncols % 8 != 0)
            throw std::invalid_argument("[ERROR] kmcomp::__sse2_trans : Number of columns and of rows must be both multiple of 8.");

        const long ncols_bytes = ncols / 8;
        const long nrows_bytes = nrows / 8;

        for (long br = 0; br < nrows / 8; ++br)
        {
            for (long bc = 0; bc < ncols_bytes; ++bc)
            {
                std::uint8_t block[8];
                for (int r = 0; r < 8; ++r)
                    block[r] = INP(br * 8 + r, bc * 8);

                __kmcomp_transpose8x8(block);

                for (int r = 0; r < 8; ++r)
                    OUT(br * 8, bc * 8 + r) = block[r];
            }
        }
    }

#endif

#undef II
#undef OUT
#undef INP
    
    std::size_t target_block_nb_rows(const std::size_t NB_COLS, const std::size_t BLOCK_TARGET_SIZE)
    {
        const std::size_t ROW_LENGTH = (NB_COLS + 7) / 8;

        //Compute the number of rows in a block and round to next multiple of 8 
        return (((BLOCK_TARGET_SIZE+ROW_LENGTH-1) / ROW_LENGTH) + 7) / 8 * 8;
    }

    std::size_t target_block_size(const std::size_t NB_COLS, const std::size_t BLOCK_TARGET_SIZE)
    {
        const std::size_t ROW_LENGTH = (NB_COLS + 7) / 8;

        //Block size will most of time be slightly bigger than targeted size
        return ROW_LENGTH * target_block_nb_rows(NB_COLS, BLOCK_TARGET_SIZE); 
    }
  
    double compute_order_from_matrix_columns(const std::string& MATRIX_PATH, const unsigned HEADER, const std::size_t NB_COLS, const std::size_t NB_ROWS, std::size_t groupsize, std::size_t subsampled_rows, std::vector<std::uint64_t>& order)
    {
        #ifdef KMCOMP_METRICS
        DECLARE_TIMER;
        START_TIMER;
        #endif

        int fd = open(MATRIX_PATH.c_str(), O_RDONLY); 
        if(fd < 0)
            throw std::runtime_error("[ERROR] kmcomp::compute_order_from_matrix_columns : Failed to open a file descriptor on reference matrix.");

        const std::size_t ROW_LENGTH = (NB_COLS + 7) / 8;
        const std::size_t FILE_SIZE = HEADER + ROW_LENGTH * NB_ROWS;

        order.resize(ROW_LENGTH * 8);

        const char * const mapped_file = (const char * const)mmap(nullptr, FILE_SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
        
        subsampled_rows = subsampled_rows / 8 * 8;
        if(subsampled_rows == 0)
            subsampled_rows = NB_ROWS / 8 * 8;

        if(subsampled_rows > NB_ROWS)
            throw std::invalid_argument("[ERROR] kmcomp::compute_order_from_matrix_columns : Number of subsampled rows can't be greater to the number of rows in the binary matrix. Maybe one of the parameters is wrong ?");

        if(subsampled_rows % 8 != 0)
            throw std::invalid_argument("[ERROR] kmcomp::compute_order_from_matrix_columns : Number of subsampled rows is not a multiple of 8. Maybe your matrix has less than 8 rows ?");
       
        if(groupsize % 8 != 0)
            throw std::invalid_argument("[ERROR] kmcomp::compute_order_from_matrix_columns : The size of a group of columns must be a multiple of 8 (for transposition).");

        if(groupsize == 0 || groupsize > ROW_LENGTH*8)
            groupsize = ROW_LENGTH * 8;

        char * transposed_matrix = KMCOMP_ALLOCATE_MATRIX(subsampled_rows, ROW_LENGTH*8);
        __sse2_trans(reinterpret_cast<const std::uint8_t*>(mapped_file+HEADER), reinterpret_cast<std::uint8_t*>(transposed_matrix), subsampled_rows, ROW_LENGTH*8);

        std::size_t last_group_size;
        
        //Number of group of columns
        const std::size_t NB_GROUPS = (ROW_LENGTH*8+groupsize-1)/groupsize;

        if((ROW_LENGTH*8) % groupsize == 0)
            last_group_size = groupsize;
        else
            last_group_size = (ROW_LENGTH*8) % groupsize;

        std::size_t offset = 0; //Offset for global order assignation


        std::size_t computed_distances = 0;
        double original_consecutive_distances_sum = 0.0;
        double new_consecutive_distances_sum = 0.0;
        
        for(std::size_t i = 0; i + 1 < NB_GROUPS; ++i)
        {
            //Find a suboptimal path minimizing the weight of edges and visiting each node once
            computed_distances += build_double_ended_NN(transposed_matrix, groupsize, subsampled_rows, offset, order);
            
            for(std::size_t j = 0; j + 1 < groupsize; ++j)
            {
                original_consecutive_distances_sum += columns_hamming_distance(transposed_matrix, subsampled_rows, j+offset, j+1+offset);
                new_consecutive_distances_sum += columns_hamming_distance(transposed_matrix, subsampled_rows, order[j+offset], order[j+1+offset]);;
            }

            offset += groupsize;
        }

        computed_distances += build_double_ended_NN(transposed_matrix, last_group_size, subsampled_rows, offset, order);

        for(std::size_t j = 0; j + 1 < last_group_size; ++j)
        {
            original_consecutive_distances_sum += columns_hamming_distance(transposed_matrix, subsampled_rows, j+offset, j+1+offset);;
            new_consecutive_distances_sum += columns_hamming_distance(transposed_matrix, subsampled_rows, order[j+offset], order[j+1+offset]);
        }
        
        #ifdef KMCOMP_METRICS
        END_TIMER;
        metrics["3_time_permutation(s)"] = GET_TIMER; 
        
        std::size_t max_computable_distances = (groupsize * (groupsize - 1) / 2) * (NB_GROUPS - 1) + last_group_size * (last_group_size - 1) / 2;
        metrics["2a_computed_distances"] = computed_distances;
        metrics["2a_max_computable_distances"] = max_computable_distances;
        metrics["2a_pct_computed_distances(%)"] = 100.0 * computed_distances / max_computable_distances;

        double original_consecutive_distances_average = original_consecutive_distances_sum / (ROW_LENGTH*8 - 1);
        double new_consecutive_distances_average = new_consecutive_distances_sum / (ROW_LENGTH*8 - 1);

        double original_consecutive_distances_variance = 0.0;
        double new_consecutive_distances_variance = 0.0;

        //Compute variance
        for(unsigned i = 0; i + 1 < ROW_LENGTH*8; ++i)
        {
            original_consecutive_distances_variance += std::pow(columns_hamming_distance(transposed_matrix, subsampled_rows, i, i+1) - original_consecutive_distances_average, 2);
            new_consecutive_distances_variance += std::pow(columns_hamming_distance(transposed_matrix, subsampled_rows, order[i], order[i+1]) - new_consecutive_distances_average, 2);
        }

        //N-1: fix population bias
        original_consecutive_distances_variance /= ROW_LENGTH*8 - 2;
        new_consecutive_distances_variance /= ROW_LENGTH*8 - 2;

        //Compute stdev from variance
        double original_consecutive_distances_stdev = std::sqrt(original_consecutive_distances_variance);
        double new_consecutive_distances_stdev = std::sqrt(new_consecutive_distances_variance);

        metrics["2b_consecutive_column_distance_avg_original"] = original_consecutive_distances_average;
        metrics["2b_consecutive_column_distance_avg_reorder"] = new_consecutive_distances_average;
        metrics["2b_consecutive_column_distance_var_original"] = original_consecutive_distances_variance;
        metrics["2b_consecutive_column_distance_var_reorder"] = new_consecutive_distances_variance;
        metrics["2b_consecutive_column_distance_stdev_original"] = original_consecutive_distances_stdev;
        metrics["2b_consecutive_column_distance_stdev_reorder"] = new_consecutive_distances_stdev;
        metrics["2b_pigain"] = 1.0 * original_consecutive_distances_sum / new_consecutive_distances_sum;
        #endif

        KMCOMP_DELETE_MATRIX(transposed_matrix);

        return original_consecutive_distances_sum / new_consecutive_distances_sum;
    }

    #ifdef KMCOMP_METRICS
    void __count_bytes(const std::uint8_t * bytes, const std::size_t length, std::uint64_t * counts)
    {
        for(std::size_t i = 0; i < length; ++counts[bytes[i++]]);
    }

    double compute_byte_entropy(const std::uint64_t * counts)
    {
        //Shannon Entropy
        double entropy = 0.0;
        std::size_t total = 0;

        int i;

        for(i = 0; i < 256; ++i) 
            total += counts[i];
        
        //Shannon Entropy: -Sigma p(x) * log2(p(x)); p(x) = byte frequency
        for(i = 0; i < 256; ++i)
        {
            if(counts[i] > 0)
            {
                double px = (1.0 * counts[i]) / total;
                entropy -= px * std::log2(px);
            }
        }

        return entropy;
    }

    double get_block_entropy(const char * block_ptr, const std::size_t BLOCK_SIZE, std::uint64_t * counts)
    {
        __count_bytes(reinterpret_cast<const std::uint8_t*>(block_ptr), BLOCK_SIZE, counts);
        return compute_byte_entropy(counts);
    }

    double get_entropy_ratio(const std::string& MATRIX_PATH, const unsigned HEADER, const std::size_t NB_COLS, const std::size_t NB_ROWS, const std::vector<std::uint64_t>& ORDER, std::size_t SAMPLED_BYTES)
    {
        //Get row length in bytes
        const std::size_t ROW_LENGTH = (NB_COLS + 7) / 8;

        //Compute the number of rows in a block and round to next multiple of 8
        const std::size_t BLOCK_NB_ROWS = target_block_nb_rows(NB_COLS, SAMPLED_BYTES);

        //Block size will most of time be slightly bigger than targeted size
        const std::size_t BLOCK_SIZE = target_block_size(NB_COLS, SAMPLED_BYTES);

        //Compute last block size
        std::size_t last_block_size = (NB_ROWS % BLOCK_NB_ROWS) * ROW_LENGTH;
        if(last_block_size == 0)
            last_block_size = BLOCK_SIZE; //Each blocks are full, so last is full

        char * buffered_block = KMCOMP_ALLOCATE_MATRIX(BLOCK_NB_ROWS, ROW_LENGTH*8);
        char * transposed_block = KMCOMP_ALLOCATE_MATRIX(BLOCK_NB_ROWS, ROW_LENGTH*8);

        int fd = open(MATRIX_PATH.c_str(), O_RDONLY);

        //Skip header
        lseek(fd, HEADER, SEEK_SET);

        std::uint64_t count_bytes_original[256] = {};
        std::uint64_t count_bytes_reordered[256] = {};


        ssize_t read_bytes = read(fd, buffered_block, BLOCK_SIZE);
        close(fd);

        if(read_bytes == -1)
            throw std::runtime_error("[ERROR] kmcomp::get_entropy_ratio : An error occured while trying to read input matrix.");

        if(read_bytes % ROW_LENGTH != 0)
            throw std::runtime_error("[ERROR] kmcomp::get_entropy_ratio : Input matrix size is not a multiple of the size of a row.");

        double entropy_original = get_block_entropy(buffered_block, read_bytes, count_bytes_original);
        reorder_block(buffered_block, transposed_block, buffered_block, read_bytes, BLOCK_NB_ROWS, ROW_LENGTH, ORDER);
        double entropy_reordered = get_block_entropy(buffered_block, read_bytes, count_bytes_reordered);

        KMCOMP_DELETE_MATRIX(buffered_block);
        KMCOMP_DELETE_MATRIX(transposed_block);

        return entropy_original / entropy_reordered;
    }

    #endif

    void reorder_block(const char * input_block, char * tmp_block, char * output_block, const std::size_t BLOCK_SIZE, const std::size_t BLOCK_NB_ROWS, const std::size_t ROW_LENGTH, const std::vector<std::uint64_t>& ORDER)
    {
        //Copy block from disk to memory
        if(input_block != output_block)
            std::memcpy(output_block, input_block, BLOCK_SIZE);

        //Transpose matrix block
        __sse2_trans(reinterpret_cast<const std::uint8_t*>(output_block), reinterpret_cast<std::uint8_t*>(tmp_block), BLOCK_NB_ROWS, ROW_LENGTH*8);
        
        //Reorder block columns (by reordering transposed block rows)
        reorder_matrix_rows(tmp_block, 0, BLOCK_NB_ROWS/8, ORDER);
        
        //Transpose matrix block back
        __sse2_trans(reinterpret_cast<const std::uint8_t*>(tmp_block), reinterpret_cast<std::uint8_t*>(output_block), ROW_LENGTH*8, BLOCK_NB_ROWS);
    }

    void reorder_matrix_columns(const std::string& MATRIX_PATH, const unsigned HEADER, const std::size_t NB_COLS, const std::size_t NB_ROWS, const std::vector<std::uint64_t>& ORDER, const std::size_t BLOCK_TARGET_SIZE)
    {
        //Get row length in bytes
        const std::size_t ROW_LENGTH = (NB_COLS + 7) / 8;

        //Compute the number of rows in a block and round to next multiple of 8 
        const std::size_t BLOCK_NB_ROWS = target_block_nb_rows(NB_COLS, BLOCK_TARGET_SIZE);

        //Block size will most of time be slightly bigger than targeted size
        const std::size_t BLOCK_SIZE = target_block_size(NB_COLS, BLOCK_TARGET_SIZE); 

        //The last block may not be full
        const std::size_t NB_BLOCKS = (NB_ROWS+BLOCK_NB_ROWS-1) / BLOCK_NB_ROWS; 
        
        //Overshoot allows to consider last block as full and to apply operations, overshooted rows won't be written 
        const std::size_t FILE_SIZE = HEADER + NB_ROWS * ROW_LENGTH;

        //Compute last block size
        std::size_t last_block_size = (NB_ROWS % BLOCK_NB_ROWS) * ROW_LENGTH;
        if(last_block_size == 0)
            last_block_size = BLOCK_SIZE; //Each blocks are full, so last is full

        char * buffered_block = KMCOMP_ALLOCATE_MATRIX(BLOCK_NB_ROWS, ROW_LENGTH*8);
        char * transposed_block = KMCOMP_ALLOCATE_MATRIX(BLOCK_NB_ROWS, ROW_LENGTH*8);
    
        int fd = open(MATRIX_PATH.c_str(), O_RDWR);
        char * mapped_file = (char*)mmap(nullptr, FILE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

        if(mapped_file == MAP_FAILED)
            throw std::runtime_error("[ERROR] kmcomp::reorder_matrix_columns : mmap() failed for reordering matrix.");

        //Tell system that data will be accessed sequentially
        posix_madvise(mapped_file, FILE_SIZE, POSIX_MADV_SEQUENTIAL);

        std::size_t i = 0;
        for(; i + 1 < NB_BLOCKS; ++i)
        {
            //Reorder block
            reorder_block(GET_BLOCK_PTR(i), transposed_block, buffered_block, BLOCK_SIZE, BLOCK_NB_ROWS, ROW_LENGTH, ORDER);
            
            //Copy block from memory to disk
            std::memcpy(GET_BLOCK_PTR(i), buffered_block, BLOCK_SIZE);
        }

        //Reorder last block
        reorder_block(GET_BLOCK_PTR(i), transposed_block, buffered_block, last_block_size, BLOCK_NB_ROWS, ROW_LENGTH, ORDER);

        //Copy last block from memory to disk 
        std::memcpy(GET_BLOCK_PTR(i), buffered_block, last_block_size);

        KMCOMP_DELETE_MATRIX(buffered_block);
        KMCOMP_DELETE_MATRIX(transposed_block);

        munmap(mapped_file, FILE_SIZE);
        close(fd);
    }

    void reorder_matrix_columns_and_compress(const std::string& MATRIX_PATH, const std::string& OUTPUT_PATH, const std::string& OUTPUT_EF_PATH, const std::string& CONFIG_PATH, const unsigned HEADER, const std::size_t NB_COLS, const std::size_t NB_ROWS, const std::vector<std::uint64_t>& ORDER, std::size_t BLOCK_TARGET_SIZE)
    {
        #ifdef KMCOMP_METRICS
        DECLARE_TIMER;
        #endif

        //Get row length in bytes
        const std::size_t ROW_LENGTH = (NB_COLS + 7) / 8;

        //Compute the number of rows in a block and round to next multiple of 8 
        const std::size_t BLOCK_NB_ROWS = target_block_nb_rows(NB_COLS, BLOCK_TARGET_SIZE);

        //Block size will most of time be slightly bigger than targeted size
        const std::size_t BLOCK_SIZE = target_block_size(NB_COLS, BLOCK_TARGET_SIZE); 

        //The last block may not be full
        const std::size_t NB_BLOCKS = (NB_ROWS+BLOCK_NB_ROWS-1) / BLOCK_NB_ROWS; 
        
        const std::size_t FILE_SIZE = HEADER + NB_ROWS * ROW_LENGTH;
        
        //Compute last block size
        std::size_t last_block_size = (NB_ROWS % BLOCK_NB_ROWS) * ROW_LENGTH;
        if(last_block_size == 0)
            last_block_size = BLOCK_SIZE; //Each blocks are full, so last is full

        char * buffered_block = KMCOMP_ALLOCATE_MATRIX(BLOCK_NB_ROWS, ROW_LENGTH*8);
        char * transposed_block = KMCOMP_ALLOCATE_MATRIX(BLOCK_NB_ROWS, ROW_LENGTH*8);
    
        int fd = open(MATRIX_PATH.c_str(), O_RDONLY);

        //Since no modifications will be applied to original matrix, open it with MAP_PRIVATE mode
        char * const mapped_file = (char* const)mmap(nullptr, FILE_SIZE, PROT_READ, MAP_PRIVATE, fd, 0);

        if(mapped_file == MAP_FAILED)
            throw std::runtime_error("[ERROR] kmcomp::reorder_matrix_columns_and_compress : mmap() initialization failed for reordering matrix.");

        //Tell system that data will be accessed sequentially
        posix_madvise(mapped_file, FILE_SIZE, POSIX_MADV_SEQUENTIAL);

        #ifdef KMCOMP_METRICS
        std::size_t time_compression = 0;
        std::size_t time_reorder = 0;
        START_TIMER;
        #endif

        BlockCompressorZSTD block_compressor(OUTPUT_PATH, OUTPUT_EF_PATH, CONFIG_PATH);
        block_compressor.write_header(mapped_file, HEADER);

        #ifdef KMCOMP_METRICS
        END_TIMER;
        time_compression += __integral_time;
        #endif
        
        std::size_t i = 0;
        //Process each blocks except the last
        for(; i + 1 < NB_BLOCKS; ++i)
        {
            #ifdef KMCOMP_METRICS
            START_TIMER;
            #endif
            reorder_block(GET_BLOCK_PTR(i), transposed_block, buffered_block, BLOCK_SIZE, BLOCK_NB_ROWS, ROW_LENGTH, ORDER);
            #ifdef KMCOMP_METRICS
            END_TIMER;
            time_reorder += __integral_time;

            START_TIMER;
            #endif

            //Bring buffered block to compressor
            block_compressor.append_block(reinterpret_cast<std::uint8_t*>(buffered_block), BLOCK_SIZE);

            #ifdef KMCOMP_METRICS
            END_TIMER;
            time_compression += __integral_time;
            #endif
        }

        #ifdef KMCOMP_METRICS
        START_TIMER;
        #endif

        //Handle last block that may be smaller, only block effective size differs
        reorder_block(GET_BLOCK_PTR(i), transposed_block, buffered_block, last_block_size, BLOCK_NB_ROWS, ROW_LENGTH, ORDER);
        #ifdef KMCOMP_METRICS
        END_TIMER;
        time_reorder += __integral_time;

        START_TIMER;
        #endif
        //Bring last block to compressor
        block_compressor.append_block(reinterpret_cast<std::uint8_t*>(buffered_block), last_block_size);

        //Close
        block_compressor.close();

        #ifdef KMCOMP_METRICS
        END_TIMER;

        time_compression += __integral_time;
        metrics["1_nb_blocks"] = NB_BLOCKS;
        metrics["3_time_compression(s)"] = time_compression / 1000.0;
        metrics["3_time_reorder(s)"] = time_reorder / 1000.0;
        #endif

        KMCOMP_DELETE_MATRIX(buffered_block);
        KMCOMP_DELETE_MATRIX(transposed_block);

        munmap(mapped_file, FILE_SIZE);
        close(fd);
    }

    //Linear complexity, permute objects inplace by processing cycles
    void reorder_matrix_rows(char * mapped_file, const unsigned HEADER, const std::size_t ROW_LENGTH, const std::vector<std::uint64_t>& ORDER)
    {
        //Buffer to store a row
        char * buffer = new char[ROW_LENGTH];

        std::vector<bool> visited;
        visited.resize(ORDER.size());
    
        //Process each cycle in the permutation
        for (std::size_t i = 0; i < ORDER.size(); ++i) 
        {
            if (visited[i]) 
                continue;
            
            //Start of a new cycle
            std::size_t current = i;
            std::memcpy(buffer, GET_ROW_PTR(i), ROW_LENGTH);
            
            //Follow the cycle
            while (!visited[current]) 
            {
                visited[current] = true;
                std::size_t next = ORDER[current];
                
                if (next == i) 
                {
                    //End of cycle - place the temp value
                    std::memcpy(GET_ROW_PTR(current), buffer, ROW_LENGTH);
                } 
                else 
                {
                    //Move element and continue cycle
                    std::memcpy(GET_ROW_PTR(current), GET_ROW_PTR(next), ROW_LENGTH);
                    current = next;
                }
            }
        }

        delete[] buffer;
    }

    //Get an order that can be used to retrieve original matrix
    void reverse_order(const std::vector<std::uint64_t>& ORDER, std::vector<std::uint64_t>& reversed_order)
    {
        reversed_order.resize(ORDER.size());

        for(std::size_t i = 0; i < ORDER.size(); ++i)
            reversed_order[ORDER[i]] = i;
    }

};

#undef GET_ROW_PTR
#undef GET_BLOCK_PTR