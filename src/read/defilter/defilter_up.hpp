#include <cstddef>
#include <cstdint>

namespace slp {

#define DEFILTER_UP(ext)                                                                           \
    void defilter_up_##ext(std::size_t&, const std::uint8_t* __restrict, std::uint8_t* __restrict, \
                           const std::uint8_t* __restrict, std::size_t)

#if defined(__i386__) || defined(__x86_64__)
DEFILTER_UP(avx2);
DEFILTER_UP(sse2);
#else
static inline DEFILTER_UP(avx2) {}
static inline DEFILTER_UP(sse2) {}
#endif

#undef DEFILTER_UP

}  // namespace slp