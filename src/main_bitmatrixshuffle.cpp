#include <bitmatrixshuffle.h>
#include <cxxopts.hpp>
#include <fstream>
#include <filesystem>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

//Initialize metrics global JSON object
nlohmann::json metrics;

void usage()
{
    std::cout << "Usage: bitmatrixshuffle -i <path> -c <columns> [-b <blocksize>] [--compress-to <path> [-p <level>]] [-f <path> [-r]] [-g <groupsize>] [--header <headersize>] [-j <path>] [-n] [-s <subsamplesize>] [--config-path <path>] [--threshold] [-t <path>]\n\n-b, --block-size\t<int>\tTargeted block size in bytes {8388608}.\n-c, --columns\t\t<int>\tNumber of columns.\n-z, --compress-to\t\t<str>\tWrite out permuted and compressed matrix to path.\n-f, --from-order\t<str>\tLoad permutation file from path.\n-g, --group-size\t<int>\tPartition column reordering into groups of given size {%columns%}.\n--header\t\t<int>\tInput matrix header size {0}.\n-h, --help\t\t\tPrint help.\n-i, --input\t\t<str>\tInput matrix file path.\n-n, --no-reorder\t\tIgnore reordering flags, program will do nothing if '-z' is not used.\n-p, --preset\t\t<int>\tRequire '--compress-to'. Zstd preset level [1-22] {3}.\n-r, --reverse\t\t\tRequire '-f'. Invert permutation (retrieve original matrix).\n-s, --subsample-size\t<int>\tNumber of rows to use for distance computation {20000}.\n--threshold\t\t<int>\tReorder only if permutation would improve compression more than given percent (%).\n-t, --to-order\t\t<str>\tWrite out permutation file to path.\n\n";
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
    unsigned wlog = 0;

    double threshold = 0.0;
    std::size_t groupsize = 0;
    std::size_t subsampled_rows = 20000;
    std::size_t columns;
    std::size_t target_block_size = 8388608; //8MiB

    bool compress = false;
    bool reverse = false;
    bool serialize_order = false;
    bool deserialize_order = false;
    bool no_reorder = false;
    bool user_threshold = false;
    bool print_json = true;

    try 
    {
        cxxopts::Options options("BitmatrixShuffle", "Program reordering bitmatrix columns in a more compressive way (path TSP using Nearest-Neighbor)\n");
        
        options.add_options()
            ("b,block-size", "Targeted block size in bytes {8388608}.", cxxopts::value<std::size_t>())
            ("c,columns", "Number of columns.", cxxopts::value<std::size_t>())
            ("z,compress-to", "Write out permuted and compressed matrix to path.", cxxopts::value<std::string>())
            ("f,from-order", "Load permutation file from path.", cxxopts::value<std::string>())
            ("g,group-size", "Partition column reordering into groups of given size {%columns%}.", cxxopts::value<std::size_t>())
            ("header", "Input matrix header size {0}.", cxxopts::value<unsigned>())
            ("h,help", "Print help.")
            ("i,input", "Input matrix file path.", cxxopts::value<std::string>())
            ("j,json", "Output JSON file", cxxopts::value<std::string>())
            ("n,no-reorder", "No reorder")
            ("p,preset", "Require '--compress-to'. Compression preset level [1-22] {3}.", cxxopts::value<unsigned>())
            ("r,reverse", "Require '-f'. Invert permutation (retrieve original matrix).")
            ("s,subsample-size", "Number of rows to use for distance computation {20000}.", cxxopts::value<std::size_t>())
            ("threshold", "Reorder only if permutation would improve compression more than given percent (%).", cxxopts::value<short>())
            ("t,to-order", "Write out permutation file to path.", cxxopts::value<std::string>())
            ("wlog", "Zstd wlog size (dictionary size)", cxxopts::value<unsigned>())
            ("config-path", "Path to config file {config.cfg}.", cxxopts::value<std::string>());

        auto args = options.parse(argc, argv);

        if (args.count("wlog"))
        {
            wlog = args["wlog"].as<unsigned>();

            if(wlog != 0 && (wlog < 10 || wlog > 31))
            {
                std::cerr << "Error: wlog must be 0 (default value) or between 10 and 31.\nSee https://facebook.github.io/zstd/zstd_manual.html\n";
                return 1;
            }
        }

        if (args.count("help"))
        {
            usage();
            return 0;
        }

        // Check required argument
        if (!args.count("input"))
        {
            std::cerr << "Error: -i/--input is required\n";
            usage();
            return 1;
        }

        // Get required argument
        input_path = args["input"].as<std::string>();
        
        // Validate that index path exists and is a directory
        if (!std::filesystem::exists(input_path)) 
        {
            std::cerr << "Error: Input matrix '" << input_path << "' does not exist\n";
            return 2;
        }

        if (!args.count("columns"))
        {
            std::cerr << "Error: Number of columns required\n";
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

            metrics["0_output_path"] = output_path;
            metrics["0_output_ef_path"] = output_ef_path;
            
            if(args.count("preset"))
                preset_level = args["preset"].as<unsigned>();

            if(preset_level < 1 || preset_level > 22)
            {
                std::cerr << "Error: Compression preset level is out of range [1-22], got: " << preset_level << std::endl;
                return 2;
            }
        }

        if(args.count("reverse"))
            if(args.count("from-order"))
                reverse = true;
            else
            {
                std::cerr << "Can't use 'reverse' option if no order was given with '--from-order'\n";
                return 2;
            }

        if(args.count("from-order"))
        {
            in_order_path = args["from-order"].as<std::string>();
            deserialize_order = true;

            metrics["0_from_permutation"] = in_order_path;
        }
        else
            metrics["1_subsample_size"] = subsampled_rows;

        if(args.count("to-order"))
        {
            out_order_path = args["to-order"].as<std::string>();
            serialize_order = true;
            
            metrics["0_to_permutation"] = out_order_path;
        }

        if(args.count("block-size"))
            target_block_size = args["block-size"].as<std::size_t>();


        config_path = "config.cfg";
        if (args.count("config-path"))
            config_path = args["config-path"].as<std::string>();

        if(args.count("json"))
        {
            json_path = args["json"].as<std::string>();
            print_json = false;
        }

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
        std::cerr << "Error parsing options: " << e.what() << std::endl;
        return 2;
    } 
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 2;
    }


    //Get file size to get the number of rows
    int fd = open(input_path.c_str(), O_RDONLY); //Open matrix in read-only
    
    if(fd < 0)
    {
        std::cerr << "Failed to open a file descriptor on reference matrix\n";
        return 2;
    }

    const std::size_t FILE_SIZE = lseek(fd, 0, SEEK_END);
    close(fd);

    
    std::vector<std::size_t> order;
    
    const std::size_t ROW_LENGTH = (columns + 7) / 8;
    const std::size_t NB_ROWS = (FILE_SIZE - header) / ROW_LENGTH;
    
    metrics["0_input_path"] = input_path;
    metrics["1_nb_rows"] = NB_ROWS;
    metrics["1_nb_cols"] = ROW_LENGTH*8;
    metrics["1_groupsize"] = groupsize == 0 ? ROW_LENGTH*8 : (groupsize + 7) / 8 * 8;
    metrics["0_user_permutation"] = deserialize_order;
    metrics["0_invert_permutation"] = reverse;
    metrics["0_is_compressed"] = compress;

    DECLARE_TIMER;

    //Compute block size according to the number of columns
    const std::size_t BLOCK_SIZE = bms::target_block_size(columns, target_block_size);
    const std::size_t BLOCK_NB_ROWS = bms::target_block_nb_rows(columns, target_block_size);

    //Compute order (or deserialize if given)
    if(deserialize_order)
    {
        order.resize(ROW_LENGTH*8);
        fd = open(in_order_path.c_str(), O_RDONLY);

        if(fd < 0)
        {
            std::cerr << "Error: Couldn't deserialize order, open syscall failed\n";
            return 2;
        }

        read(fd, reinterpret_cast<char*>(order.data()), order.size()*sizeof(std::uint64_t));
        close(fd);
    }
    else if(!no_reorder) //If reorder enabled and no order was given, compute it
    {
        if(subsampled_rows > NB_ROWS)
        {
            std::cout << "Warning: Subsampled rows (" << subsampled_rows << ") exceeds row count (" << NB_ROWS << "). Clamping to " << NB_ROWS << " rows." << std::endl;
            subsampled_rows = NB_ROWS;
        }

        START_TIMER;
        double metric = bms::compute_order_from_matrix_columns(input_path, header, columns, NB_ROWS, groupsize, subsampled_rows, order);
        END_TIMER;


        double entropy_ratio = bms::get_entropy_ratio(input_path, header, columns, NB_ROWS, order);
        metrics["2b_entropy_ratio"] = entropy_ratio;

        metrics["3_time_permutation(s)"] = GET_TIMER;

        double predicted_metric = bms::predict_metric_from_threshold(threshold);

        //If default threshold and reordering would decrease compressibility, override linear regression and don't reorder
        if(user_threshold && metric < predicted_metric)
        {
            metrics["2b_metric_interpolated_threshold"] = predicted_metric;
            metrics["2b_metric_user_threshold"] = threshold;

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
        bms::reverse_order(order_tmp, order);
    }

    if(compress)
    {
        metrics["1_blocksize(bytes)"] = BLOCK_SIZE;
        metrics["1_rows_per_block"] = BLOCK_NB_ROWS;
        metrics["1_target_blocksize(bytes)"] = target_block_size;

        if (!std::filesystem::exists(config_path))
        {
            std::ofstream config_file(config_path, std::ios::out);
            config_file << "samples = " << columns << "\n";
            config_file << "bitvectorsperblock = " << BLOCK_NB_ROWS << "\n";
            config_file << "preset = " << preset_level << std::endl;
        }

        if(no_reorder)
        {
            //Compress matrix
            START_TIMER;
            BlockCompressorZSTD(output_path, output_ef_path, config_path).compress_file(input_path, header);
            END_TIMER;

            metrics["3_time_compression(s)"] = GET_TIMER;
        }
        else
        {
            //Reorder and compress matrix
            bms::reorder_matrix_columns_and_compress(input_path, output_path, output_ef_path, config_path, header, columns, NB_ROWS, order, target_block_size, wlog);
        }
    }
    else if(!no_reorder)
    {
        //Reorder matrix
        START_TIMER;
        bms::reorder_matrix_columns(input_path, header, columns, NB_ROWS, order, target_block_size); 
        END_TIMER;

        metrics["3_time_reorder(s)"] = GET_TIMER;
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

    //Handle JSON output
    if(print_json)
    {
        std::cout << std::setw(4) << metrics << std::endl;
    }
    else
    {
        std::ofstream json_out(json_path);
        json_out << std::setw(4) << metrics << std::endl;
        json_out.close();
    }
}
