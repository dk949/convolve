#include "defer.hpp"
#include "print.hpp"
#include "stb_image.h"
#include "stb_image_write.h"

#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <format>
#include <tuple>

namespace fs = std::filesystem;

// #define TIMING

enum struct Alg { None, Gauss, Avg };

namespace timing {
namespace chr = std::chrono;
#ifdef TIMING
static chr::time_point<chr::high_resolution_clock> start_point;
static chr::time_point<chr::high_resolution_clock> stop_point;

void start() {
    start_point = chr::high_resolution_clock::now();
}

void stop() {
    stop_point = chr::high_resolution_clock::now();
}

template<typename Precision = chr::microseconds>
void report() {
    auto took = (double)chr::duration_cast<Precision>(stop_point - start_point).count()
              / (double)chr::duration_cast<Precision>(chr::seconds(1)).count();
    println("Took {}s", took);
}
#else
void start() { }

void stop() { }

template<typename Precision = chr::microseconds>
void report() { }
#endif
}  // namespace timing

bool writeImage(fs::path filename, stbi_uc image[], int width, int height, int channels) {
    if (auto ex = filename.extension(); ex == ".jpg")
        return stbi_write_jpg(filename.c_str(), width, height, channels, image, 100);
    else if (ex == ".tga")
        return stbi_write_tga(filename.c_str(), width, height, channels, image);
    else if (ex == ".bmp")
        return stbi_write_bmp(filename.c_str(), width, height, channels, image);
    else if (ex == ".png")
        return stbi_write_png(filename.c_str(), width, height, channels, image, 0);

    return false;
}

fs::path checkExt(char const *filename) {
    auto const path = fs::path(filename);
    auto const ext = path.extension();
    if (ext != ".jpg" && ext != ".tga" && ext != ".bmp" && ext != ".png") {
        println("Unknown file extension {}", ext.c_str());
        exit(1);
    }
    return path;
}

auto args(int argc, char **argv) {
    if (argc < 3) {
        println(R"(Usage: {} INFILE OUTFILE [OPTS]

        -m|--matsize N      set matrix size, default: 5
        -s|--sigma N        set sigma, default: 1.4
        -a|--alg ENUM       pick algorythm, one of gauss, avg or none, default: gauss
        -c|--channels N     set number of channels to output, default: same as image
)",
            fs::path(argv[0]).filename().c_str());
        exit(1);
    }

    auto matsize = 5;
    auto channels = 0;
    auto sigma = 1.4;
    auto alg = Alg::Gauss;

    std::string arg;
    int i;
    auto const getNext = [&]() -> std::string & {
        if (argc <= ++i) {
            println("Expected an argument after {}", arg);
            exit(1);
        }
        arg = argv[i];
        return arg;
    };
    for (i = 3; i < argc; i++) {
        arg = argv[i];
        if (arg == "-m" || arg == "--matsize") {
            matsize = std::stoi(getNext());
            if (!(matsize % 2)) {
                println("Matrix size has to be odd");
                exit(1);
            }
        } else if (arg == "-c" || arg == "--channels") {
            channels = std::stoi(getNext());
            if (channels < 1) {
                println("Cannot have fewer than 1 channel");
                exit(1);
            }
            if (channels > 4) {
                println("Cannot have more than 4 channels");
                exit(1);
            }
        } else if (arg == "-s" || arg == "--sigma") {
            sigma = std::stod(getNext());
        } else if (arg == "-a" || arg == "--alg") {
            auto &next = getNext();
            std::transform(next.begin(), next.end(), next.begin(), [](auto ch) { return std::tolower(ch); });
            if (next == "gauss")
                alg = Alg::Gauss;
            else if (next == "avg")
                alg = Alg::Avg;
            else if (next == "none")
                alg = Alg::None;
            else {
                println("Unknown algorithm {}", arg);
                exit(1);
            }
        } else {
            println("Unrecognised argument '{}'", arg);
        }
    }
    return std::make_tuple(argv[1], checkExt(argv[2]), matsize, channels, sigma, alg);
}

