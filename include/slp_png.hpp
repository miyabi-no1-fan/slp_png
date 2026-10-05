#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <expected>
#include <span>
#include <stdexcept>
#include <string>

namespace slp {
#define SLP_PNG_MACROS
#include "slp_image_transform.h"
#include "slp_png.h"

enum class Error {
    IO = IO_ERR,
    INVALID_PNG = INVALID_PNG,
};

class Image : private slp_image_t {
   public:
    Image() : slp_image_t{} {}
    ~Image() { clear(); }

    /**
     * @brief free the image's pixels
     */
    void clear() {
        SLP_FREE(pixels, size);
        pixels = nullptr;
    }

    /**
     * @brief allocate the image's pixels based on size,
     * assert(pixels == nullptr)
     */
    void allocate() {
        assert(pixels == nullptr);
        pixels = static_cast<std::uint8_t*>(SLP_MALLOC(size));
        if (pixels == nullptr)
            throw std::runtime_error("Image: allocation failed");
    }

    Image(const Image& o) : Image() { copy(o); }
    Image(const slp_image_t& o) : Image() { copy(o); }
    Image(Image&& o) noexcept : Image() { move(o); }
    Image(slp_image_t&& o) noexcept : Image() { move(o); }

    Image& operator=(const Image& o) {
        if (this != &o) {
            clear();
            copy(o);
        }
        return *this;
    }
    Image& operator=(const slp_image_t& o) {
        if (static_cast<const slp_image_t*>(this) != &o) {
            clear();
            copy(o);
        }
        return *this;
    }
    Image& operator=(Image&& o) noexcept {
        move(o);
        return *this;
    }
    Image& operator=(slp_image_t&& o) noexcept {
        move(o);
        return *this;
    }

   private:
    // assert(pixels == nullptr)
    void copy(const slp_image_t& o) {
        height = o.height;
        width = o.width;
        channels = o.channels;
        bit_depth = o.bit_depth;
        size = o.size;
        if (o.pixels != nullptr && size != 0) {
            allocate();
            if (pixels == nullptr)
                throw std::runtime_error("Image: allocation failed");
            std::memcpy(pixels, o.pixels, size);
        }
    }

    void move(slp_image_t& o) noexcept {
        if (static_cast<slp_image_t*>(this) == &o) return;
        clear();
        pixels = o.pixels;
        height = o.height;
        width = o.width;
        channels = o.channels;
        bit_depth = o.bit_depth;
        size = o.size;
        o.pixels = nullptr;
        o.size = 0;
    }

   public:
    std::uint8_t* data() const { return pixels; }
    std::uint32_t get_width() const { return width; }
    std::uint32_t get_height() const { return height; }
    // row_stride * height
    std::size_t get_size() const { return size; }
    // div_ceil(width * channels * bit_depth, 8)
    std::size_t row_stride() const {
        auto div_ceil = [](std::size_t a, std::size_t b) -> std::size_t {
            return (a / b) + (a % b != 0);
        };
        return div_ceil((std::size_t)width * (std::size_t)channels * (std::size_t)bit_depth, 8);
    }

    std::span<const std::uint8_t> row(std::size_t i) const {
        return std::span(pixels + i * row_stride(), row_stride());
    }
    std::span<std::uint8_t> row(std::size_t i) {
        return std::span(pixels + i * row_stride(), row_stride());
    }

    enum class Format {
        G1,
        G2,
        G4,
        G8,
        G16,

        GA8,
        GA16,

        RGB8,
        RGB16,

        RGBA8,
        RGBA16,
    };

    Format get_format() const {
        switch (channels) {
            case 1:
                switch (bit_depth) {
                    case 1: return Format::G1;
                    case 2: return Format::G2;
                    case 4: return Format::G4;
                    case 8: return Format::G8;
                    case 16: return Format::G16;
                    default: throw std::invalid_argument("Image::format: image has unknown format");
                }
            case 2:
                switch (bit_depth) {
                    case 8: return Format::GA8;
                    case 16: return Format::GA16;
                    default: throw std::invalid_argument("Image::format: image has unknown format");
                }
            case 3:
                switch (bit_depth) {
                    case 8: return Format::RGB8;
                    case 16: return Format::RGB16;
                    default: throw std::invalid_argument("Image::format: image has unknown format");
                }
            case 4:
                switch (bit_depth) {
                    case 8: return Format::RGBA8;
                    case 16: return Format::RGBA16;
                    default: throw std::invalid_argument("Image::format: image has unknown format");
                }
            default: throw std::invalid_argument("Image::format: image has unknown format");
        }
    }

