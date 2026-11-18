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
#include <zconf.h>
#include <zlib.h>

#include <bit>
#include <cstdint>
#include <expected>
#include <memory>

#include "slp_png.hpp"

namespace Inflate {
struct Deleter {
    void operator()(z_stream* strm) const {
        inflateEnd(strm);
        delete strm;
    }
};
using Stream = std::unique_ptr<z_stream, Deleter>;
static inline auto stream_alloc() -> std::expected<Stream, slp::Error> {
    auto strm = static_cast<Stream>(new z_stream{});
    int ret = inflateInit(strm.get());
    if (ret != Z_OK) return std::unexpected(slp::Error::INTERNAL_ERROR);
    return strm;
}
}  // namespace Inflate

namespace Deflate {
struct Deleter {
    void operator()(z_stream* strm) const {
        deflateEnd(strm);
        delete strm;
    }
};
using Stream = std::unique_ptr<z_stream, Deleter>;
static inline auto stream_alloc() -> std::expected<Stream, slp::Error> {
    auto strm = static_cast<Stream>(new z_stream{});
    int ret = deflateInit2(strm.get(), Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS, 8, Z_FILTERED);
    if (ret != Z_OK) return std::unexpected(slp::Error::INTERNAL_ERROR);
    return strm;
}
}  // namespace Deflate

namespace slp {

static constexpr std::uint64_t PNG_TAG = 0x89504E470D0A1A0A;

static constexpr auto from_be_u32(const std::uint8_t* buf) -> std::uint32_t {
    return static_cast<std::uint32_t>(buf[0]) << 24 | static_cast<std::uint32_t>(buf[1]) << 16 |
           static_cast<std::uint32_t>(buf[2]) << 8 | static_cast<std::uint32_t>(buf[3]) << 0;
}

static constexpr auto from_be_u64(const std::uint8_t* buf) -> std::uint64_t {
    return static_cast<std::uint64_t>(buf[0]) << 56 | static_cast<std::uint64_t>(buf[1]) << 48 |
           static_cast<std::uint64_t>(buf[2]) << 40 | static_cast<std::uint64_t>(buf[3]) << 32 |
           static_cast<std::uint64_t>(buf[4]) << 24 | static_cast<std::uint64_t>(buf[5]) << 16 |
           static_cast<std::uint64_t>(buf[6]) << 8 | static_cast<std::uint64_t>(buf[7]) << 0;
}

static inline auto to_be_u32(std::uint32_t x) -> std::uint32_t {
    std::uint16_t v = 1;
    if (*reinterpret_cast<std::uint8_t*>(&v) != 0) {
        return std::byteswap(x);
    }
    return x;
}

static inline auto to_be_u64(uint64_t x) -> uint64_t {
    uint16_t v = 1;
    if (*reinterpret_cast<std::uint8_t*>(&v) != 0) {
        return std::byteswap(x);
    }
    return x;
}

template <typename A, typename B = A, typename C = A>
static constexpr auto div_ceil(A dividend, B divisor) -> C {
    return static_cast<C>(dividend / divisor) + static_cast<C>(dividend % divisor != 0);
}

}  // namespace slp
