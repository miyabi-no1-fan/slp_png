#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "filter_internal.hpp"

#if defined(__i386__) || defined(__x86_64__)
#include <immintrin.h>
#endif

namespace slp {

#if !defined(__i386__) && !defined(__x86_64__)
void filter_internal_avx2(std::size_t&, std::array<std::vector<std::uint8_t>, 5>&,  // NOLINT
                          std::array<std::uint64_t, 5>&, const std::uint8_t* __restrict,
                          std::uint8_t* __restrict, std::size_t, std::size_t) {}
#else

#define HELPER __attribute__((target("avx2"), always_inline)) static inline auto

HELPER avx2_srli_epi8(__m256i v, int count) -> __m256i;
HELPER avx2_avg(__m256i a, __m256i b) -> __m256i;
HELPER avx2_paeth(__m256i a, __m256i b, __m256i c) -> __m256i;

__attribute__((target("avx2"))) void filter_internal_avx2(
    std::size_t& j, std::array<std::vector<std::uint8_t>, 5>& filter_buffers,
    std::array<std::uint64_t, 5>& filter_scores, const std::uint8_t* __restrict cur,
    const std::uint8_t* __restrict prev, std::size_t bpp, std::size_t bpr) {
    if (!__builtin_cpu_supports("avx2")) return;

    __m256i noneSum = _mm256_setzero_si256();
    __m256i subSum = _mm256_setzero_si256();
    __m256i upSum = _mm256_setzero_si256();
    __m256i avgSum = _mm256_setzero_si256();
    __m256i paethSum = _mm256_setzero_si256();
    __m256i zero = _mm256_setzero_si256();

    for (; j + 32 <= bpr; j += 32) {
        const __m256i raw = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(cur + j));
        const __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(cur + j - bpp));
        const __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(prev + j));
        const __m256i c = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(prev + j - bpp));

        const __m256i sub = _mm256_sub_epi8(raw, a);
        const __m256i up = _mm256_sub_epi8(raw, b);
        const __m256i avg = _mm256_sub_epi8(raw, avx2_avg(a, b));
        const __m256i paeth = _mm256_sub_epi8(raw, avx2_paeth(a, b, c));

        noneSum = _mm256_add_epi64(noneSum, _mm256_sad_epu8(_mm256_abs_epi8(raw), zero));
        subSum = _mm256_add_epi64(subSum, _mm256_sad_epu8(_mm256_abs_epi8(sub), zero));
        upSum = _mm256_add_epi64(upSum, _mm256_sad_epu8(_mm256_abs_epi8(up), zero));
        avgSum = _mm256_add_epi64(avgSum, _mm256_sad_epu8(_mm256_abs_epi8(avg), zero));
        paethSum = _mm256_add_epi64(paethSum, _mm256_sad_epu8(_mm256_abs_epi8(paeth), zero));

        _mm256_storeu_si256(reinterpret_cast<__m256i*>(filter_buffers[0].data() + j + 1), raw);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(filter_buffers[1].data() + j + 1), sub);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(filter_buffers[2].data() + j + 1), up);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(filter_buffers[3].data() + j + 1), avg);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(filter_buffers[4].data() + j + 1), paeth);
    }

    alignas(32) std::array<std::uint64_t, 4> tmp0;
    alignas(32) std::array<std::uint64_t, 4> tmp1;
    alignas(32) std::array<std::uint64_t, 4> tmp2;
    alignas(32) std::array<std::uint64_t, 4> tmp3;
    alignas(32) std::array<std::uint64_t, 4> tmp4;

    _mm256_store_si256(reinterpret_cast<__m256i*>(tmp0.data()), noneSum);
    _mm256_store_si256(reinterpret_cast<__m256i*>(tmp1.data()), subSum);
    _mm256_store_si256(reinterpret_cast<__m256i*>(tmp2.data()), upSum);
    _mm256_store_si256(reinterpret_cast<__m256i*>(tmp3.data()), avgSum);
    _mm256_store_si256(reinterpret_cast<__m256i*>(tmp4.data()), paethSum);

    for (unsigned int u = 0; u < 4; u++) {
        filter_scores[0] += tmp0[u];
        filter_scores[1] += tmp1[u];
        filter_scores[2] += tmp2[u];
        filter_scores[3] += tmp3[u];
        filter_scores[4] += tmp4[u];
    }
}

HELPER avx2_avg(__m256i a, __m256i b) -> __m256i {
    // average of two integers without converting to a wider bit-width
    return _mm256_add_epi8(_mm256_and_si256(a, b), avx2_srli_epi8(_mm256_xor_si256(a, b), 1));
}

HELPER avx2_paeth(__m256i a, __m256i b, __m256i c) -> __m256i {
    // fpnge
    // https://www.lucaversari.it/FJXL_and_FPNGE.pdf
    const __m256i max_bc = _mm256_max_epu8(b, c);
    const __m256i min_bc = _mm256_min_epu8(b, c);

    const __m256i max_ac = _mm256_max_epu8(a, c);
    const __m256i min_ac = _mm256_min_epu8(a, c);

    const __m256i pa = _mm256_sub_epi8(max_bc, min_bc);
    const __m256i pb = _mm256_sub_epi8(max_ac, min_ac);

    const __m256i c_le_a = _mm256_cmpeq_epi8(min_ac, c);
    const __m256i b_le_c = _mm256_cmpeq_epi8(min_bc, b);
    const __m256i mask = _mm256_xor_si256(c_le_a, b_le_c);

    const __m256i max_papb = _mm256_max_epu8(pa, pb);
    const __m256i min_papb = _mm256_min_epu8(pa, pb);
    const __m256i abs_diff_papb = _mm256_sub_epi8(max_papb, min_papb);

    const __m256i pc = _mm256_or_si256(abs_diff_papb, mask);

    const __m256i min_papc = _mm256_min_epu8(pa, pc);
    const __m256i min_pbpc = _mm256_min_epu8(pb, pc);

    const __m256i pa_le_pb = _mm256_cmpeq_epi8(min_papb, pa);
    const __m256i pa_le_pc = _mm256_cmpeq_epi8(min_papc, pa);

    const __m256i cond1 = _mm256_and_si256(pa_le_pb, pa_le_pc);
    const __m256i cond2 = _mm256_cmpeq_epi8(min_pbpc, pb);

    const __m256i d = _mm256_blendv_epi8(c, b, cond2);
    return _mm256_blendv_epi8(d, a, cond1);
}

HELPER avx2_srli_epi8(__m256i v, int count) -> __m256i {
    const __m256i mask = _mm256_set1_epi8(static_cast<char>(~((1UL << count) - 1)));
    v = _mm256_and_si256(v, mask);
    return _mm256_srli_epi64(v, count);
}

#endif

}  // namespace slp