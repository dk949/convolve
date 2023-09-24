#ifndef WRITER_HPP
#define WRITER_HPP
#include <cstdint>
#include <cstdio>

struct File {
    enum struct Type { Invalid, Jpg, Png, Tga, Bmp };
    enum struct Mode { Read, Write };
    char const *name;
    std::FILE *fp;
    Type type;
    static File open(char const *name, File::Mode mode, File::Type type = File::Type::Invalid) noexcept;
    File(File const &) = delete;
    File operator=(File const &) = delete;

    File(File &&other) noexcept;
    File &operator=(File &&) noexcept;
    ~File() noexcept;
private:
    File(char const *name, std::FILE *fp, Type type) noexcept;
};

bool writeImage(File const &file, std::uint8_t image[], int width, int height, int channels) noexcept;


#endif  // WRITER_HPP
