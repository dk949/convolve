#ifndef ARGS_HPP
#define ARGS_HPP

#include "print.hpp"

#include <algorithm>
#include <filesystem>
namespace fs = std::filesystem;

enum struct Alg { None, Gauss, Sobel, Avg };

#define DIE(...)              \
    do {                      \
        println(__VA_ARGS__); \
        exit(1);              \
    } while (false)

inline fs::path checkExt(char const *filename) {
    auto const path = fs::path(filename);
    auto const ext = path.extension();
    if (ext != ".jpg" && ext != ".tga" && ext != ".bmp" && ext != ".png") DIE("Unknown file extension {}", ext.c_str());

    return path;
}

inline auto args(int argc, char **argv) {
    auto matsize = 5;
    auto channels = 0;
    auto sigma = 1.4;
    auto sobel_type = 0;
    auto alg = Alg::Gauss;
    int th_hi = 255;
    int th_lo = 0;

    if (argc < 3) {
        DIE(R"(Usage: {} INFILE OUTFILE [OPTS]

        -m|--matsize N      set matrix size, default: {}
        -s|--sigma N        set sigma, default: {}
           --sobel-type N   Sobel filter type (0, 1 or 2), default: {}
        -t|--threshold N,N  upper and lower threshold values, default: {},{}
        -a|--alg ENUM       pick algorythm, one of gauss, sobel, avg or none, default: gauss
        -c|--channels N     set number of channels to output, default: same as input image
)",
            fs::path(argv[0]).filename().c_str(),
            matsize,
            sigma,
            sobel_type,
            th_lo,
            th_hi);
    }


    std::string arg;
    int i;
    auto const getNext = [&]() -> std::string & {
        if (argc <= ++i) DIE("Expected an argument after {}", arg);

        arg = argv[i];
        return arg;
    };
    for (i = 3; i < argc; i++) {
        arg = argv[i];
        try {
            if (arg == "-m" || arg == "--matsize") {
                matsize = std::stoi(getNext());
                if (!(matsize % 2)) DIE("Matrix size has to be odd");

            } else if (arg == "-c" || arg == "--channels") {
                channels = std::stoi(getNext());
                if (channels < 1) DIE("Cannot have fewer than 1 channel");
                if (channels > 4) DIE("Cannot have more than 4 channels");

            } else if (arg == "--sobel-type") {
                sobel_type = std::stoi(getNext());
                if (sobel_type < 0 || sobel_type > 2) DIE("Sobel filter type has to be between 0 and 2 inclusive");

            } else if (arg == "-t" || arg == "--threshold") {
                auto &next = getNext();
                auto const comma = next.find(',');
                if (comma == next.npos) DIE("expected threshold in the format lo,hi");

                th_lo = std::stoi(next.substr(0, comma));
                th_hi = std::stoi(next.substr(comma + 1));

                if (th_lo < 0 || th_lo > 255 || th_hi < 0 || th_hi > 255)
                    DIE("threshold values have to be 0-255 inclusive");
                if (th_lo > th_hi) DIE("first threshold value has to be lower (or equal to) the second one");

            } else if (arg == "-s" || arg == "--sigma") {
                sigma = std::stod(getNext());
            } else if (arg == "-a" || arg == "--alg") {
                auto &next = getNext();
                std::transform(next.begin(), next.end(), next.begin(), [](auto ch) { return std::tolower(ch); });
                if (next == "gauss")
                    alg = Alg::Gauss;
                else if (next == "sobel")
                    alg = Alg::Sobel;
                else if (next == "avg")
                    alg = Alg::Avg;
                else if (next == "none")
                    alg = Alg::None;
                else
                    DIE("Unknown algorithm {}", arg);

            } else
                DIE("Unrecognised argument '{}'", arg);

        } catch (std::invalid_argument const &e) {
            DIE("Invalid number '{}': {}", arg, e.what());
        } catch (std::out_of_range const &e) {
            DIE("{} is out of range: {}", arg, e.what());
        }
    }
    return std::make_tuple(argv[1],
        checkExt(argv[2]),
        matsize,
        channels,
        sobel_type,
        sigma,
        (std::uint8_t)th_lo,
        (std::uint8_t)th_hi,
        alg);
}

#undef DIE

#endif  // ARGS_HPP
