#include <spng.h>

#include <cstddef>
#include <cstring>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "slp_png.hpp"

static void read_test(const char* input_path, const char* output_path);

auto main(int argc, char* argv[]) -> int try {
    const char* input_path = nullptr;
    const char* output_path = nullptr;

    for (int i = 1; i < argc; i++) {
        if (argv[i] == nullptr) continue;

        if (std::strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc || argv[i + 1] == nullptr) {
                std::cout << "invalid argument\n";
                return -1;
            }
            output_path = argv[i + 1];
            i += 2;
        } else {
            input_path = argv[i];
        }
    }

    if (input_path == nullptr || output_path == nullptr) {
        std::cout << "invalid argument\n";
        return -1;
    }

    read_test(input_path, output_path);

    return 0;
} catch (std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return -1;
} catch (...) {
    std::cerr << "Error: Unknown\n";
    return -1;
}

struct SpngImage {
    spng_ihdr ihdr{};
    spng_plte plte{};
    spng_trns trns{};
    bool has_trns = false;
    std::vector<unsigned char> pixels;
};
static auto spng_read(const char* path) -> SpngImage;
static void compare(const SpngImage& a, const slp::Image& b);

static void read_test(const char* input_path, const char* output_path) {
    auto image = slp::Image::read_png(input_path).value();
    auto spng_image = spng_read(input_path);
    compare(spng_image, image);
    image.write_png(output_path).value();
    spng_image = spng_read(input_path);
    compare(spng_image, image);
    image = slp::Image::read_png(output_path).value();
    compare(spng_image, image);
}

static void compare(const SpngImage& a, const slp::Image& b) {
    auto check = [](bool x) -> void {
        if (!x) throw std::runtime_error("test fail");
    };
    check(a.ihdr.width == b.width());
    check(a.ihdr.height == b.height());
    check(a.ihdr.bit_depth == b.format().bit_depth());
    check(a.ihdr.color_type == b.format().color_type());
    if (a.ihdr.compression_method != 0) throw std::runtime_error("unsupported format");
    if (a.ihdr.filter_method != 0) throw std::runtime_error("unsupported format");
    if (a.ihdr.interlace_method != 0) throw std::runtime_error("unsupported format");
    check(a.pixels == b.pixels());
    for (std::size_t i = 0; i < a.plte.n_entries; i++) {
        check(a.plte.entries[i].red == b.palette()[(i * 4) + 0]);
        check(a.plte.entries[i].green == b.palette()[(i * 4) + 1]);
        check(a.plte.entries[i].blue == b.palette()[(i * 4) + 2]);
        if (a.has_trns && i < a.trns.n_type3_entries)
            check(a.trns.type3_alpha[i] == b.palette()[(i * 4) + 3]);
    }
}

#define TRY(expr)          \
    do {                   \
        auto ret = (expr); \
        if (ret != 0) {    \
            goto error;    \
        }                  \
    } while (false)

static auto spng_read(const char* path) -> SpngImage {
    SpngImage img;
    std::FILE* file = std::fopen(path, "rb");
    spng_ctx* ctx = spng_ctx_new(0);
    std::size_t size = 0;
    TRY(spng_set_png_file(ctx, file));
    TRY(spng_get_ihdr(ctx, &img.ihdr));
    TRY(spng_decoded_image_size(ctx, SPNG_FMT_RAW, &size));
    img.pixels.resize(size);
    TRY(spng_decode_image(ctx, img.pixels.data(), size, SPNG_FMT_RAW, 0));
    if (img.ihdr.color_type == SPNG_COLOR_TYPE_INDEXED) {
        TRY(spng_get_plte(ctx, &img.plte));
        img.has_trns = spng_get_trns(ctx, &img.trns) == 0;
    }
    spng_ctx_free(ctx);
    std::fclose(file);
    return img;
error:
    spng_ctx_free(ctx);
    std::fclose(file);
    throw std::runtime_error("spng_read fail");
}
