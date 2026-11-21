#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace slp {

#define FILTER_INTERNAL(extension)                                                       \
    void filter_internal_##extension(                                                    \
        std::size_t& j, std::array<std::vector<std::uint8_t>, 5>& filter_buffers,        \
        std::array<std::uint64_t, 5>& filter_scores, const std::uint8_t* __restrict cur, \
        const std::uint8_t* __restrict prev, std::size_t bpp, std::size_t bpr)

FILTER_INTERNAL(avx2);
FILTER_INTERNAL(sse2);
FILTER_INTERNAL(scalar);

#undef FILTER_INTERNAL

}  // namespace slp