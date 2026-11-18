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
#include "read.hpp"

#include <zlib.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <ios>
#include <istream>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "extras.hpp"
#include "slp_png.hpp"

static constexpr std::size_t IDAT_BUFFER_SIZE = 65536;

namespace slp {

auto read_png_tag(std::istream& io) -> std::expected<void, Error> {
    std::array<std::uint8_t, 8> signature;
    if (!io.read(reinterpret_cast<char*>(signature.data()), signature.size())) {
        return std::unexpected(Error::IO);
    }
    if (from_be_u64(signature.data()) != PNG_TAG) {
        return std::unexpected(Error::INVALID_PNG);
    }
    return {};
}

auto read_ihdr(ChunkInfo& info, std::uint32_t width_limit, std::uint32_t height_limit)
    -> std::expected<void, Error> {
    if (info.check_list.ihdr) {
        return std::unexpected(Error::INVALID_PNG);
    }
    info.check_list.ihdr = true;

    std::uint32_t chunk_length = from_be_u32(info.worker.data());

    if (chunk_length != 13) {
        return std::unexpected(Error::INVALID_PNG);
    }

    std::array<std::uint8_t, 13> chunk_data;
    if (!info.io.read(reinterpret_cast<char*>(chunk_data.data()), chunk_data.size())) {
        return std::unexpected(Error::IO);
    }

    std::uint32_t crc = crc32(0, info.worker.data() + 4, 4);
    crc = crc32(crc, chunk_data.data(), chunk_data.size());
    if (!info.io.read(reinterpret_cast<char*>(info.worker.data() + 8), 4)) {
        return std::unexpected(Error::IO);
    }
    if (from_be_u32(info.worker.data() + 8) != crc) {
        return std::unexpected(Error::INVALID_PNG);
    }

    info.ihdr.width = from_be_u32(chunk_data.data());
    info.ihdr.height = from_be_u32(chunk_data.data() + 4);
    info.ihdr.bit_depth = chunk_data[8];
    info.ihdr.color_type = chunk_data[9];
    info.ihdr.compression_method = chunk_data[10];
    info.ihdr.filter_method = chunk_data[11];
    info.ihdr.interlace_method = chunk_data[12];

    if (info.ihdr.width == 0 || info.ihdr.height == 0 || info.ihdr.compression_method != 0 ||
        info.ihdr.filter_method != 0 || info.ihdr.interlace_method != 0 ||
        info.ihdr.width > width_limit || info.ihdr.height > height_limit) {
        return std::unexpected(Error::INVALID_PNG);
    }

    {
        auto format = Format::guess_format(info.ihdr.bit_depth, info.ihdr.color_type);
        if (!format) return std::unexpected(Error::INVALID_PNG);

        info.image.overwrite_width(info.ihdr.width);
        info.image.overwrite_height(info.ihdr.height);
        info.image.overwrite_format(format.value());
        info.image.pixels().resize(info.image.bytes_per_row() * info.image.height(), 0);
    }

    return {};
}

auto read_plte(ChunkInfo& info) -> std::expected<void, Error> {
    if (info.check_list.plte || !info.check_list.ihdr) {
        return std::unexpected(Error::INVALID_PNG);
    }
    info.check_list.plte = true;

    std::uint32_t chunk_length = from_be_u32(info.worker.data());

    if (chunk_length > 256 * 3) {
        // max chunk length for a PLTE is for color type 3 with 256 entries of RGB
        return std::unexpected(Error::INVALID_PNG);
    }

    std::vector<std::uint8_t> plte(chunk_length);
    if (!info.io.read(reinterpret_cast<char*>(plte.data()), chunk_length)) {
        return std::unexpected(Error::IO);
    }

    std::uint32_t crc = crc32(0, info.worker.data() + 4, 4);
    crc = crc32(crc, plte.data(), chunk_length);
    if (!info.io.read(reinterpret_cast<char*>(info.worker.data() + 8), 4)) {
        return std::unexpected(Error::IO);
    }
    if (from_be_u32(info.worker.data() + 8) != crc) {
        return std::unexpected(Error::INVALID_PNG);
    }

    if (info.ihdr.color_type == 3) {
        if (chunk_length % 3 != 0 || chunk_length / 3 > std::size_t{1}
                                                            << info.image.format().bit_depth()) {
            return std::unexpected(Error::INVALID_PNG);
        }
        info.image.palette().resize(chunk_length * 4 / 3);
        for (std::size_t i = 0, k = 0; k + 3 <= chunk_length; i += 4, k += 3) {
            info.image.palette()[i + 0] = plte[k + 0];
            info.image.palette()[i + 1] = plte[k + 1];
            info.image.palette()[i + 2] = plte[k + 2];
            info.image.palette()[i + 3] = 255;
        }
    }

    return {};
}

auto read_trns(ChunkInfo& info) -> std::expected<void, Error> {
    if (info.check_list.trns || !info.check_list.ihdr) {
        return std::unexpected(Error::INVALID_PNG);
    }
    info.check_list.trns = true;

    std::uint32_t chunk_length = from_be_u32(info.worker.data());

    if (chunk_length > 256) {
        // max chunk length for a tRNS is for color type 3 with 256 entries of alpha
        // channel
        return std::unexpected(Error::INVALID_PNG);
    }

    std::vector<std::uint8_t> trns(chunk_length);
    if (!info.io.read(reinterpret_cast<char*>(trns.data()), chunk_length)) {
        return std::unexpected(Error::IO);
    }

    std::uint32_t crc = crc32(0, info.worker.data() + 4, 4);
    crc = crc32(crc, trns.data(), chunk_length);
    if (!info.io.read(reinterpret_cast<char*>(info.worker.data() + 8), 4)) {
        return std::unexpected(Error::IO);
    }
    if (from_be_u32(info.worker.data() + 8) != crc) {
        return std::unexpected(Error::INVALID_PNG);
    }

    if (info.ihdr.color_type == 3) {
        if (!info.check_list.plte || chunk_length > info.image.palette().size() / 4) {
            return std::unexpected(Error::INVALID_PNG);
        }
        for (std::size_t i = 0; i < chunk_length; i++) {
            info.image.palette()[(i * 4) + 3] = trns[i];
        }
    }

    return {};
}

auto read_idat(ChunkInfo& info) -> std::expected<void, Error> {  // NOLINT
    // inconsecutive IDAT is error
    if (info.check_list.idat || !info.check_list.ihdr) {
        return std::unexpected(Error::INVALID_PNG);
    }
    info.check_list.idat = true;

    std::uint32_t chunk_length = from_be_u32(info.worker.data());
    std::uint32_t chunk_type = IDAT;

    Inflate::Stream strm;
    {
        auto res = Inflate::stream_alloc();
        if (!res) return std::unexpected(res.error());
        strm = std::move(res.value());
    }

    const std::size_t bpp = info.image.format().channels() *
                            div_ceil(static_cast<std::size_t>(info.image.format().bit_depth()), 8);
    const std::size_t bpr = info.image.bytes_per_row();

    // decoded line
    std::vector<std::uint8_t> decoded(bpr + 1);

    strm->avail_out = decoded.size();
    strm->next_out = decoded.data();

    std::size_t current_row = 0;

    auto drain = [&]() -> std::expected<void, Error> {
        if (strm->avail_out == 0) {
            if (current_row > info.image.height()) {
                return std::unexpected(Error::INVALID_PNG);
            }

            auto* cur = info.image.pixels().data() + (current_row * bpr);
            auto* prev = current_row > 0 ? cur - bpr : nullptr;

            auto res = defilter(decoded.data(), cur, prev, bpp, bpr);
            if (!res) {
                return std::unexpected(res.error());
            }

            current_row++;
            strm->avail_out = decoded.size();
            strm->next_out = decoded.data();
        }
        return {};
    };

    // fixed size buffer to parse IDAT chunks
    std::vector<std::uint8_t> raw(IDAT_BUFFER_SIZE);
    auto raw_it = raw.begin();

    while (chunk_type == IDAT) {
        std::uint32_t crc = crc32(0, info.worker.data() + 4, 4);

        // chunk_length > remaining raw capacity
        while (chunk_length > std::distance(raw_it, raw.end())) {
            // read as much as `raw_it` have left
            if (!info.io.read(reinterpret_cast<char*>(raw_it.base()),
                              std::distance(raw_it, raw.end()))) {
                return std::unexpected(Error::IO);
            }

            crc = crc32(crc, raw_it.base(), std::distance(raw_it, raw.end()));
            chunk_length -= std::distance(raw_it, raw.end());
            raw_it = raw.end();  // `raw` buffer is full now

            // flush `raw`
            strm->avail_in = std::distance(raw.begin(), raw_it);
            strm->next_in = raw.data();

            while (strm->avail_in > 0) {
                int ret = inflate(strm.get(), Z_NO_FLUSH);
                if (ret != Z_OK) {
                    return std::unexpected(Error::INVALID_PNG);
                }
                auto res = drain();
                if (!res) return std::unexpected(res.error());
            }

            raw_it = raw.begin();
        }

        if (!info.io.read(reinterpret_cast<char*>(raw_it.base()), chunk_length)) {
            return std::unexpected(Error::IO);
        }
        crc = crc32(crc, raw_it.base(), chunk_length);
        raw_it += chunk_length;

        if (!info.io.read(reinterpret_cast<char*>(info.worker.data() + 8), 4)) {
            return std::unexpected(Error::IO);
        }
        if (from_be_u32(info.worker.data() + 8) != crc) {
            return std::unexpected(Error::INVALID_PNG);
        }

        if (!info.io.read(reinterpret_cast<char*>(info.worker.data()), 8)) {
            return std::unexpected(Error::IO);
        }
        chunk_length = from_be_u32(info.worker.data());
        chunk_type = from_be_u32(info.worker.data() + 4);
    }

    if (!info.io.seekg(-8, std::ios::cur)) {
        return std::unexpected(Error::IO);
    }

    // flush
    strm->avail_in = std::distance(raw.begin(), raw_it);
    strm->next_in = raw.data();

    int ret = Z_OK;

    while (ret != Z_STREAM_END) {
        ret = inflate(strm.get(), Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            return std::unexpected(Error::INVALID_PNG);
        }
        auto res = drain();
        if (!res) return std::unexpected(res.error());
    }

    // optional: check strm->avail_out

    if (current_row != info.image.height()) {
        return std::unexpected(Error::INVALID_PNG);
    }

    return {};
}

auto read_iend(ChunkInfo& info) -> std::expected<void, Error> {
    if (info.check_list.iend || !info.check_list.ihdr || !info.check_list.idat ||
        (info.ihdr.color_type == 3 && !info.check_list.plte)) {
        return std::unexpected(Error::INVALID_PNG);
    }
    info.check_list.iend = true;

    std::uint32_t chunk_length = from_be_u32(info.worker.data());

    if (chunk_length != 0) {
        return std::unexpected(Error::INVALID_PNG);
    }

    std::uint32_t crc = crc32(0, info.worker.data() + 4, 4);
    if (!info.io.read(reinterpret_cast<char*>(info.worker.data() + 8), 4)) {
        return std::unexpected(Error::IO);
    }
    if (from_be_u32(info.worker.data() + 8) != crc) {
        return std::unexpected(Error::INVALID_PNG);
    }

    return {};
}

}  // namespace slp