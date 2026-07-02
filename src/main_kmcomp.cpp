#include <kmcomp.h>
#include <cxxopts.hpp>
#include <fstream>
#include <filesystem>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

#ifdef KMCOMP_METRICS
//Initialize metrics global JSON object
nlohmann::json metrics;
#endif

void usage()
{
    #ifdef KMCOMP_METRICS
    std::cout << "Usage: kmcomp -i <path> -c <columns> [-b <blocksize>] [--compress-to <path> --config-path <path> [-p <level>]] [-f <path> [-r]] [-g <groupsize>] [--header <headersize>] [-j <path>] [-n] [-s <subsamplesize>] [--threshold] [-t <path>]\n\n-b, --block-size\t<int>\tTargeted block size in bytes {8388608}.\n-c, --columns\t\t<int>\tNumber of columns.\n-z, --compress-to\t\t<str>\tWrite out permuted and compressed matrix to path.\n-f, --from-order\t<str>\tLoad permutation file from path.\n-g, --group-size\t<int>\tPartition column reordering into groups of given size {%columns%}.\n--header\t\t<int>\tInput matrix header size {0}.\n-h, --help\t\t\tPrint help.\n-i, --input\t\t<str>\tInput matrix file path.\n-j, --json\t\t<str>\tStore metrics in JSON file.\n-n, --no-reorder\t\tIgnore reordering flags, program will do nothing if '-z' is not used.\n-p, --preset\t\t<int>\tRequire '--compress-to'. Zstd preset level [1-22] {3}.\n-r, --reverse\t\t\tRequire '-f'. Invert permutation (retrieve original matrix).\n-s, --subsample-size\t<int>\tNumber of rows to use for distance computation {20000}.\n--threshold\t\t<int>\tReorder only if permutation would improve compression more than given percent (%).\n-t, --to-order\t\t<str>\tWrite out permutation file to path.\n\n";
    #else
    std::cout << "Usage: kmcomp -i <path> -c <columns> [-b <blocksize>] [--compress-to <path> --config-path <path> [-p <level>]] [-f <path> [-r]] [-g <groupsize>] [--header <headersize>] [-j <path>] [-n] [-s <subsamplesize>] [--threshold] [-t <path>]\n\n-b, --block-size\t<int>\tTargeted block size in bytes {8388608}.\n-c, --columns\t\t<int>\tNumber of columns.\n-z, --compress-to\t\t<str>\tWrite out permuted and compressed matrix to path.\n-f, --from-order\t<str>\tLoad permutation file from path.\n-g, --group-size\t<int>\tPartition column reordering into groups of given size {%columns%}.\n--header\t\t<int>\tInput matrix header size {0}.\n-h, --help\t\t\tPrint help.\n-i, --input\t\t<str>\tInput matrix file path.\n-j, --json\t\t<str>\tDisabled, for enabling this option see README.\n-n, --no-reorder\t\tIgnore reordering flags, program will do nothing if '-z' is not used.\n-p, --preset\t\t<int>\tRequire '--compress-to'. Zstd preset level [1-22] {3}.\n-r, --reverse\t\t\tRequire '-f'. Invert permutation (retrieve original matrix).\n-s, --subsample-size\t<int>\tNumber of rows to use for distance computation {20000}.\n--threshold\t\t<int>\tReorder only if permutation would improve compression more than given percent (%).\n-t, --to-order\t\t<str>\tWrite out permutation file to path.\n\n";
    #endif
}

