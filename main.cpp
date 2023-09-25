#define PRINT_FILE stderr

#include "args.hpp"
#include "defer.hpp"
#include "io.hpp"
#include "print.hpp"
#include "stb_image.h"
#include "stb_image_write.h"

#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <format>
#include <limits>
#include <numeric>
#include <tuple>

namespace timing {
namespace chr = std::chrono;
#ifdef TIMING
static chr::time_point<chr::high_resolution_clock> start_point;
static chr::time_point<chr::high_resolution_clock> stop_point;

void start() noexcept {
    start_point = chr::high_resolution_clock::now();
}

void stop() noexcept {
    stop_point = chr::high_resolution_clock::now();
}

template<typename Precision = chr::microseconds>
void report() noexcept {
    auto took = double(chr::duration_cast<Precision>(stop_point - start_point).count())
              / double(chr::duration_cast<Precision>(chr::seconds(1)).count());
    println("Took {}s", took);
}
#else
void start() noexcept { }

void stop() noexcept { }

template<typename Precision = chr::microseconds>
void report() noexcept { }
#endif
}  // namespace timing

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
         0.,   0.,   0.,
      -47., -162., -47.,
    },
};

// clang-format on

double G(int x, int y, double sigma) noexcept {
    auto const sigma_2 = sigma * sigma;
    auto const frac = 1. / (2. * M_PI * sigma_2);
    auto const ex = exp(-(x * x + y * y) / (2. * sigma_2));
    return frac * ex;
}

double *makeGaussMat(int size, double sigma) {
    auto const size_2 = size_t(size * size);
    auto *out = new double[size_2];
    auto const mid = size / 2;
    auto sum = 0.;
    for (int i = 0; i < size; i++)
        for (int j = 0; j < size; j++) {
            out[j * size + i] = G(i - mid, j - mid, sigma);
            sum += out[j * size + i];
        }
    for (size_t i = 0; i < size_2; i++)
        out[i] /= sum;

    return out;
}

double *makeAvgMat(int size) {
    auto const size_2 = size_t(size * size);
    auto *out = new double[size_2];
    for (size_t i = 0; i < size_2; i++)
        out[i] = 1. / double(size_2);

    return out;
}

double *reportCustomMatError(char const *custom_mat, size_t pos, char const *error = "") {
    println("Custom matrix specification error: {}\n"
            "\n"
            "\t{}\n"
            "\t{:>{}}\n",
        error,
        custom_mat,
        '^',
        pos);
    return nullptr;
}

double *makeCustomMat(char const *custom_mat, int size) {
    std::string_view sv = custom_mat;
    auto const size_2 = size_t(size * size);
    auto out = std::make_unique<double[]>(size_2);
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            char *end;
            out[size_t(i * size + j)] = std::strtod(sv.data(), &end);
            auto success = true;
            if (j == size - 1) {
                if (i == size - 1)
                    success = end[0] == 0 || end[0] == '|';
                else
                    success = end[0] == '|';
            } else {
                success = end[0] == ',';
            }
            if (!success) return reportCustomMatError(custom_mat, size_t(end - custom_mat));
            end += !(i == size - 1 && j == size - 1);
            sv = std::string_view(end, sv.end());
        }
    }
    if (sv.size() == 0 || (sv.size() == 1 && sv[0] == '|')) {
        auto const sum = std::reduce(out.get(), out.get() + size_2, 0);
        if (sum != 0)
            for (size_t i = 0; i < size_2; i++)
                out[i] /= sum;
        return out.release();
    } else
        return reportCustomMatError(custom_mat, sv.size(), "Extra characters");
}

