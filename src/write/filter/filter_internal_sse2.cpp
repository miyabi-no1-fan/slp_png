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
void filter_internal_sse2(std::size_t&, std::array<std::vector<std::uint8_t>, 5>&,  // NOLINT
                          std::array<std::uint64_t, 5>&, const std::uint8_t* __restrict,
                          std::uint8_t* __restrict, std::size_t, std::size_t) {}
#else

#define HELPER __attribute__((target("sse4.2"), always_inline)) static inline auto

// only work when mask = -1 for true and = 0 for false
HELPER sse2_blendv_epi8(__m128i V1, __m128i V2, __m128i M) -> __m128i;
HELPER sse2_abs_epi8(__m128i v) -> __m128i;
HELPER sse2_srli_epi8(__m128i v, int count) -> __m128i;
HELPER sse2_avg(__m128i a, __m128i b) -> __m128i;
HELPER sse2_paeth(__m128i a, __m128i b, __m128i c) -> __m128i;

__attribute__((target("sse4.2"))) void filter_internal_sse2(
    std::size_t& j, std::array<std::vector<std::uint8_t>, 5>& filter_buffers,
    std::array<std::uint64_t, 5>& filter_scores, const std::uint8_t* __restrict cur,
    const std::uint8_t* __restrict prev, std::size_t bpp, std::size_t bpr) {
    if (!__builtin_cpu_supports("sse2")) return;

    __m128i noneSum = _mm_setzero_si128();
    __m128i subSum = _mm_setzero_si128();
    __m128i upSum = _mm_setzero_si128();
    __m128i avgSum = _mm_setzero_si128();
    __m128i paethSum = _mm_setzero_si128();
    __m128i zero = _mm_setzero_si128();

    for (; j + 16 <= bpr; j += 16) {
        const __m128i raw = _mm_loadu_si128(reinterpret_cast<const __m128i*>(cur + j));
        const __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(cur + j - bpp));
        const __m128i b = _mm_loadu_si128(reinterpret_cast<const __m128i*>(prev + j));
        const __m128i c = _mm_loadu_si128(reinterpret_cast<const __m128i*>(prev + j - bpp));

        const __m128i sub = _mm_sub_epi8(raw, a);
        const __m128i up = _mm_sub_epi8(raw, b);
        const __m128i avg = _mm_sub_epi8(raw, sse2_avg(a, b));
        const __m128i paeth = _mm_sub_epi8(raw, sse2_paeth(a, b, c));

        noneSum = _mm_add_epi64(noneSum, _mm_sad_epu8(sse2_abs_epi8(raw), zero));
        subSum = _mm_add_epi64(subSum, _mm_sad_epu8(sse2_abs_epi8(sub), zero));
        upSum = _mm_add_epi64(upSum, _mm_sad_epu8(sse2_abs_epi8(up), zero));
        avgSum = _mm_add_epi64(avgSum, _mm_sad_epu8(sse2_abs_epi8(avg), zero));
        paethSum = _mm_add_epi64(paethSum, _mm_sad_epu8(sse2_abs_epi8(paeth), zero));

        _mm_storeu_si128(reinterpret_cast<__m128i*>(filter_buffers[0].data() + j + 1), raw);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(filter_buffers[1].data() + j + 1), sub);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(filter_buffers[2].data() + j + 1), up);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(filter_buffers[3].data() + j + 1), avg);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(filter_buffers[4].data() + j + 1), paeth);
    }

    alignas(16) std::array<std::uint64_t, 2> tmp0;
    alignas(16) std::array<std::uint64_t, 2> tmp1;
    alignas(16) std::array<std::uint64_t, 2> tmp2;
    alignas(16) std::array<std::uint64_t, 2> tmp3;
    alignas(16) std::array<std::uint64_t, 2> tmp4;

    _mm_store_si128(reinterpret_cast<__m128i*>(tmp0.data()), noneSum);
    _mm_store_si128(reinterpret_cast<__m128i*>(tmp1.data()), subSum);
    _mm_store_si128(reinterpret_cast<__m128i*>(tmp2.data()), upSum);
    _mm_store_si128(reinterpret_cast<__m128i*>(tmp3.data()), avgSum);
    _mm_store_si128(reinterpret_cast<__m128i*>(tmp4.data()), paethSum);

    for (unsigned int u = 0; u < 2; u++) {
        filter_scores[0] += tmp0[u];
        filter_scores[1] += tmp1[u];
        filter_scores[2] += tmp2[u];
        filter_scores[3] += tmp3[u];
        filter_scores[4] += tmp4[u];
    }
}

