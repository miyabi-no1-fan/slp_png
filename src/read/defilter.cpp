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

#include "defilter/defilter_up.hpp"
#include "read.hpp"
#include "slp_png.hpp"

namespace slp {

static constexpr auto paeth(const int a, const int b, const int c) noexcept -> int {
    const int da = b - c;  // p - a
    const int db = a - c;  // p - b
    const int pa = std::abs(da);
    const int pb = std::abs(db);
    const int pc = std::abs(da + db);
    const int d = (pb <= pc) ? b : c;
    return (pa <= pb && pa <= pc) ? a : d;
}

auto defilter(const std::uint8_t* __restrict buffer, std::uint8_t* __restrict cur,
              const std::uint8_t* __restrict prev, std::size_t bpp, std::size_t bpr)
    -> std::expected<void, Error> {
    std::uint8_t filter = *buffer++;
    switch (filter) {
        case 0: {
            std::memcpy(cur, buffer, bpr);
            break;
        }

        case 1: {
            std::memcpy(cur, buffer, bpp);
            for (std::size_t i = bpp; i < bpr; i++) cur[i] = buffer[i] + cur[i - bpp];
            break;
        }

        case 2: {
            std::size_t i = 0;
            defilter_up_avx2(i, buffer, cur, prev, bpr);
            defilter_up_sse2(i, buffer, cur, prev, bpr);
            for (; i < bpr; i++) cur[i] = buffer[i] + prev[i];
            break;
        }

        case 3: {
            std::size_t i = 0;
            for (; i < bpp; i++) cur[i] = buffer[i] + (prev[i] / 2);
            for (; i < bpr; i++) cur[i] = buffer[i] + ((prev[i] + cur[i - bpp]) / 2);
            break;
        }

        case 4: {
            std::size_t i = 0;
            for (; i < bpp; i++) cur[i] = buffer[i] + prev[i];
            for (; i < bpr; i++) cur[i] = buffer[i] + paeth(cur[i - bpp], prev[i], prev[i - bpp]);
            break;
        }

        default: return std::unexpected(Error::INVALID_PNG);
    }

    return {};
}

}  // namespace slp
