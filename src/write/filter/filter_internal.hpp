#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace slp {

#define FILTER_INTERNAL(extension)                                                              \
    void filter_internal_##extension(                                                           \
        std::size_t&, std::array<std::vector<std::uint8_t>, 5>&, std::array<std::uint64_t, 5>&, \
        const std::uint8_t* __restrict, const std::uint8_t* __restrict, std::size_t, std::size_t)

#if defined(__i386__) || defined(__x86_64__)
FILTER_INTERNAL(avx2);
FILTER_INTERNAL(sse2);
#else
static inline FILTER_INTERNAL(avx2) {}
static inline FILTER_INTERNAL(sse2) {}
#endif

FILTER_INTERNAL(scalar);

#undef FILTER_INTERNAL

}  // namespace slp