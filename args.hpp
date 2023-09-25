#ifndef ARGS_HPP
#define ARGS_HPP

#include "io.hpp"
#include "print.hpp"

#include <algorithm>
#include <filesystem>
namespace fs = std::filesystem;

enum struct Alg { None, Gauss, Sobel, Custom, Avg };

#define DIE(...)              \
    do {                      \
        println(__VA_ARGS__); \
        exit(1);              \
    } while (false)

inline fs::path checkExt(char const *filename) noexcept {
    auto const path = fs::path(filename);
    auto const ext = path.extension();
    if (ext != ".jpg" && ext != ".tga" && ext != ".bmp" && ext != ".png") DIE("Unknown file extension {}", ext.c_str());

    return path;
}

inline auto args(int argc, char **argv) noexcept {
    auto matsize = 5;
    auto channels = 0;
    auto sigma = 1.4;
    auto sobel_type = 0;
    auto alg = Alg::None;
    int th_hi = 255;
    int th_lo = 0;
    char const *custom_mat = nullptr;

    if (argc < 3) {
        DIE(R"(Usage: {0} INFILE OUTFILE [OPTS]

        -m|--matsize N              set matrix size, default: {1}
        -s|--sigma N                set sigma, default: {2}
           --sobel-type N           Sobel filter type (0, 1 or 2), default: {3}
        -t|--threshold N,N          upper and lower threshold values, default: {4},{5}
        -x|--custom-matrix MAT      specify the matrix to use use with custom algorythm, default: none
        -a|--alg ENUM               pick algorythm, one of gauss, sobel, avg, custom or none, default: none
        -c|--channels N             set number of channels to output, default: same as input image


        note that a dash (-) can be used insted of INFILE or OUTFILE to use stdin and stdout respectively

        -.extension can be used to force a particular input or output format. E.g:
            {0} -.jpg -.png -a none # convert image from jpg to png

        if no extension is specified, input format is obtained from file signature
        and output format is the same as input format


        the following format can be used to specify a custom matrix:
            cells are separated by commas (,)
            rows are separated by bars (|)
            cells may only be numbers (integer or floating point)
            the matrix has to be a squear with odd side length
            if the matrix is not normalised, it will be normalised
        E.g:
            0.1,0.2,0.3|0,0,0|-0.1,-0.2,-0.3
            represents the matrix:
            ┌               ┐
            │ 0.1  0.2  0.3 │
            │   0    0    0 │
            │-0.1 -0.2 -0.3 │
            └               ┘

            1,2,3|4,5,6|7,8,9
            represents:
            ┌                  ┐
            │0.022 0.044 0.067 │
            │0.089 0.111 0.133 │
            │0.156 0.178   0.2 │
            └                  ┘

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
            } else if (arg == "-x" || arg == "--custom-matrix") {
                getNext();
                custom_mat = argv[i];
            } else if (arg == "-a" || arg == "--alg") {
                auto &next = getNext();
                std::transform(next.begin(), next.end(), next.begin(), [](auto ch) { return std::tolower(ch); });
                if (next == "gauss")
                    alg = Alg::Gauss;
                else if (next == "sobel")
                    alg = Alg::Sobel;
                else if (next == "custom")
                    alg = Alg::Custom;
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
    if (custom_mat) {
        std::string_view sv = custom_mat;
        matsize = int(std::count(sv.begin(), sv.end(), '|') + !sv.ends_with('|'));
    }
    if (alg == Alg::Custom && !custom_mat) DIE("custom algorythm requires specifying a matrix");

    auto input_file = File::open(argv[1], File::Mode::Read);
    auto outout_file = File::open(argv[2], File::Mode::Write, input_file.type);
    return std::make_tuple(std::move(input_file),
        std::move(outout_file),
        matsize,
        channels,
        sobel_type,
        sigma,
        std::uint8_t(th_lo),
        std::uint8_t(th_hi),
        custom_mat,
        alg);
}

#undef DIE

#endif  // ARGS_HPP
