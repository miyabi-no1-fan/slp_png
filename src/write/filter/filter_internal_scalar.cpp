#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "filter_internal.hpp"

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

void filter_internal_scalar(std::size_t& j,
                            std::array<std::vector<std::uint8_t>, 5>& filter_buffers,
                            std::array<std::uint64_t, 5>& filter_scores,
                            const std::uint8_t* __restrict cur, const std::uint8_t* __restrict prev,
                            std::size_t bpp, std::size_t bpr) {
    for (; j < bpr; j++) {
        const int a = cur[j - bpp];
        const int b = prev[j];
        const int c = prev[j - bpp];

        filter_buffers[0][j + 1] = cur[j];
        filter_buffers[1][j + 1] = cur[j] - a;
        filter_buffers[2][j + 1] = cur[j] - b;
        filter_buffers[3][j + 1] = cur[j] - ((a + b) / 2);
        filter_buffers[4][j + 1] = cur[j] - paeth(a, b, c);

        filter_scores[0] += std::abs(static_cast<std::int8_t>(filter_buffers[0][j + 1]));
        filter_scores[1] += std::abs(static_cast<std::int8_t>(filter_buffers[1][j + 1]));
        filter_scores[2] += std::abs(static_cast<std::int8_t>(filter_buffers[2][j + 1]));
        filter_scores[3] += std::abs(static_cast<std::int8_t>(filter_buffers[3][j + 1]));
        filter_scores[4] += std::abs(static_cast<std::int8_t>(filter_buffers[4][j + 1]));
    }
}

}  // namespace slp
