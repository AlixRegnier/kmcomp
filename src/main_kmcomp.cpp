#include <fstream>
#include <filesystem>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

#include <cxxopts.hpp>

#include <block_compressor/block_compressor.hpp>
#include <block_compressor/block_decompressor.hpp>
#include <block_compressor/compressor_zstd.hpp>
#include <block_compressor/decompressor_zstd.hpp>

#include <kmcomp/error.hpp>
#include <kmcomp/kmcomp.hpp>

#ifdef KMCOMP_METRICS
//Initialize metrics global JSON object
nlohmann::json metrics;
#endif

void usage()
{
    std::cerr << \
"Usage: kmcomp -i <path> -c <columns> [-b <blocksize>] [--compress-to <path> --config-path <path> [-p <level>]] [-e <epsilon>] [-f <path> [-r]] [-g <groupsize>] [--header <headersize>] [-j <path>] [-n] [-s <subsamplesize>] [--threshold] [-t <path>]\n\n\
-b, --block-size\t<int>\tTargeted block size in bytes {65536}.\n\
-c, --columns\t\t<int>\tNumber of columns.\n\
-d, --decompress-to\t<str>\tWrite out decompressed matrix to path.\n\
-z, --compress-to\t<str>\tWrite out compressed matrix to path.\n\
-f, --from-order\t<str>\tLoad permutation file from path.\n\
-g, --group-size\t<int>\tPartition column reordering into groups of given size {%columns%}.\n\
--header\t\t<int>\tInput matrix header size {0}.\n\
-h, --help\t\t\tPrint help.\n\
-i, --input\t\t<str>\tInput matrix file path.\n"
#ifdef KMCOMP_METRICS
"-j, --json\t\t<str>\tStore metrics in JSON file.\n"
#else
"-j, --json\t\t<str>\tDisabled, for enabling this option see README.\n"
#endif
"-n, --no-reorder\t\tIgnore reordering flags, program will do nothing if '-z' is not used.\n\
-p, --preset\t\t<int>\tRequire '--compress-to'. Zstd preset level [1-22] {3}.\n\
-r, --reverse\t\t\tRequire '-f'. Invert permutation (retrieve original matrix).\n\
-s, --subsample-size\t<int>\tNumber of rows to use for distance computation {10000}.\n\
--threshold\t\t<int>\tReorder only if permutation would improve compression more than given percent (%).\n\
-t, --to-order\t\t<str>\tWrite out permutation file to path.\n\n";
}

inline void reverse_order(std::vector<std::uint64_t>& order)
{
    std::vector<std::uint64_t> order_tmp(order);
    kmcomp::reverse_order(order_tmp, order);
}

inline void decompress_matrix(const std::string& input_path, const std::string& input_ef_path, std::size_t header_size, std::size_t block_size, const std::string& output_path)
{
    if(!std::filesystem::exists(input_ef_path))
        throw kmcomp::kmcomp_error("kmcomp", "main", "Serialized IntContainer file was not found: '" + input_ef_path + "'");

    int fd = open(input_path.c_str(), O_RDONLY);

    if(fd < 0)
        throw kmcomp::kmcomp_error("kmcomp", "main", "Open syscall failed to read '" + input_path + "' header");

    char* header_buffer = new char[header_size];

    if(read(fd, header_buffer, header_size) != header_size)
    {
        close(fd);
        delete[] header_buffer;

        throw kmcomp::kmcomp_error("kmcomp", "main", "Read syscall could not read full header, expected header size: " + std::to_string(header_size) + "");
    }

    close(fd);

    std::ofstream out_stream(output_path, std::ofstream::binary);

    if(!out_stream.good())
    {
        delete[] header_buffer;
        throw kmcomp::kmcomp_error("kmcomp", "main", "Could not open stream for decompressing matrix");
    }

    out_stream.write(header_buffer, header);
    delete[] header_buffer;


    block_compressor::DecompressorZstd decompressor;
    block_compressor::IntContainerRaw<std::uint64_t> int_container;

    int_container.deserialize_file(input_ef_path);
    block_compressor::BlockDecompressor bd(input_path, block_size, decompressor, int_container, header_size);
    bd.decompress_all(out_stream);
    out_stream.close();
}

