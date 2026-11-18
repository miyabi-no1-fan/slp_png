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
#include "slp_png.hpp"

#include <cstdint>
#include <expected>
#include <fstream>
#include <ios>
#include <istream>
#include <ostream>

#include "extras.hpp"
#include "read.hpp"
#include "write.hpp"

namespace slp {

[[nodiscard]] auto Image::write_png(const std::filesystem::path& path) const
    -> std::expected<void, Error> {
    std::ofstream file(path, std::ios::binary);
    return write_png(file);
}

#define TRY(expr)                                      \
    do {                                               \
        auto res = (expr);                             \
        if (!res) return std::unexpected(res.error()); \
    } while (0)

[[nodiscard]] auto Image::write_png(std::ostream& io) const -> std::expected<void, Error> {
    if (pixels().size() != bytes_per_row() * height()) {
        return std::unexpected(Error::INVALID_PNG);
    }

    TRY(write_png_tag(io));
    TRY(write_ihdr(*this, io));
    if (format().is_indexed_color()) {
        TRY(write_plte(*this, io));
        TRY(write_trns(*this, io));
    }
    TRY(write_idat(*this, io));
    TRY(write_iend(io));

    return {};
}

#undef TRY

[[nodiscard]] auto Image::read_png(const std::filesystem::path& path, std::uint32_t width_limit,
                                   std::uint32_t height_limit) -> std::expected<Image, Error> {
    std::ifstream file(path, std::ios::binary);
    return Image::read_png(file, width_limit, height_limit);
}

[[nodiscard]] auto Image::read_png(std::istream& io, std::uint32_t width_limit,
                                   std::uint32_t height_limit) -> std::expected<Image, Error> {
    auto res = read_png_tag(io);
    if (!res) return std::unexpected(res.error());

    Image image;
    ChunkInfo chunk_info(image, io);

    while (!chunk_info.check_list.iend) {
        if (!io.read(reinterpret_cast<char*>(chunk_info.worker.data()), 8)) {
            return std::unexpected(Error::IO);
        }

        std::uint32_t chunk_type = from_be_u32(chunk_info.worker.data() + 4);

        switch (chunk_type) {
            case IHDR: {
                auto res = read_ihdr(chunk_info, width_limit, height_limit);
                if (!res) return std::unexpected(res.error());
                break;
            }

            case PLTE: {
                auto res = read_plte(chunk_info);
                if (!res) return std::unexpected(res.error());
                break;
            }

            case tRNS: {
                auto res = read_trns(chunk_info);
                if (!res) return std::unexpected(res.error());
                break;
            }

            case IDAT: {
                auto res = read_idat(chunk_info);
                if (!res) return std::unexpected(res.error());
                break;
            }

            case IEND: {
                auto res = read_iend(chunk_info);
                if (!res) return std::unexpected(res.error());
                break;
            }

            default: {
                std::uint32_t chunk_length = from_be_u32(chunk_info.worker.data());
                if (!io.seekg(chunk_length + 4, std::ios::cur)) {
                    return std::unexpected(Error::IO);
                }
            }
        }
    }

    return image;
}

}  // namespace slp