void customMatPrinter(double mat[], int matsize) {
    size_t const max_w = std::transform_reduce(
        mat,
        mat + matsize * matsize,
        0ul,
        [](size_t x, size_t y) { return std::max(x, y); },
        [](double x) { return std::formatted_size("{:.2}", x); });
    size_t line_max_w = 0;
    for (int i = 0; i < matsize; i++) {
        size_t line_w = 2;
        for (int j = 0; j < matsize; j++)
            line_w += std::formatted_size("{:>{}.2} ", mat[i * matsize + j], max_w) + 1;
        line_w -= 2;
        line_max_w = std::max(line_max_w, line_w);
    };
    println("custom matrix: ");
    auto const [w, _] = getTermWH();
    if (line_max_w > w) {
        println("Matrix too big to display");
        return;
    }
    println("┌{:>{}}┐", "", line_max_w);
    for (int i = 0; i < matsize; i++) {
        print("│");
        for (int j = 0; j < matsize; j++)
            print(" {:>{}.2} ", mat[i * matsize + j], max_w);

        println("│");
    }
    println("└{:>{}}┘", "", line_max_w);
}

inline constexpr auto reflect(auto const &x, auto top) {
    top--;
    if (top < x)
        return top - (x - top);
    else
        return std::abs(x);
}

inline constexpr auto threshold(auto const &x, auto const &lo, auto hi) {
    if (x <= lo) return std::numeric_limits<std::remove_cvref_t<decltype(x)>>::min();
    if (x >= hi) return std::numeric_limits<std::remove_cvref_t<decltype(x)>>::max();
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
            sum += image[ycoord * width * channels + xcoord] * mat[imat * matsize + jmat];
        }

    return sum;
}

int main(int argc, char **argv) {
    auto const [infile, outfile, matsize, desired_channels, sobel_type, sigma, th_lo, th_hi, custom_mat, alg] =
        args(argc, argv);
    auto const halfmat = matsize / 2;
    int width, height, image_channels;

    auto image = stbi_load_from_file(infile.fp, &width, &height, &image_channels, desired_channels);
    defer {
        stbi_image_free(image);
    };
    auto const channels = desired_channels ? desired_channels : image_channels;
    if (!image) {
        println("Could not load image {}: {}", infile.name, stbi_failure_reason());
        return 1;
    }

    auto mat = [&] {
        switch (alg) {
            case Alg::Gauss: return makeGaussMat(matsize, sigma);
            case Alg::Avg: return makeAvgMat(matsize);
            case Alg::Custom: return makeCustomMat(custom_mat, matsize);
            case Alg::Sobel:
            case Alg::None: break;
        }
        return static_cast<double *>(nullptr);
    }();
    if (alg == Alg::Custom && !mat) {
        println("Failed to create matrix");
        return 1;
    }

    defer {
        delete[] mat;
    };

    print("input image {}: ({}x{})@{}. Using ", infile.name[0] == '-' ? "stdin" : infile.name, width, height, channels);
    switch (alg) {
        case Alg::Gauss: println("Gausian blur, σ = {}, size = {}.", sigma, matsize); break;
        case Alg::Sobel: println("Sobel filter, type {}.", sobel_type); break;
        case Alg::Custom: customMatPrinter(mat, matsize); break;
        case Alg::Avg: println("averaging."); break;
        case Alg::None: println("nothing."); break;
    }
    auto image_copy = new stbi_uc[size_t(width * height * channels)];
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
                    case Alg::Avg:
                    case Alg::Custom:
                        px = stbi_uc(convolve(mat, image, x, y, channels, ch, width, height, matsize, halfmat));
                        break;
                    case Alg::Sobel: {
                        auto const g_x = convolve(sobelX[sobel_type], image, x, y, channels, ch, width, height, 3, 1);
                        auto const g_y = convolve(sobelY[sobel_type], image, x, y, channels, ch, width, height, 3, 1);
                        px = stbi_uc(std::sqrt(g_x * g_x + g_y * g_y));
                    } break;
                    case Alg::None: px = image[y * width * channels + x + ch]; break;
                }
                px = threshold(px, th_lo, th_hi);
            }
        }
    }
    timing::stop();
    if (!writeImage(outfile, image_copy, width, height, channels)) {
        println("Could not write image to {}", outfile.name);
        return 1;
    }
    timing::report();
}