double G(int x, int y, double sigma) {
    auto const sigma_2 = sigma * sigma;
    auto const frac = 1. / (2. * M_PI * sigma_2);
    auto const ex = exp(-(x * x + y * y) / (2. * sigma_2));
    return frac * ex;
}

double *makeMat(int size, double sigma) {
    auto *out = new double[size * size];
    auto const mid = size / 2;
    auto sum = 0.;
    for (int i = 0; i < size; i++)
        for (int j = 0; j < size; j++) {
            out[j * size + i] = G(i - mid, j - mid, sigma);
            sum += out[j * size + i];
        }
    for (int i = 0; i < size * size; i++)
        out[i] /= sum;

    return out;
}

inline constexpr double gauss(
    double mat[], stbi_uc image[], ssize_t x, ssize_t y, int channels, int ch, int width, int matsize, int halfmat) {
    double sum = 0.;
    for (int i = -halfmat, imat = 0; i <= halfmat; i++, imat++)
        for (int j = -halfmat, jmat = 0; j <= halfmat; j++, jmat++)
            sum += image[(y + j) * width * channels + (x + (i * channels)) + ch] * mat[imat * matsize + jmat];

    return sum;
}

inline constexpr double avg(
    double mat[], stbi_uc image[], ssize_t x, ssize_t y, int channels, int ch, int width, int matsize, int halfmat) {
    double sum = 0.;
    for (int i = -halfmat, imat = 0; i <= halfmat; i++, imat++)
        for (int j = -halfmat, jmat = 0; j <= halfmat; j++, jmat++)
            sum += image[(y + j) * width * channels + (x + (i * channels)) + ch];

    return sum / ((double)matsize * (double)matsize);
}

int main(int argc, char **argv) {
    auto const [infile, outfile, matsize, desired_channels, sigma, alg] = args(argc, argv);
    auto const halfmat = matsize / 2;
    int width, height, image_channels;

    auto image = stbi_load(infile, &width, &height, &image_channels, desired_channels);
    defer {
        stbi_image_free(image);
    };
    auto const channels = desired_channels ? desired_channels : image_channels;
    if (!image) {
        println("Could not load image {}: {}", argv[1], stbi_failure_reason());
        return 1;
    }

    print("input image {}: ({}x{})@{}. Using ", infile, width, height, channels);
    switch (alg) {
        case Alg::Gauss: println("Gausian blur, σ = {}, size = {}.", sigma, matsize); break;
        case Alg::Avg: println("averaging."); break;
        case Alg::None: println("nothing."); break;
    }

    auto mat = makeMat(matsize, sigma);
    defer {
        delete[] mat;
    };
    auto image_copy = new stbi_uc[width * height * channels];
    defer {
        delete[] image_copy;
    };
    timing::start();
#pragma omp parallel for
    for (ssize_t y = halfmat; y < height - halfmat; y++) {
        for (ssize_t x = halfmat * channels; x < (width - halfmat) * channels; x += channels) {
            for (int ch = 0; ch < channels; ch++) {
                switch (alg) {
                    case Alg::Gauss:
                        image_copy[y * width * channels + x + ch] =
                            gauss(mat, image, x, y, channels, ch, width, matsize, halfmat);
                        break;
                    case Alg::Avg:
                        image_copy[y * width * channels + x + ch] =
                            avg(mat, image, x, y, channels, ch, width, matsize, halfmat);
                        break;
                    case Alg::None:
                        image_copy[y * width * channels + x + ch] = image[y * width * channels + x + ch];
                        break;
                }
            }
        }
    }
    timing::stop();
    if (!writeImage(outfile, image_copy, width, height, channels)) {
        println("Could not write image to {}", outfile.c_str());
        return 1;
    }
    timing::report();
}
