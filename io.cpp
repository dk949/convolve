#include "io.hpp"
#define PRINT_FILE stderr

#include "print.hpp"
#include "stb_image.h"
#include "stb_image_write.h"

#include <cerrno>
#include <cstring>
#include <filesystem>

void writeCallback(void *context, void *data, int size) {
    std::FILE *fp = (FILE *)context;
    std::fwrite(data, 1, size, fp);
}

bool writeImage(File const &file, std::uint8_t image[], int width, int height, int channels) {
    using enum File::Type;
    switch (file.type) {
        case Jpg: return stbi_write_jpg_to_func(writeCallback, file.fp, width, height, channels, image, 100);
        case Png: return stbi_write_png_to_func(writeCallback, file.fp, width, height, channels, image, 0);
        case Tga: return stbi_write_tga_to_func(writeCallback, file.fp, width, height, channels, image);
        case Bmp: return stbi_write_bmp_to_func(writeCallback, file.fp, width, height, channels, image);
        case Invalid: println("Impossible state: invalid file type when writing"); std::abort();
    }
    println("Impossible state: unhandled file type when writing");
    std::abort();
}

File File::open(char const *name, File::Mode mode, File::Type type) {
    static constexpr std::uint8_t bmp_magic[] = {0x42, 0x4d};
    static constexpr std::uint8_t jpg_magic[] = {0xff, 0xd8, 0xff, 0xe0};
    static constexpr std::uint8_t png_magic[] = {0x89, 0x50, 0x4e, 0x47};

    using enum File::Mode;
    FILE *const fp = [&] {
        if (name[0] == '-')
            return mode == Read ? stdin : stdout;
        else
            return std::fopen(name, mode == Read ? "r" : "w");
    }();

    if (!fp) {
        println("Could not open file {} for {}: {}", name, mode == Read ? "reading" : "writing", std::strerror(errno));
        exit(1);
    }

    type = [&] {
        using enum File::Type;
        if (auto ex = std::filesystem::path(name).extension(); ex == ".jpg")
            return Jpg;
        else if (ex == ".tga")
            return Tga;
        else if (ex == ".bmp")
            return Bmp;
        else if (ex == ".png")
            return Png;
        else if (mode == Write)
            return type;
        else {
            std::uint8_t dest[4];
            if (std::fread(dest, 1, 4, fp) != 4) {
                println("could not read file {}", name);
                exit(1);
            }
            for (int i = 3; i >= 0; i--)
                std::ungetc(dest[i], fp);

            bool is_bmp = true, is_jpg = true, is_png = true;
            for (int i = 0; i < 4; i++) {
                if (i == 2 && is_bmp) return Bmp;
                is_bmp = is_bmp && dest[i] == bmp_magic[i];
                is_jpg = is_jpg && dest[i] == jpg_magic[i];
                is_png = is_png && dest[i] == png_magic[i];
            }
            if (is_jpg) return Jpg;
            if (is_png) return Png;
            println("Could not determine input file type from magic, please use the -.extention syntax to specify");
            exit(1);
        }
    }();
    return File(name, fp, type);
}

File::File(File &&other) noexcept
        : name(nullptr)
        , fp(nullptr)
        , type(Type::Invalid) {
    *this = std::move(other);
}

File::File(char const *name, std::FILE *fp, Type type)
        : name(name)
        , fp(fp)
        , type(type) { }

File &File::operator=(File &&other) noexcept {
    if (this == &other) return *this;
    if (fp) std::fclose(fp);
    std::swap(name, other.name);
    std::swap(fp, other.fp);
    std::swap(type, other.type);
    return *this;
}

File::~File() {
    if (fp) std::fclose(fp);
}
