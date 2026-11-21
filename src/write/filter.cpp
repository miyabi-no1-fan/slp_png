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
#include <array>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "extras.hpp"
#include "filter/filter_internal.hpp"
#include "write.hpp"

namespace slp {

void filter(const Image& image, std::array<std::vector<std::uint8_t>, 5>& filter_buffers,
            std::array<std::uint64_t, 5>& filter_scores, std::size_t i) {
    const std::size_t bpp = image.format().channels() *
                            div_ceil(static_cast<std::size_t>(image.format().bit_depth()), 8);
    const std::size_t bpr = image.bytes_per_row();

    if (i == 0) {
        const std::uint8_t* cur = image.pixels().data() + (i * bpr);
        for (std::size_t j = 0; j < bpp; j++) filter_buffers[1][j + 1] = cur[j];
        for (std::size_t j = bpp; j < bpr; j++) filter_buffers[1][j + 1] = cur[j] - cur[j - bpp];
        for (int j = 0; j < 5; j++) filter_scores[j] = 1;
        filter_scores[1] = 0;
        return;
    }

    const std::uint8_t* cur = image.pixels().data() + (i * bpr);
    const std::uint8_t* prev = image.pixels().data() + ((i - 1) * bpr);

    std::size_t j = 0;
    for (; j < bpp; j++) {
        filter_buffers[0][j + 1] = cur[j];
        filter_buffers[1][j + 1] = cur[j];
        filter_buffers[2][j + 1] = cur[j] - prev[j];
        filter_buffers[3][j + 1] = cur[j] - (prev[j] / 2);
        filter_buffers[4][j + 1] = cur[j] - prev[j];

        filter_scores[0] += std::abs(static_cast<std::int8_t>(filter_buffers[0][j + 1]));
        filter_scores[1] += std::abs(static_cast<std::int8_t>(filter_buffers[1][j + 1]));
        filter_scores[2] += std::abs(static_cast<std::int8_t>(filter_buffers[2][j + 1]));
        filter_scores[3] += std::abs(static_cast<std::int8_t>(filter_buffers[3][j + 1]));
        filter_scores[4] += std::abs(static_cast<std::int8_t>(filter_buffers[4][j + 1]));
    }

    filter_internal_avx2(j, filter_buffers, filter_scores, cur, prev, bpp, bpr);
    filter_internal_sse2(j, filter_buffers, filter_scores, cur, prev, bpp, bpr);
    filter_internal_scalar(j, filter_buffers, filter_scores, cur, prev, bpp, bpr);

    // filter_scores can't overflow
    // width max is UINT32_MAX
    // bpp max is 8
    // bpr max is width * bpp = UINT32_MAX * 8
    // so filter_scores max is bpr * INT8_MAX = UINT32_MAX * 8 * INT8_MAX
    // which is less than UINT64_MAX
}

}  // namespace slp