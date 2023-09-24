#include "args.hpp"
#include "defer.hpp"
#include "print.hpp"
#include "stb_image.h"
#include "stb_image_write.h"

#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <format>
#include <limits>
#include <tuple>

#define TIMING

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

// clang-format off
constexpr double sobelX[][9] = {
    {
        1., 0., -1.,
        2., 0., -2.,
        1., 0., -1.,
    },
    {
         3., 0.,  -3.,
        10., 0., -10.,
         3., 0.,  -3.,
    },
    {
         47., 0.,  -47.,
        162., 0., -162.,
         47., 0.,  -47.,
    }
};
constexpr double sobelY[][9] = {
    {
         1.,  2.,  1.,
         0.,  0.,  0.,
        -1., -2., -1.,
    },
    {
         3.,  10.,  3.,
         0.,   0.,  0.,
        -3., -10., -3.,
    },
    {
         47.,  162.,  47.,
         0.,   0.,  0.,
        -47., -162., -47.,
    },
};

// clang-format on

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

inline constexpr auto reflect(auto const &x, auto top) {
    top--;
    if (top < x)
        return top - (x - top);
    else
        return std::abs(x);
}

inline constexpr auto threshold(auto x, auto lo, auto hi) {
    if (x <= lo) return std::numeric_limits<decltype(x)>::min();
    if (x >= hi) return std::numeric_limits<decltype(x)>::max();
    return x;
}

inline constexpr double convolve(double const mat[],
    const stbi_uc image[],
    ssize_t x,
    ssize_t y,
    int channels,
    int ch,
    int width,
    int height,
    int matsize,
    int halfmat) {
    double sum = 0.;
    for (int i = -halfmat, imat = 0; i <= halfmat; i++, imat++)
        for (int j = -halfmat, jmat = 0; j <= halfmat; j++, jmat++) {
            auto const ycoord = reflect(y + j, height);
            auto const xcoord = reflect(x + (i * channels) + ch, width * channels);
            auto const addr = ycoord * width * channels + xcoord;
            sum += image[addr] * mat[imat * matsize + jmat];
        }

    return sum;
}

inline constexpr double avg(
    double mat[], stbi_uc image[], ssize_t x, ssize_t y, int channels, int ch, int width, int height, int matsize, int halfmat) {
    double sum = 0.;
    for (int i = -halfmat, imat = 0; i <= halfmat; i++, imat++)
        for (int j = -halfmat, jmat = 0; j <= halfmat; j++, jmat++)
            sum += image[reflect(y + j, height) * width * channels + reflect(x + (i * channels), width * channels) + ch];

    return sum / ((double)matsize * (double)matsize);
}

int main(int argc, char **argv) {
    auto const [infile, outfile, matsize, desired_channels, sobel_type, sigma, th_lo, th_hi, alg] = args(argc, argv);
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
        case Alg::Sobel: println("Sobel filter, type {}.", sobel_type); break;
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
    for (ssize_t y = 0; y < height; y++) {
        for (ssize_t x = 0; x < width * channels; x += channels) {
            for (int ch = 0; ch < channels; ch++) {
                auto &px = image_copy[y * width * channels + x + ch];
                switch (alg) {
                    case Alg::Gauss:
                        px = convolve(mat, image, x, y, channels, ch, width, height, matsize, halfmat);
                        break;
                    case Alg::Sobel: {
                        auto const g_x = convolve(sobelX[sobel_type], image, x, y, channels, ch, width, height, 3, 1);
                        auto const g_y = convolve(sobelY[sobel_type], image, x, y, channels, ch, width, height, 3, 1);
                        px = std::sqrt(g_x * g_x + g_y * g_y);
                    } break;
                    case Alg::Avg: px = avg(mat, image, x, y, channels, ch, width, height, matsize, halfmat); break;
                    case Alg::None: px = image[y * width * channels + x + ch]; break;
                }
                px = threshold(px, th_lo, th_hi);
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
