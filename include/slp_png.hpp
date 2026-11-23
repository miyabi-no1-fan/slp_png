/*
Copyright 2026 miyabi-no1-fan

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#pragma once
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <istream>
#include <optional>
#include <ostream>
#include <utility>
#include <vector>

namespace slp {

enum class Error : std::int8_t {
    IO = -1,
    INVALID_PNG = -2,
    INTERNAL_ERROR = -3,
};

inline auto operator<<(std::ostream& os, Error self) -> std::ostream& {
    switch (self) {
        case Error::IO: return os << "Error::IO";
        case Error::INVALID_PNG: return os << "Error::INVALID_PNG";
        case Error::INTERNAL_ERROR: return os << "Error::INTERNAL_ERROR";
    }
    std::unreachable();
}

class Format {
   public:
    // clang-format off
    enum Type : std::uint8_t {
        G1, G2, G4, G8, G16,
        Index1, Index2, Index4, Index8,
        GA8, GA16,
        RGB8, RGB16,
        RGBA8, RGBA16,
    };
    // clang-format on

    constexpr Format(Type format) : format(format) {}

    [[nodiscard]] constexpr auto value() const -> Type { return format; }

    [[nodiscard]] constexpr auto bit_depth() const -> std::uint8_t {
        // clang-format off
        switch (format) {
            case G1: case Index1: return 1;
            case G2: case Index2: return 2;
            case G4: case Index4: return 4;
            case G8: case Index8: case GA8: case RGB8: case RGBA8: return 8;
            case G16: case GA16: case RGB16: case RGBA16: return 16;
        }
        // clang-format on
        std::unreachable();
    }

    [[nodiscard]] constexpr auto channels() const -> std::uint8_t {
        // clang-format off
        switch (format) {
            case G1: case G2: case G4: case G8: case G16:
            case Index1: case Index2: case Index4: case Index8: return 1;
            case GA8: case GA16: return 2;
            case RGB8: case RGB16: return 3;
            case RGBA8: case RGBA16: return 4;
        }
        // clang-format on
        std::unreachable();
    }

    [[nodiscard]] constexpr auto color_type() const -> std::uint8_t {
        // clang-format off
        switch (format) {
            case Format::G1: case Format::G2: case Format::G4: case Format::G8: case Format::G16: return 0;
            case Format::RGB8: case Format::RGB16: return 2;
            case Format::Index1: case Format::Index2: case Format::Index4: case Format::Index8: return 3;
            case Format::GA8: case Format::GA16: return 4;
            case Format::RGBA8: case Format::RGBA16: return 6;
        }
        // clang-format on
        std::unreachable();
    }

    static constexpr auto guess_format(std::uint8_t bit_depth, std::uint8_t color_type)
        -> std::optional<Format> {
        switch (color_type) {
            case 0:
                switch (bit_depth) {
                    case 1: return Format::G1;
                    case 2: return Format::G2;
                    case 4: return Format::G4;
                    case 8: return Format::G8;
                    case 16: return Format::G16;
                    default: return std::nullopt;
                }

            case 2:
                switch (bit_depth) {
                    case 8: return Format::RGB8;
                    case 16: return Format::RGB16;
                    default: return std::nullopt;
                }

            case 3:
                switch (bit_depth) {
                    case 1: return Format::Index1;
                    case 2: return Format::Index2;
                    case 4: return Format::Index4;
                    case 8: return Format::Index8;
                    default: return std::nullopt;
                }

            case 4:
                switch (bit_depth) {
                    case 8: return Format::GA8;
                    case 16: return Format::GA16;
                    default: return std::nullopt;
                }

            case 6:
                switch (bit_depth) {
                    case 8: return Format::RGBA8;
                    case 16: return Format::RGBA16;
                    default: return std::nullopt;
                }

            default: return std::nullopt;
        }
    }

    [[nodiscard]] constexpr auto is_indexed_color() const -> bool {
        // clang-format off
        switch (format) {
            case Index1: case Index2: case Index4: case Index8: return true;
            default: return false;
        }
        // clang-format on
    }

    friend auto operator<<(std::ostream& os, Format self) -> std::ostream& {
        switch (self.format) {
            case G1: return os << "Format::G1";
            case G2: return os << "Format::G2";
            case G4: return os << "Format::G4";
            case G8: return os << "Format::G8";
            case G16: return os << "Format::G16";
            case Index1: return os << "Format::Index1";
            case Index2: return os << "Format::Index2";
            case Index4: return os << "Format::Index4";
            case Index8: return os << "Format::Index8";
            case GA8: return os << "Format::GA8";
            case GA16: return os << "Format::GA16";
            case RGB8: return os << "Format::RGB8";
            case RGB16: return os << "Format::RGB16";
            case RGBA8: return os << "Format::RGBA8";
            case RGBA16: return os << "Format::RGBA16";
        }
        std::unreachable();
    }

   private:
    Type format;
};

class Image {
    std::vector<std::uint8_t> pixels_;
    std::vector<std::uint8_t> palette_;
    std::uint32_t height_ = 0;
    std::uint32_t width_ = 0;
    Format format_ = Format::RGBA8;

   public:
    Image() = default;

    static constexpr auto zeros(std::uint32_t width, std::uint32_t height, Format format) -> Image {
        Image image;
        image.width_ = width;
        image.height_ = height;
        image.format_ = format;
        image.pixels_ = std::vector<std::uint8_t>(image.bytes_per_row() * height, 0);
        return image;
    }

    Image(const Image& o) = default;
    auto operator=(const Image& o) -> Image& = default;
    Image(Image&&) noexcept = default;
    auto operator=(Image&&) noexcept -> Image& = default;

    [[nodiscard]] constexpr auto width() const -> std::size_t {
        return static_cast<std::size_t>(width_);
    }
    [[nodiscard]] constexpr auto height() const -> std::size_t {
        return static_cast<std::size_t>(height_);
    }
    [[nodiscard]] constexpr auto format() const -> Format { return format_; }
    [[nodiscard]] constexpr auto bytes_per_row() const -> std::size_t {
        return div_ceil(width() * format().channels() * format().bit_depth(), 8);
    }

    [[nodiscard]] constexpr auto pixels() const -> const std::vector<std::uint8_t>& {
        return pixels_;
    }
    [[nodiscard]] constexpr auto pixels() -> std::vector<std::uint8_t>& { return pixels_; }

    [[nodiscard]] constexpr auto palette() const -> const std::vector<std::uint8_t>& {
        return palette_;
    }
    [[nodiscard]] constexpr auto palette() -> std::vector<std::uint8_t>& { return palette_; }

    [[nodiscard]] static auto read_png(std::istream& io, std::uint32_t width_limit = 12288,
                                       std::uint32_t height_limit = 6480)
        -> std::expected<Image, Error>;

    [[nodiscard]] auto write_png(std::ostream& io) const -> std::expected<void, Error>;

    [[nodiscard]] static auto read_png(const std::filesystem::path& path,
                                       std::uint32_t width_limit = 12288,
                                       std::uint32_t height_limit = 6480)
        -> std::expected<Image, Error>;

    [[nodiscard]] auto write_png(const std::filesystem::path& path) const
        -> std::expected<void, Error>;

    constexpr void overwrite_width(std::uint32_t width) { width_ = width; }
    constexpr void overwrite_height(std::uint32_t height) { height_ = height; }
    constexpr void overwrite_format(Format foramt) { format_ = foramt; }
    constexpr void overwrite_pixels(const std::vector<std::uint8_t>& pixels) { pixels_ = pixels; }
    constexpr void overwrite_palette(const std::vector<std::uint8_t>& palette) {
        palette_ = palette;
    }

   private:
    template <typename A, typename B = A, typename C = A>
    static constexpr auto div_ceil(A dividend, B divisor) -> C {
        return static_cast<C>(dividend / divisor) + static_cast<C>(dividend % divisor != 0);
    }
};

}  // namespace slp