    struct Extent {
        std::uint32_t width;
        std::uint32_t height;
        Extent(std::uint32_t width, std::uint32_t height)
            : width(width), height(height) {}
    };

    Extent get_extent() const {
        return { width, height };
    }

    static Image uninitialized(Extent extent, Format format) {
        Image res;
        res.width = extent.width;
        res.height = extent.height;
        switch (format) {
            case Format::G1:
                res.channels = 1;
                res.bit_depth = 1;
                break;
            case Format::G2:
                res.channels = 1;
                res.bit_depth = 2;
                break;
            case Format::G4:
                res.channels = 1;
                res.bit_depth = 4;
                break;
            case Format::G8:
                res.channels = 1;
                res.bit_depth = 8;
                break;
            case Format::G16:
                res.channels = 1;
                res.bit_depth = 16;
                break;

            case Format::GA8:
                res.channels = 2;
                res.bit_depth = 8;
                break;
            case Format::GA16:
                res.channels = 2;
                res.bit_depth = 16;
                break;

            case Format::RGB8:
                res.channels = 3;
                res.bit_depth = 8;
                break;
            case Format::RGB16:
                res.channels = 3;
                res.bit_depth = 16;
                break;

            case Format::RGBA8:
                res.channels = 4;
                res.bit_depth = 8;
                break;
            case Format::RGBA16:
                res.channels = 4;
                res.bit_depth = 16;
                break;

            default: throw std::logic_error("Image::zeros: unimplemented format");
        }
        res.size = res.row_stride() * res.height;
        res.allocate();
        return res;
    }

    static std::expected<Image, Error> read_png(const std::string& path) {
        std::FILE* file = std::fopen(path.c_str(), "rb");
        if (file == nullptr)
            return std::unexpected(Error::IO);

        slp_png_io io{};
        io.buf = file;

        Image image;
        int res = slp_png_read(static_cast<slp_image_t*>(&image), &io);
        fclose(file);

        if (res == 0)
            return image;
        switch (res) {
            case ALLOC_ERR: throw std::runtime_error("Image::read_png: allocation error");
            case IO_ERR: return std::unexpected(Error::IO);
            case INVALID_PNG:
            case ZLIB_ERR: return std::unexpected(Error::INVALID_PNG);
            case NULL_ARGS:
            default: throw std::logic_error("Image::read_png: internal error, pls create an issue");
        }
    }

    std::expected<void, Error> write_png(const std::string& path) {
        std::FILE* file = std::fopen(path.c_str(), "wb");
        if (file == nullptr)
            return std::unexpected(Error::IO);

        slp_png_io io{};
        io.buf = file;

        int res = slp_png_write(static_cast<slp_image_t*>(this), &io);
        fclose(file);

        if (res == 0)
            return {};
        switch (res) {
            case ALLOC_ERR: throw std::runtime_error("Image::write_png: allocation error");
            case IO_ERR: return std::unexpected(Error::IO);
            case INVALID_PNG:
            case ZLIB_ERR: return std::unexpected(Error::INVALID_PNG);
            case NULL_ARGS:
            default: throw std::logic_error("Image::write_png: internal error, pls create an issue");
        }
    }

    void to_rgba8() {
        if (slp_image_convert_to_RGBA8(static_cast<slp_image_t*>(this)) != 0)
            throw std::runtime_error("Image::to_rgba8: allocation failed or unknown image format");
    }

    void linear_transform(const double matrix[2][2]) {
        if (slp_image_unpack(static_cast<slp_image_t*>(this)) != 0) throw std::runtime_error("Image::linear_transform: allocation failed or unknown image format");
        *this = static_cast<Image>(slp_image_linear_transform(static_cast<slp_image_t*>(this), matrix));
        if (this->pixels == nullptr && this->size != 0) throw std::runtime_error("Image::linear_transform: allocation failed");
        if (slp_image_pack(static_cast<slp_image_t*>(this)) != 0) throw std::runtime_error("Image::linear_transform: allocation failed");
    }

    void crop(Extent offset, Extent extent) {
        if (slp_image_unpack(static_cast<slp_image_t*>(this)) != 0) throw std::runtime_error("Image::crop: allocation failed or unknown image format");
        *this = static_cast<Image>(slp_image_crop(static_cast<slp_image_t*>(this), extent.width, extent.height, offset.width, offset.height));
        if (this->pixels == nullptr && this->size != 0) throw std::runtime_error("Image::crop: allocation failed");
        if (slp_image_pack(static_cast<slp_image_t*>(this)) != 0) throw std::runtime_error("Image::crop: allocation failed");
    }
};
};  // namespace slp
