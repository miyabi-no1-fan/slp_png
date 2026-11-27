#if defined(__i386__) || defined(__x86_64__)
#include <immintrin.h>

#include <cstddef>
#include <cstdint>

namespace slp {

__attribute__((target("sse2"))) void defilter_up_sse2(std::size_t& i,
                                                      const std::uint8_t* __restrict buffer,
                                                      std::uint8_t* __restrict cur,
                                                      const std::uint8_t* __restrict prev,
                                                      std::size_t bpr) {
    if (!__builtin_cpu_supports("sse2")) return;
    for (; i + 16 <= bpr; i += 16) {
        __m128i raw = _mm_loadu_si128(reinterpret_cast<const __m128i*>(buffer + i));
        __m128i up = _mm_loadu_si128(reinterpret_cast<const __m128i*>(prev + i));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(cur + i), _mm_add_epi8(raw, up));
    }
}

}  // namespace slp

#endif