HELPER sse2_avg(__m128i a, __m128i b) -> __m128i {
    // average of two integers without converting to a wider bit-width
    return _mm_add_epi8(_mm_and_si128(a, b), sse2_srli_epi8(_mm_xor_si128(a, b), 1));
}

HELPER sse2_paeth(__m128i a, __m128i b, __m128i c) -> __m128i {
    // fpnge
    // https://www.lucaversari.it/FJXL_and_FPNGE.pdf
    const __m128i max_bc = _mm_max_epu8(b, c);
    const __m128i min_bc = _mm_min_epu8(b, c);

    const __m128i max_ac = _mm_max_epu8(a, c);
    const __m128i min_ac = _mm_min_epu8(a, c);

    const __m128i pa = _mm_sub_epi8(max_bc, min_bc);
    const __m128i pb = _mm_sub_epi8(max_ac, min_ac);

    const __m128i c_le_a = _mm_cmpeq_epi8(min_ac, c);
    const __m128i b_le_c = _mm_cmpeq_epi8(min_bc, b);
    const __m128i mask = _mm_xor_si128(c_le_a, b_le_c);

    const __m128i max_papb = _mm_max_epu8(pa, pb);
    const __m128i min_papb = _mm_min_epu8(pa, pb);
    const __m128i abs_diff_papb = _mm_sub_epi8(max_papb, min_papb);

    const __m128i pc = _mm_or_si128(abs_diff_papb, mask);

    const __m128i min_papc = _mm_min_epu8(pa, pc);
    const __m128i min_pbpc = _mm_min_epu8(pb, pc);

    const __m128i pa_le_pb = _mm_cmpeq_epi8(min_papb, pa);
    const __m128i pa_le_pc = _mm_cmpeq_epi8(min_papc, pa);

    const __m128i cond1 = _mm_and_si128(pa_le_pb, pa_le_pc);
    const __m128i cond2 = _mm_cmpeq_epi8(min_pbpc, pb);

    const __m128i d = sse2_blendv_epi8(c, b, cond2);
    return sse2_blendv_epi8(d, a, cond1);
}

HELPER sse2_srli_epi8(__m128i v, int count) -> __m128i {
    const __m128i mask = _mm_set1_epi8(static_cast<char>(~((1UL << count) - 1)));
    v = _mm_and_si128(v, mask);
    return _mm_srli_epi64(v, count);
}

HELPER sse2_abs_epi8(__m128i v) -> __m128i {
    if (__builtin_cpu_supports("ssse3")) return _mm_abs_epi8(v);
    __m128i mask = _mm_cmpgt_epi8(_mm_setzero_si128(), v);  // if (v < 0) mask = -1 else mask = 0
    v = _mm_xor_si128(v, mask);                             // if (mask != 0) v = ~v
    return _mm_sub_epi8(v, mask);                           // if (mask != 0) v += 1
}

HELPER sse2_blendv_epi8(__m128i V1, __m128i V2, __m128i M) -> __m128i {
    if (__builtin_cpu_supports("sse4.1")) return _mm_blendv_epi8(V1, V2, M);
    return _mm_or_si128(_mm_andnot_si128(M, V1), _mm_and_si128(M, V2));
}

#endif

}  // namespace slp