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
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <istream>

#include "slp_png.hpp"

namespace slp {

struct Ihdr {
    std::uint32_t width;
    std::uint32_t height;
    std::uint8_t bit_depth;
    std::uint8_t color_type;
    std::uint8_t compression_method;
    std::uint8_t filter_method;
    std::uint8_t interlace_method;
};

struct CheckList {
    bool plte = false;
    bool trns = false;
    bool idat = false;
    bool iend = false;
    bool ihdr = false;
};

struct ChunkInfo {
    Image& image;
    std::istream& io;
    Ihdr ihdr;
    // [0: chunk_length][4: chunk_type][8: crc]
    std::array<std::uint8_t, 12> worker;
    CheckList check_list;

    ChunkInfo(Image& image, std::istream& io) : image(image), io(io) {}
};

auto read_png_tag(std::istream& io) -> std::expected<void, Error>;
auto read_ihdr(ChunkInfo& info, std::uint32_t width_limit, std::uint32_t height_limit)
    -> std::expected<void, Error>;
auto read_plte(ChunkInfo& info) -> std::expected<void, Error>;
auto read_trns(ChunkInfo& info) -> std::expected<void, Error>;
auto read_idat(ChunkInfo& info) -> std::expected<void, Error>;
auto read_iend(ChunkInfo& info) -> std::expected<void, Error>;

auto defilter(const std::uint8_t* __restrict buffer, std::uint8_t* __restrict cur,
              const std::uint8_t* __restrict prev, std::size_t bpp, std::size_t bpr)
    -> std::expected<void, Error>;

#define CHUNK(x0, x1, x2, x3)                                                      \
    (static_cast<std::uint32_t>(x0) << 24 | static_cast<std::uint32_t>(x1) << 16 | \
     static_cast<std::uint32_t>(x2) << 8 | static_cast<std::uint32_t>(x3) << 0)
enum ChunkType : std::uint32_t {
    IHDR = CHUNK('I', 'H', 'D', 'R'),
    IDAT = CHUNK('I', 'D', 'A', 'T'),
    IEND = CHUNK('I', 'E', 'N', 'D'),
    PLTE = CHUNK('P', 'L', 'T', 'E'),
    tRNS = CHUNK('t', 'R', 'N', 'S'),
};
#undef CHUNK

}  // namespace slp
