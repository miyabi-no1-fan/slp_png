#include <spng.h>

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "slp_png.hpp"

struct SpngImage {
    spng_ihdr ihdr{};
    spng_plte plte{};
    spng_trns trns{};
    bool has_trns = false;
    std::vector<unsigned char> pixels;
};

static auto spng_read(const char* path) -> SpngImage;
static void spng_write(SpngImage& img, const char* path);

auto main(int argc, char* argv[]) -> int try {
    if (argc != 2) throw std::invalid_argument("what?");
    const char* input = argv[1];

    const char* slp_out = "a.png";
    const char* spng_out = "b.png";
    constexpr int N = 10;
    constexpr int W = 3;

    // warmup
    for (int i = 0; i < W; i++) {
        auto slp_img = slp::Image::read_png(input).value();
        slp_img.write_png(slp_out).value();
        auto spng_img = spng_read(input);
        spng_write(spng_img, spng_out);
    }

    using namespace std::chrono;

    duration<double> slp_read_time(0);
    duration<double> slp_write_time(0);
    duration<double> spng_read_time(0);
    duration<double> spng_write_time(0);

    for (int i = 0; i < N; i++) {
        std::cout << "\r" << i * 100 / N << "%" << std::flush;

        auto t = steady_clock::now();
        auto slp_img = slp::Image::read_png(input).value();
        slp_read_time += steady_clock::now() - t;

        t = steady_clock::now();
        slp_img.write_png(slp_out).value();
        slp_write_time += steady_clock::now() - t;

        t = steady_clock::now();
        auto spng_img = spng_read(input);
        spng_read_time += steady_clock::now() - t;

        t = steady_clock::now();
        spng_write(spng_img, spng_out);
        spng_write_time += steady_clock::now() - t;
    }
    std::cout << "\r" << 100 << "%" << "\n";

    slp_read_time /= N;
    slp_write_time /= N;
    spng_read_time /= N;
    spng_write_time /= N;

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Test image: `" << input << "`\n\n";
    std::cout << "| Library | Read time | Write time |\n";
    std::cout << "| --- | --- | --- |\n";
    std::cout << "| `libspng` | " << spng_read_time << " | " << spng_write_time << " |\n";
    std::cout << "| `slp_png` | " << slp_read_time << " | " << slp_write_time << " |\n";

    return 0;
} catch (std::exception& e) {
    std::cerr << "Exception: " << e.what() << "\n";
    return 1;
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

static void spng_write(SpngImage& img, const char* path) {
    std::FILE* file = std::fopen(path, "wb");
    spng_ctx* ctx = spng_ctx_new(SPNG_CTX_ENCODER);
    TRY(spng_set_png_file(ctx, file));
    TRY(spng_set_ihdr(ctx, &img.ihdr));
    if (img.ihdr.color_type == SPNG_COLOR_TYPE_INDEXED) {
        TRY(spng_set_plte(ctx, &img.plte));
        if (img.has_trns) TRY(spng_set_trns(ctx, &img.trns));
    }
    TRY(spng_encode_image(ctx, img.pixels.data(), img.pixels.size(), SPNG_FMT_RAW,
                          SPNG_ENCODE_FINALIZE));
    spng_ctx_free(ctx);
    std::fclose(file);
    return;
error:
    spng_ctx_free(ctx);
    std::fclose(file);
    throw std::runtime_error("spng_write fail");
}
