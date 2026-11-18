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
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <expected>
#include <vector>

#if defined(__i386__) || defined(__x86_64__)
#include <immintrin.h>
#endif

#include "read.hpp"
#include "slp_png.hpp"

namespace slp {

static auto paeth(std::uint8_t a, std::uint8_t b, std::uint8_t c) -> std::uint8_t {
    int p = a + b - c;
    int pa = std::abs(p - a);
    int pb = std::abs(p - b);
    int pc = std::abs(p - c);
    std::uint8_t d = (pb <= pc) ? b : c;
    d = (pa <= pb && pa <= pc) ? a : d;
    return d;
}

auto defilter(const std::uint8_t* __restrict buffer, std::uint8_t* __restrict cur,
              const std::uint8_t* __restrict prev, std::size_t bpp, std::size_t bpr)
    -> std::expected<void, Error> {
    std::vector<std::uint8_t> optional_zero_filled_scaline;

    if (prev == nullptr) {
        optional_zero_filled_scaline.resize(bpr, 0);
        prev = optional_zero_filled_scaline.data();
    }

    std::uint8_t filter = *buffer++;
    switch (filter) {
        case 0: {
            std::memcpy(cur, buffer, bpr);
            break;
        }

        case 1: {
            std::memcpy(cur, buffer, bpp);
            for (std::size_t i = bpp; i < bpr; i++) {
                cur[i] = buffer[i] + cur[i - bpp];
            }
            break;
        }

        case 2: {
            std::size_t i = 0;
#ifdef __AVX2__
            if (__builtin_cpu_supports("avx2")) {
                for (; i + 32 <= bpr; i += 32) {
                    __m256i raw = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(buffer + i));
                    __m256i up = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(prev + i));
                    _mm256_storeu_si256(reinterpret_cast<__m256i*>(cur + i),
                                        _mm256_add_epi8(raw, up));
                }
            }
#endif
#ifdef __SSE2__
            if (__builtin_cpu_supports("sse2")) {
                for (; i + 16 <= bpr; i += 16) {
                    __m128i raw = _mm_loadu_si128(reinterpret_cast<const __m128i*>(buffer + i));
                    __m128i up = _mm_loadu_si128(reinterpret_cast<const __m128i*>(prev + i));
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(cur + i), _mm_add_epi8(raw, up));
                }
            }
#endif
            for (; i < bpr; i++) {
                cur[i] = buffer[i] + prev[i];
            }
            break;
        }

        case 3: {
            std::size_t i = 0;
            for (; i < bpp; i++) {
                cur[i] = buffer[i] + (prev[i] / 2);
            }
            for (; i < bpr; i++) {
                cur[i] = buffer[i] + ((prev[i] + cur[i - bpp]) / 2);
            }
            break;
        }

        case 4: {
            std::size_t i = 0;
            for (; i < bpp; i++) {
                cur[i] = buffer[i] + prev[i];
            }
            for (; i < bpr; i++) {
                cur[i] = buffer[i] + paeth(cur[i - bpp], prev[i], prev[i - bpp]);
            }
            break;
        }

        default: return std::unexpected(Error::INVALID_PNG);
    }

    return {};
}

}  // namespace slp