int main(int argc, char ** argv)
{    
    std::string input_path;
    std::string input_ef_path;
    std::string output_path;
    std::string output_ef_path;
    std::string in_order_path;
    std::string out_order_path;
    std::string config_path;

    #ifdef KMCOMP_METRICS
    std::string json_path = "";
    #endif

    unsigned preset_level = 3;
    
    double threshold = 0.0;
    double error_factor = 0.0;
    std::size_t groupsize = 0;
    std::size_t subsampled_rows = 10000;
    std::size_t columns;
    std::size_t target_block_size = 65536; //64 KiB
    
    std::size_t header = 0;

    bool compress = false;
    bool decompress = false;
    bool reverse = false;
    bool serialize_order = false;
    bool deserialize_order = false;
    bool no_reorder = false;
    bool user_threshold = false;

    try 
    {
        cxxopts::Options options("kmcomp", "Program reordering bitmatrix columns in a more compressive way (path TSP using Nearest-Neighbor)\n");
        
        options.add_options()
            ("b,block-size", "Targeted block size in bytes {65536}.", cxxopts::value<std::size_t>())
            ("c,columns", "Number of columns.", cxxopts::value<std::size_t>())
            ("d,decompress-to", "Write out decompressed matrix to path.", cxxopts::value<std::string>())
            ("z,compress-to", "Write out permuted and compressed matrix to path.", cxxopts::value<std::string>())
            ("e,epsilon", "Approximate nearest-neighbor [0.0-inf[ {0.0}. See README.", cxxopts::value<double>())
            ("f,from-order", "Load permutation file from path.", cxxopts::value<std::string>())
            ("g,group-size", "Partition column reordering into groups of given size {%columns%}.", cxxopts::value<std::size_t>())
            ("header", "Input matrix header size {0}.", cxxopts::value<std::size_t>())
            ("h,help", "Print help.")
            ("i,input", "Input matrix file path.", cxxopts::value<std::string>())
            ("n,no-reorder", "No reorder")
            ("p,preset", "Require '--compress-to'. Compression preset level [1-22] {3}.", cxxopts::value<unsigned>())
            ("r,reverse", "Require '-f'. Invert permutation (retrieve original matrix).")
            ("s,subsample-size", "Number of rows to use for distance computation {10000}.", cxxopts::value<std::size_t>())
            ("threshold", "Reorder only if permutation would improve compression more than given percent (%).", cxxopts::value<short>())
            ("t,to-order", "Write out permutation file to path.", cxxopts::value<std::string>())
            ("config-path", "Mandatory if '-z' is used. Configuration path to used. If it exists, it will be loaded.", cxxopts::value<std::string>());

        #ifdef KMCOMP_METRICS
        options.add_options()("j,json", "Output JSON file for metrics", cxxopts::value<std::string>());
        #else
        options.add_options()("j,json", "Disabled, see README for enabling metrics options", cxxopts::value<std::string>());
        #endif

        auto args = options.parse(argc, argv);

        if (args.count("help"))
        {
            usage();
            return 0;
        }

        // Check required argument
        if (!args.count("input"))
            throw kmcomp::kmcomp_error("kmcomp", "main", "-i/--input is required");

        // Get required argument
        input_path = args["input"].as<std::string>();

        // Validate that index path exists and is a directory
        if (!std::filesystem::exists(input_path)) 
            throw kmcomp::kmcomp_error("kmcomp", "main", "Input matrix '" + input_path + "' does not exist");

        if (!args.count("columns"))
            throw kmcomp::kmcomp_error("kmcomp", "main", "Number of columns required");

        columns = args["columns"].as<std::size_t>();

        // Get optional arguments
        if (args.count("group-size"))
            groupsize = args["group-size"].as<std::size_t>();
        else
            groupsize = (columns + 7) / 8 * 8;
    
        if(args.count("header"))
            header = args["header"].as<std::size_t>();

        if (args.count("subsample-size"))
            subsampled_rows = args["subsample-size"].as<std::size_t>();

        if(args.count("compress-to") && args.count("decompress-to"))
            throw kmcomp::kmcomp_error("kmcomp", "main", "Options '-z' (--compress-to) and '-d' (--decompress-to) are mutually exclusives.\n");

        if (args.count("compress-to"))
        {
            output_path = args["compress-to"].as<std::string>();
            output_ef_path = output_path + ".ef";
            compress = true;

            if(args.count("preset"))
                preset_level = args["preset"].as<unsigned>();

            if(preset_level < 1 || preset_level > 22)
                throw kmcomp::kmcomp_error("kmcomp", "main", "Compression preset level is out of range [1-22], got: '" + std::to_string(preset_level) + "'");

            if (args.count("config-path"))
                config_path = args["config-path"].as<std::string>();
            else
                throw kmcomp::kmcomp_error("kmcomp", "main", "'--config-path' option is mandatory with option '-z' (--compress-to)");
        }

        if(args.count("decompress-to"))
        {
            output_path = args["decompress-to"].as<std::string>();
            input_ef_path = input_path + ".ef";
            decompress = true;

            if(args.count("config-path"))
                config_path = args["config-path"].as<std::string>();
            else
                throw kmcomp::kmcomp_error("kmcomp", "main", "'--config-path' option is mandatory with option '-d' (--decompress-to).\n");
        }

        if(args.count("reverse"))
        {
            if(args.count("from-order"))
                reverse = true;
            else
                throw kmcomp::kmcomp_error("kmcomp", "main", "Cannot use 'reverse' option if no order was given with '--from-order'");
        }

        if(args.count("from-order"))
        {
            in_order_path = args["from-order"].as<std::string>();
            deserialize_order = true;
        }

        if(args.count("to-order"))
        {
            out_order_path = args["to-order"].as<std::string>();
            serialize_order = true;
        }

        if(args.count("block-size"))
            target_block_size = args["block-size"].as<std::size_t>();
            
        #ifndef KMCOMP_METRICS
        if(args.count("json"))
            std::cerr << kmcomp::warning_str("kmcomp", "main", "Option -j/--json specified but disabled at compilation. See README.\n");
        #endif
        
        if(args.count("no-reorder"))
        {
            no_reorder = true;
            deserialize_order = false;
            serialize_order = false;
            reverse = false;
        }

        if(args.count("threshold"))
        {
            user_threshold = true;
            threshold = args["threshold"].as<short>() / 100.0;
        }

        if(args.count("epsilon"))
        {
            error_factor = args["epsilon"].as<double>();

            if(error_factor < 0.0)
                throw kmcomp::kmcomp_error("kmcomp", "main", "Option -e/--error-nn is out of range [0.0-inf[, got: '" + std::to_string(error_factor) + "'");
        }
    } 
    catch (const cxxopts::exceptions::exception& e)
    {
        usage();
        std::cerr << kmcomp::error_str("kmcomp", "main", std::string(e.what())) << std::endl;
        return 1;
    }
    catch (const kmcomp::kmcomp_error& e)
    {
        std::cerr << e.what() << std::endl;
        return 2;
    }
    catch (const std::exception& e)
    {
        std::cerr << kmcomp::error_str("kmcomp", "main", std::string{"Unhandled exception '"} + e.what() + "'");
        return 2;
    }

    try
    {
        //Get file size to get the number of rows
        int fd = open(input_path.c_str(), O_RDONLY); //Open matrix in read-only

        if(fd < 0)
            throw kmcomp::kmcomp_error("kmcomp", "main", "Open syscall failed on matrix '" + input_path + "'");

        const std::size_t FILE_SIZE = lseek(fd, 0, SEEK_END);
        close(fd);

        //Compute block size according to the number of columns
        block_compressor::ConfigZstd config;

        if(decompress)
        {
            config.import_config_file(config_path);
            columns = config.get_elements_per_row();
            header = config.get_header_size();
            preset_level = config.get_preset();
            target_block_size = config.get_block_size();
        }

        const std::size_t ROW_LENGTH = (columns + 7) / 8;
        const std::size_t NB_ROWS = (FILE_SIZE - header) / ROW_LENGTH;

        if(compress)
        {
            config.set_preset(preset_level);
            config.set_bits_per_element(1, false);
            config.set_elements_per_row(ROW_LENGTH*8, false);
            config.set_header_size(header);
            config.target_block_size(target_block_size);
            config.export_config_file(config_path);
        }

        #ifdef KMCOMP_METRICS
        metrics["0_output_path"] = output_path;
        metrics["0_output_ef_path"] = output_ef_path;
        metrics["0_input_path"] = input_path;
        metrics["0_from_permutation"] = in_order_path;
        metrics["0_to_permutation"] = out_order_path;
        metrics["1_subsample_size"] = subsampled_rows;
        metrics["1_nb_rows"] = NB_ROWS;
        metrics["1_nb_cols"] = ROW_LENGTH*8;
        metrics["1_groupsize"] = groupsize == 0 ? ROW_LENGTH*8 : (groupsize + 7) / 8 * 8;
        metrics["1_error_factor"] = error_factor;
        metrics["0_user_permutation"] = deserialize_order;
        metrics["0_invert_permutation"] = reverse;
        metrics["0_is_compressed"] = compress;

        DECLARE_TIMER;
        #endif

        std::vector<std::uint64_t> order;

        //Compute order (or deserialize if given)
        if(deserialize_order)
        {
            order.resize(config.get_elements_per_row());
            fd = open(in_order_path.c_str(), O_RDONLY);

            if(fd < 0)
                throw kmcomp::kmcomp_error("kmcomp", "main", "Could not deserialize order file '" + in_order_path + "', open syscall failed");

            read(fd, reinterpret_cast<char*>(order.data()), order.size()*sizeof(std::uint64_t));
            close(fd);
        }
        else if(!no_reorder) //If reorder enabled and no order was given, compute it
        {
            if(subsampled_rows > NB_ROWS)
            {
                std::cerr << kmcomp::warning_str("kmcomp", "main", "Subsampled rows '" + std::to_string(subsampled_rows) + "' exceeds row count '" + std::to_string(NB_ROWS) + "'. Clamping to " + std::to_string(NB_ROWS) + " rows.\n");
                subsampled_rows = NB_ROWS;
            }

            #ifdef KMCOMP_METRICS
            START_TIMER;
            #endif
            double metric = kmcomp::compute_order_from_matrix_columns(input_path, header, columns, NB_ROWS, groupsize, subsampled_rows, order, error_factor);
            #ifdef KMCOMP_METRICS
            END_TIMER;
            metrics["3_time_permutation(s)"] = GET_TIMER;
            #endif

            #ifdef KMCOMP_METRICS
            double entropy_ratio = kmcomp::get_entropy_ratio(input_path, header, columns, NB_ROWS, order);
            metrics["2b_entropy_ratio"] = entropy_ratio;
            #endif
            
            double predicted_metric = kmcomp::predict_metric_from_threshold(threshold);

            //If default threshold and reordering would decrease compressibility, override linear regression and don't reorder
            if(user_threshold && metric < predicted_metric)
            {
                #ifdef KMCOMP_METRICS
                metrics["2b_metric_interpolated_threshold"] = predicted_metric;
                metrics["2b_metric_user_threshold"] = threshold;
                #endif

                no_reorder = true;
                reverse = false;
                serialize_order = false;
                deserialize_order = false;
            }
        }

        //Compute reversed order
        if(reverse)
            reverse_order(order);

        if(decompress)
            decompress_matrix(input_path, input_ef_path, header, target_block_size, output_path);

        if(compress)
        {
            #ifdef KMCOMP_METRICS
            metrics["1_blocksize(bytes)"] = config.get_block_size();
            metrics["1_rows_per_block"] = config.get_rows_per_block();
            metrics["1_target_blocksize(bytes)"] = target_block_size;
            #endif

            
            block_compressor::CompressorZstd compressor(config.get_preset());
            block_compressor::IntContainerRaw<std::uint64_t> int_container;
            int_container.reserve(FILE_SIZE/config.get_block_size()+2);
            block_compressor::BlockCompressor bc(output_path, config.get_block_size(), compressor, int_container);
            if(no_reorder)
            {
                //Compress matrix
                #ifdef KMCOMP_METRICS
                START_TIMER;
                #endif

                int fd = open(input_path.c_str(), O_RDONLY);
                const char* map = (const char*)mmap(nullptr, FILE_SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
                
                bc.write_raw_data(map, header);
                bc.append_data(map+header, FILE_SIZE-header);

                munmap(const_cast<char*>(map), FILE_SIZE);
                close(fd);

                #ifdef KMCOMP_METRICS
                END_TIMER;
                metrics["3_time_compression(s)"] = GET_TIMER;
                #endif
            }
            else 
            {
                //Reorder and compress matrix
                kmcomp::reorder_matrix_columns_and_compress(input_path, header, columns, NB_ROWS, order, bc);
            }

            bc.close();
            int_container.serialize_file(output_ef_path);
        }
        
        if(!no_reorder && !compress)
        {
            #ifdef KMCOMP_METRICS
            START_TIMER;
            #endif
            //Reorder matrix
            kmcomp::reorder_matrix_columns(decompress ? output_path : input_path, header, columns, NB_ROWS, order);

            #ifdef KMCOMP_METRICS
            END_TIMER;
            metrics["3_time_reorder(s)"] = GET_TIMER;
            #endif
        }

        //Serialize order
        if(serialize_order)
        {
            fd = open(out_order_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);

            if(fd < 0)
                throw kmcomp::kmcomp_error("kmcomp", "main", "Couldn't serialize order, open syscall failed\n");

            write(fd, reinterpret_cast<char*>(order.data()), order.size()*sizeof(std::uint64_t));
            close(fd);
        }

        #ifdef KMCOMP_METRICS
        if(json_path != "")
        {
            std::ofstream json_out(json_path);
            json_out << std::setw(4) << metrics << std::endl;
        }
        #endif
    }
    catch(const kmcomp::kmcomp_error& e)
    {
        std::cerr << e.what() << std::endl;
        return 2;
    }
    catch(const std::exception& e)
    {
        std::cerr << "Got an unhandled exception: " << e.what() << std::endl;
        return 2;
    }
}