int main(int argc, char ** argv)
{    
    std::string input_path;
    std::string output_path;
    std::string output_ef_path;
    std::string in_order_path;
    std::string out_order_path;
    std::string config_path;
    std::string json_path;
    
    unsigned header = 0;
    unsigned preset_level = 3;

    double threshold = 0.0;
    std::size_t groupsize = 0;
    std::size_t subsampled_rows = 10000;
    std::size_t columns;
    std::size_t target_block_size = 8388608; //8MiB

    bool compress = false;
    bool reverse = false;
    bool serialize_order = false;
    bool deserialize_order = false;
    bool no_reorder = false;
    bool user_threshold = false;

    try 
    {
        cxxopts::Options options("kmcomp", "Program reordering bitmatrix columns in a more compressive way (path TSP using Nearest-Neighbor)\n");
        
        options.add_options()
            ("b,block-size", "Targeted block size in bytes {8388608}.", cxxopts::value<std::size_t>())
            ("c,columns", "Number of columns.", cxxopts::value<std::size_t>())
            ("z,compress-to", "Write out permuted and compressed matrix to path.", cxxopts::value<std::string>())
            ("f,from-order", "Load permutation file from path.", cxxopts::value<std::string>())
            ("g,group-size", "Partition column reordering into groups of given size {%columns%}.", cxxopts::value<std::size_t>())
            ("header", "Input matrix header size {0}.", cxxopts::value<unsigned>())
            ("h,help", "Print help.")
            ("i,input", "Input matrix file path.", cxxopts::value<std::string>())
            ("n,no-reorder", "No reorder")
            ("p,preset", "Require '--compress-to'. Compression preset level [1-22] {3}.", cxxopts::value<unsigned>())
            ("r,reverse", "Require '-f'. Invert permutation (retrieve original matrix).")
            ("s,subsample-size", "Number of rows to use for distance computation {20000}.", cxxopts::value<std::size_t>())
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
        {
            std::cerr << "[ERROR] kmcomp::main : -i/--input is required.\n";
            return 1;
        }

        // Get required argument
        input_path = args["input"].as<std::string>();
        
        // Validate that index path exists and is a directory
        if (!std::filesystem::exists(input_path)) 
        {
            std::cerr << "[ERROR] kmcomp::main : Input matrix '" << input_path << "' does not exist.\n";
            return 2;
        }

        if (!args.count("columns"))
        {
            std::cerr << "[ERROR] kmcomp::main : Number of columns required.\n";
            return 2;
        }

        columns = args["columns"].as<std::size_t>();

        // Get optional arguments
        if (args.count("group-size"))
            groupsize = args["group-size"].as<std::size_t>();
        else
            groupsize = (columns + 7) / 8 * 8;
    
        if(args.count("header"))
            header = args["header"].as<unsigned>();

        if (args.count("subsample-size"))
            subsampled_rows = args["subsample-size"].as<std::size_t>();

        if (args.count("compress-to"))
        {
            output_path = args["compress-to"].as<std::string>();
            output_ef_path = output_path + ".ef";
            compress = true;

            #ifdef KMCOMP_METRICS
            metrics["0_output_path"] = output_path;
            metrics["0_output_ef_path"] = output_ef_path;
            #endif

            if(args.count("preset"))
                preset_level = args["preset"].as<unsigned>();

            if(preset_level < 1 || preset_level > 22)
            {
                std::cerr << "[ERROR] kmcomp::main : Compression preset level is out of range [1-22], got: '" << preset_level << "'.\n";
                return 2;
            }


            if (args.count("config-path"))
                config_path = args["config-path"].as<std::string>();
            else
            {
                std::cerr << "'--config-path' option is mandatory with option '-z'.\n";
                return 2;
            }
        }

        if(args.count("reverse"))
            if(args.count("from-order"))
                reverse = true;
            else
            {
                std::cerr << "[ERROR] kmcomp::main : Cannot use 'reverse' option if no order was given with '--from-order'.\n";
                return 2;
            }

        if(args.count("from-order"))
        {
            in_order_path = args["from-order"].as<std::string>();
            deserialize_order = true;

            #ifdef KMCOMP_METRICS
            metrics["0_from_permutation"] = in_order_path;
            #endif
        }
        #ifdef KMCOMP_METRICS
        else
            metrics["1_subsample_size"] = subsampled_rows;
        #endif

        if(args.count("to-order"))
        {
            out_order_path = args["to-order"].as<std::string>();
            serialize_order = true;
            
            #ifdef KMCOMP_METRICS
            metrics["0_to_permutation"] = out_order_path;
            #endif
        }

        if(args.count("block-size"))
            target_block_size = args["block-size"].as<std::size_t>();

        if(args.count("json"))
        {
            #ifdef KMCOMP_METRICS
            json_path = args["json"].as<std::string>();
            #else
            std::cerr << "[WARNING] kmcomp::main : Option -j/--json specified but disabled at compilation. See README.\n";
            #endif
        }
        else
            json_path = "";

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
    } 
    catch (const cxxopts::exceptions::exception& e)
    {
        usage();
        std::cerr << "[ERROR] kmcomp::main : " << e.what() << ".\n";
        return 2;
    } 
    catch (const std::exception& e)
    {
        std::cerr << "[ERROR] kmcomp::main : unhandled exception '" << e.what() << "'.\n";
        return 2;
    }

    //Get file size to get the number of rows
    int fd = open(input_path.c_str(), O_RDONLY); //Open matrix in read-only
    
    if(fd < 0)
    {
        std::cerr << "Failed to open a file descriptor on reference matrix '" << input_path <<"'.\n";
        return 2;
    }

    const std::size_t FILE_SIZE = lseek(fd, 0, SEEK_END);
    close(fd);
    
    std::vector<std::size_t> order;
    
    const std::size_t ROW_LENGTH = (columns + 7) / 8;
    const std::size_t NB_ROWS = (FILE_SIZE - header) / ROW_LENGTH;
    
    #ifdef KMCOMP_METRICS
    metrics["0_input_path"] = input_path;
    metrics["1_nb_rows"] = NB_ROWS;
    metrics["1_nb_cols"] = ROW_LENGTH*8;
    metrics["1_groupsize"] = groupsize == 0 ? ROW_LENGTH*8 : (groupsize + 7) / 8 * 8;
    metrics["0_user_permutation"] = deserialize_order;
    metrics["0_invert_permutation"] = reverse;
    metrics["0_is_compressed"] = compress;

    DECLARE_TIMER;
    #endif

    //Compute block size according to the number of columns
    const std::size_t BLOCK_SIZE = kmcomp::target_block_size(columns, target_block_size);
    const std::size_t BLOCK_NB_ROWS = kmcomp::target_block_nb_rows(columns, target_block_size);

    //Compute order (or deserialize if given)
    if(deserialize_order)
    {
        order.resize(ROW_LENGTH*8);
        fd = open(in_order_path.c_str(), O_RDONLY);

        if(fd < 0)
        {
            std::cerr << "[ERROR] kmcomp::main : Could not deserialize order file '" << in_order_path << "', open syscall failed.\n";
            return 2;
        }

        read(fd, reinterpret_cast<char*>(order.data()), order.size()*sizeof(std::uint64_t));
        close(fd);
    }
    else if(!no_reorder) //If reorder enabled and no order was given, compute it
    {
        if(subsampled_rows > NB_ROWS)
        {
            std::cerr << "[WARNING] main : Subsampled rows (" << subsampled_rows << ") exceeds row count (" << NB_ROWS << "). Clamping to " << NB_ROWS << " rows.\n";
            subsampled_rows = NB_ROWS;
        }
        
        #ifdef KMCOMP_METRICS
        START_TIMER;
        #endif
        double metric = kmcomp::compute_order_from_matrix_columns(input_path, header, columns, NB_ROWS, groupsize, subsampled_rows, order);
        #ifdef KMCOMP_METRICS
        END_TIMER;
        #endif

        #ifdef KMCOMP_METRICS
        double entropy_ratio = kmcomp::get_entropy_ratio(input_path, header, columns, NB_ROWS, order);
        metrics["2b_entropy_ratio"] = entropy_ratio;
        metrics["3_time_permutation(s)"] = GET_TIMER;
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
    {
        std::vector<std::uint64_t> order_tmp(order);
        kmcomp::reverse_order(order_tmp, order);
    }

    if(compress)
    {
        #ifdef KMCOMP_METRICS
        metrics["1_blocksize(bytes)"] = BLOCK_SIZE;
        metrics["1_rows_per_block"] = BLOCK_NB_ROWS;
        metrics["1_target_blocksize(bytes)"] = target_block_size;
        #endif

        {
            std::ofstream config_file(config_path, std::ios::out);
            config_file << "samples = " << columns << "\n";
            config_file << "bitvectorsperblock = " << BLOCK_NB_ROWS << "\n";
            config_file << "preset = " << preset_level << std::endl;
        }

        if(no_reorder)
        {
            //Compress matrix
            #ifdef KMCOMP_METRICS
            START_TIMER;
            #endif
            BlockCompressorZSTD(output_path, output_ef_path, config_path).compress_file(input_path, header);
            #ifdef KMCOMP_METRICS
            END_TIMER;
            metrics["3_time_compression(s)"] = GET_TIMER;
            #endif
        }
        else 
        {
            //Reorder and compress matrix
            kmcomp::reorder_matrix_columns_and_compress(input_path, output_path, output_ef_path, config_path, header, columns, NB_ROWS, order, target_block_size);
        }
    }
    else if(!no_reorder)
    {
        #ifdef KMCOMP_METRICS
        START_TIMER;
        #endif
        //Reorder matrix
        kmcomp::reorder_matrix_columns(input_path, header, columns, NB_ROWS, order, target_block_size);

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
        {
            std::cerr << "Error: Couldn't serialize order, open syscall failed\n";
            return 2;
        }

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
