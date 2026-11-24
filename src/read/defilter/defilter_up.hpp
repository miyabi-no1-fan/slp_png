#include <cstddef>
#include <cstdint>

namespace slp {

#if defined(__i386__) || defined(__x86_64__)
void defilter_up_simd(std::size_t& i, const std::uint8_t* __restrict buffer,
                      std::uint8_t* __restrict cur, const std::uint8_t* __restrict prev,
                      std::size_t bpr);
#else
static inline void defilter_up_simd(std::size_t&, const std::uint8_t* __restrict,
                                    std::uint8_t* __restrict, const std::uint8_t* __restrict,
                                    std::size_t) {}
#endif

}  // namespace slp