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
#include "write.hpp"

#include <zlib.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <ios>
#include <iterator>
#include <ostream>
#include <vector>

#include "extras.hpp"
#include "slp_png.hpp"

static constexpr std::size_t IDAT_BUFFER_SIZE = 65536;

namespace slp {

auto write_png_tag(std::ostream& io) -> std::expected<void, Error> {
    const std::uint64_t tag = to_be_u64(PNG_TAG);
    if (!io.write(reinterpret_cast<const char*>(&tag), 8)) {
        return std::unexpected(Error::IO);
    }
    return {};
}

auto write_ihdr(const Image& image, std::ostream& io) -> std::expected<void, Error> {
#pragma pack(push, 1)
    struct Ihdr {
        uint32_t width;
        uint32_t height;
        uint8_t bit_depth;
        uint8_t color_type;
        uint8_t compression_method;
        uint8_t filter_method;
        uint8_t interlace_method;
    };
#pragma pack(pop)

    Ihdr ihdr = {
        .width = to_be_u32(image.width()),
        .height = to_be_u32(image.height()),
        .bit_depth = image.format().bit_depth(),
        .color_type = image.format().color_type(),
        .compression_method = 0,
        .filter_method = 0,
        .interlace_method = 0,
    };

    std::uint32_t crc = crc32(0, reinterpret_cast<const std::uint8_t*>("IHDR"), 4);
    crc = crc32(crc, reinterpret_cast<const std::uint8_t*>(&ihdr), 13);
    crc = to_be_u32(crc);

    std::uint32_t ihdr_len = to_be_u32(13);

    if (!io.write(reinterpret_cast<const char*>(&ihdr_len), 4) || !io.write("IHDR", 4) ||
        !io.write(reinterpret_cast<const char*>(&ihdr), 13) ||
        !io.write(reinterpret_cast<const char*>(&crc), 4)) {
        return std::unexpected(Error::IO);
    }

    return {};
}

auto write_plte(const Image& image, std::ostream& io) -> std::expected<void, Error> {
    const std::size_t entries = image.palette().size() / 4;
    if (entries > std::size_t{1} << image.format().bit_depth()) {
        return std::unexpected(Error::INVALID_PNG);
    }

    std::vector<std::uint8_t> plte((entries * 3) + 12);

    const std::uint32_t chunk_len = to_be_u32(static_cast<std::uint32_t>(entries * 3));
    std::memcpy(plte.data(), &chunk_len, 4);
    std::memcpy(plte.data() + 4, "PLTE", 4);

    for (std::size_t i = 0; i < entries; i++) {
        std::memcpy(plte.data() + 8 + (i * 3), image.palette().data() + (i * 4), 3);
    }

    std::uint32_t crc = crc32(0, plte.data() + 4, 4 + (entries * 3));
    crc = to_be_u32(crc);
    std::memcpy(plte.data() + 8 + (entries * 3), &crc, 4);

    if (!io.write(reinterpret_cast<const char*>(plte.data()),
                  static_cast<std::streamsize>(plte.size()))) {
        return std::unexpected(Error::IO);
    }

    return {};
}

auto write_trns(const Image& image, std::ostream& io) -> std::expected<void, Error> {
    const std::size_t entries = image.palette().size() / 4;
    if (entries > std::size_t{1} << image.format().bit_depth()) {
        return std::unexpected(Error::INVALID_PNG);
    }

    std::vector<std::uint8_t> trns(entries + 12);

    const std::uint32_t chunk_len = to_be_u32(static_cast<std::uint32_t>(entries));
    std::memcpy(trns.data(), &chunk_len, 4);
    std::memcpy(trns.data() + 4, "tRNS", 4);

    for (std::size_t i = 0; i < entries; i++) {
        trns[8 + i] = image.palette()[(i * 4) + 3];
    }

    std::uint32_t crc = crc32(0, trns.data() + 4, 4 + entries);
    crc = to_be_u32(crc);
    std::memcpy(trns.data() + 8 + entries, &crc, 4);

    if (!io.write(reinterpret_cast<const char*>(trns.data()),
                  static_cast<std::streamsize>(trns.size()))) {
        return std::unexpected(Error::IO);
    }

    return {};
}

auto write_idat(const Image& image, std::ostream& io) -> std::expected<void, Error> {  // NOLINT
    std::array<std::vector<std::uint8_t>, 5> filter_buffers;
    for (std::size_t i = 0; i < filter_buffers.size(); i++) {
        filter_buffers[i].resize(image.bytes_per_row() + 1);
        filter_buffers[i][0] = i;
    }

    std::vector<std::uint8_t> idat(IDAT_BUFFER_SIZE + 12);
    std::uint32_t chunk_len = to_be_u32(IDAT_BUFFER_SIZE);
    std::memcpy(idat.data(), &chunk_len, 4);
    std::memcpy(idat.data() + 4, "IDAT", 4);

    Deflate::Stream strm;
    {
        auto res = Deflate::stream_alloc();
        if (!res) return std::unexpected(res.error());
        strm = std::move(res.value());
    }

    strm->avail_out = IDAT_BUFFER_SIZE;
    strm->next_out = idat.data() + 8;

    for (std::size_t i = 0; i < image.height(); i++) {
        std::array<std::uint64_t, 5> filter_scores;
        filter(image, filter_buffers, filter_scores, i);

        std::uint8_t filter =
            std::distance(filter_scores.begin(), std::ranges::min_element(filter_scores));

        strm->next_in = filter_buffers[filter].data();
        strm->avail_in = filter_buffers[filter].size();

        while (strm->avail_in > 0) {
            int ret = deflate(strm.get(), Z_NO_FLUSH);
            if (ret != Z_OK) {
                return std::unexpected(Error::INTERNAL_ERROR);
            }

            if (strm->avail_out == 0) {
                std::uint32_t crc = crc32(0, idat.data() + 4, IDAT_BUFFER_SIZE + 4);
                crc = to_be_u32(crc);
                std::memcpy(idat.data() + 8 + IDAT_BUFFER_SIZE, &crc, 4);

                if (!io.write(reinterpret_cast<const char*>(idat.data()),
                              static_cast<std::streamsize>(idat.size()))) {
                    return std::unexpected(Error::IO);
                }

                strm->avail_out = IDAT_BUFFER_SIZE;
                strm->next_out = idat.data() + 8;
            }
        }
    }

    int ret = Z_OK;
    while (ret != Z_STREAM_END) {
        ret = deflate(strm.get(), Z_FINISH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            return std::unexpected(Error::INTERNAL_ERROR);
        }

        if (strm->avail_out == 0) {
            std::uint32_t crc = crc32(0, idat.data() + 4, IDAT_BUFFER_SIZE + 4);
            crc = to_be_u32(crc);
            std::memcpy(idat.data() + 8 + IDAT_BUFFER_SIZE, &crc, 4);

            if (!io.write(reinterpret_cast<const char*>(idat.data()),
                          static_cast<std::streamsize>(idat.size()))) {
                return std::unexpected(Error::IO);
            }

            strm->avail_out = IDAT_BUFFER_SIZE;
            strm->next_out = idat.data() + 8;
        }
    }

    // flush last chunk
    std::size_t n = IDAT_BUFFER_SIZE - strm->avail_out;
    std::uint32_t crc = crc32(0, idat.data() + 4, 4 + n);
    chunk_len = to_be_u32(n);
    crc = to_be_u32(crc);
    if (!io.write(reinterpret_cast<const char*>(&chunk_len), 4) ||
        !io.write(reinterpret_cast<const char*>(idat.data() + 4),
                  static_cast<std::streamsize>(n + 4)) ||
        !io.write(reinterpret_cast<const char*>(&crc), 4)) {
        return std::unexpected(Error::IO);
    }

    return {};
}

auto write_iend(std::ostream& io) -> std::expected<void, Error> {
    static constexpr std::array<uint8_t, 12> IENDsig = {
        0,    0,    0,    0,     // chunk length
        'I',  'E',  'N',  'D',   // chunk type
        0xAE, 0x42, 0x60, 0x82,  // CRC
    };
    if (!io.write(reinterpret_cast<const char*>(IENDsig.data()), IENDsig.size())) {
        return std::unexpected(Error::IO);
    }
    return {};
}

}  // namespace slp