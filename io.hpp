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
    static File open(char const *name, File::Mode mode, File::Type type = File::Type::Invalid);
    File(File const &) = delete;
    File operator=(File const &) = delete;

    File(File &&other) noexcept;
    File &operator=(File &&) noexcept;
    ~File();
private:
    File(char const *name, std::FILE *fp, Type type);
};

bool writeImage(File const &file, std::uint8_t image[], int width, int height, int channels);


#endif  // WRITER_HPP
