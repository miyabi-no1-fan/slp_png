#include <assert.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__i386__) || defined(__x86_64__)
    #include <immintrin.h>
#endif

#if defined(__i386__) || defined(__x86_64__)
static inline __m128i _mm_abs_epi16_compat(__m128i v);
static inline __m128i _mm_abs_epi8_compat(__m128i v);
static inline __m128i _mm_blendv_epi8_compat(__m128i V1, __m128i V2, __m128i M);  // assume mask is -1 for true and 0 for false

static inline __m128i _mm_cmple_epu8(const __m128i a, const __m128i b);
static inline __m256i _mm256_cmple_epu8(const __m256i a, const __m256i b);

static inline __m128i _mm_srli_epi8(__m128i v, const int count);
static inline __m256i _mm256_srli_epi8(__m256i v, const int count);

static inline __m128i _mm_avg(const __m128i a, const __m128i b);
static inline __m256i _mm256_avg(const __m256i a, const __m256i b);

static inline __m128i _mm_paeth(const __m128i a, const __m128i b, const __m128i c);
static inline __m256i _mm256_paeth(const __m256i a, const __m256i b, const __m256i c);
#endif

void slp_png_filter(uint8_t* restrict image_buffer, int8_t* restrict* restrict filter_buffers, uint64_t* restrict filter_scores, const size_t i, const size_t bpr, const size_t bpp) {
    if (i == 0) {
        uint8_t* src = image_buffer;
        for (size_t j = 0; j < bpp; j++) filter_buffers[1][j + 1] = src[j];
        for (size_t j = bpp; j < bpr; j++) filter_buffers[1][j + 1] = src[j] - src[j - bpp];
        for (int j = 0; j < 5; j++) filter_scores[j] = 1;
        filter_scores[1] = 0;
    }
    else {
        uint8_t* src = image_buffer + i * bpr;

        size_t j = 0;
        for (; j < bpp; j++) {
            filter_buffers[0][j + 1] = src[j];
            filter_buffers[1][j + 1] = src[j];
            filter_buffers[2][j + 1] = src[j] - src[j - bpr];
            filter_buffers[3][j + 1] = src[j] - (src[j - bpr] >> 1);
            filter_buffers[4][j + 1] = src[j] - src[j - bpr];

            filter_scores[0] += abs(filter_buffers[0][j + 1]);
            filter_scores[1] += abs(filter_buffers[1][j + 1]);
            filter_scores[2] += abs(filter_buffers[2][j + 1]);
            filter_scores[3] += abs(filter_buffers[3][j + 1]);
            filter_scores[4] += abs(filter_buffers[4][j + 1]);
        }

        #ifdef __AVX2__
        {
            __m256i noneSum = _mm256_setzero_si256();
            __m256i subSum = _mm256_setzero_si256();
            __m256i upSum = _mm256_setzero_si256();
            __m256i avgSum = _mm256_setzero_si256();
            __m256i paethSum = _mm256_setzero_si256();
            __m256i zero = _mm256_setzero_si256();

            for (; j + 32 <= bpr; j += 32) {
                const __m256i raw = _mm256_loadu_si256((const __m256i*)(src + j));
                const __m256i a = _mm256_loadu_si256((const __m256i*)(src + j - bpp));
                const __m256i b = _mm256_loadu_si256((const __m256i*)(src + j - bpr));
                const __m256i c = _mm256_loadu_si256((const __m256i*)(src + j - bpr - bpp));

                const __m256i sub = _mm256_sub_epi8(raw, a);
                const __m256i up = _mm256_sub_epi8(raw, b);
                const __m256i avg = _mm256_sub_epi8(raw, _mm256_avg(a, b));
                const __m256i paeth = _mm256_sub_epi8(raw, _mm256_paeth(a, b, c));

                noneSum = _mm256_add_epi64(noneSum, _mm256_sad_epu8(_mm256_abs_epi8(raw), zero));
                subSum = _mm256_add_epi64(subSum, _mm256_sad_epu8(_mm256_abs_epi8(sub), zero));
                upSum = _mm256_add_epi64(upSum, _mm256_sad_epu8(_mm256_abs_epi8(up), zero));
                avgSum = _mm256_add_epi64(avgSum, _mm256_sad_epu8(_mm256_abs_epi8(avg), zero));
                paethSum = _mm256_add_epi64(paethSum, _mm256_sad_epu8(_mm256_abs_epi8(paeth), zero));

                _mm256_storeu_si256((__m256i*)(filter_buffers[0] + j + 1), raw);
                _mm256_storeu_si256((__m256i*)(filter_buffers[1] + j + 1), sub);
                _mm256_storeu_si256((__m256i*)(filter_buffers[2] + j + 1), up);
                _mm256_storeu_si256((__m256i*)(filter_buffers[3] + j + 1), avg);
                _mm256_storeu_si256((__m256i*)(filter_buffers[4] + j + 1), paeth);
            }

            alignas(32) uint64_t tmp0[4];
            alignas(32) uint64_t tmp1[4];
            alignas(32) uint64_t tmp2[4];
            alignas(32) uint64_t tmp3[4];
            alignas(32) uint64_t tmp4[4];

            _mm256_store_si256((__m256i*)tmp0, noneSum);
            _mm256_store_si256((__m256i*)tmp1, subSum);
            _mm256_store_si256((__m256i*)tmp2, upSum);
            _mm256_store_si256((__m256i*)tmp3, avgSum);
            _mm256_store_si256((__m256i*)tmp4, paethSum);

            for (unsigned int u = 0; u < 4; u++) {
                filter_scores[0] += tmp0[u];
                filter_scores[1] += tmp1[u];
                filter_scores[2] += tmp2[u];
                filter_scores[3] += tmp3[u];
                filter_scores[4] += tmp4[u];
            }
        }
        #endif
        #ifdef __SSE2__
        {
            __m128i noneSum = _mm_setzero_si128();
            __m128i subSum = _mm_setzero_si128();
            __m128i upSum = _mm_setzero_si128();
            __m128i avgSum = _mm_setzero_si128();
            __m128i paethSum = _mm_setzero_si128();
            __m128i zero = _mm_setzero_si128();

            for (; j + 16 <= bpr; j += 16) {
                const __m128i raw = _mm_loadu_si128((const __m128i*)(src + j));
                const __m128i a = _mm_loadu_si128((const __m128i*)(src + j - bpp));
                const __m128i b = _mm_loadu_si128((const __m128i*)(src + j - bpr));
                const __m128i c = _mm_loadu_si128((const __m128i*)(src + j - bpr - bpp));

                const __m128i sub = _mm_sub_epi8(raw, a);
                const __m128i up = _mm_sub_epi8(raw, b);
                const __m128i avg = _mm_sub_epi8(raw, _mm_avg(a, b));
                const __m128i paeth = _mm_sub_epi8(raw, _mm_paeth(a, b, c));

                noneSum = _mm_add_epi64(noneSum, _mm_sad_epu8(_mm_abs_epi8_compat(raw), zero));
                subSum = _mm_add_epi64(subSum, _mm_sad_epu8(_mm_abs_epi8_compat(sub), zero));
                upSum = _mm_add_epi64(upSum, _mm_sad_epu8(_mm_abs_epi8_compat(up), zero));
                avgSum = _mm_add_epi64(avgSum, _mm_sad_epu8(_mm_abs_epi8_compat(avg), zero));
                paethSum = _mm_add_epi64(paethSum, _mm_sad_epu8(_mm_abs_epi8_compat(paeth), zero));

                _mm_storeu_si128((__m128i*)(filter_buffers[0] + j + 1), raw);
                _mm_storeu_si128((__m128i*)(filter_buffers[1] + j + 1), sub);
                _mm_storeu_si128((__m128i*)(filter_buffers[2] + j + 1), up);
                _mm_storeu_si128((__m128i*)(filter_buffers[3] + j + 1), avg);
                _mm_storeu_si128((__m128i*)(filter_buffers[4] + j + 1), paeth);
            }

            alignas(16) uint64_t tmp0[2];
            alignas(16) uint64_t tmp1[2];
            alignas(16) uint64_t tmp2[2];
            alignas(16) uint64_t tmp3[2];
            alignas(16) uint64_t tmp4[2];

            _mm_store_si128((__m128i*)tmp0, noneSum);
            _mm_store_si128((__m128i*)tmp1, subSum);
            _mm_store_si128((__m128i*)tmp2, upSum);
            _mm_store_si128((__m128i*)tmp3, avgSum);
            _mm_store_si128((__m128i*)tmp4, paethSum);

            for (unsigned int u = 0; u < 2; u++) {
                filter_scores[0] += tmp0[u];
                filter_scores[1] += tmp1[u];
                filter_scores[2] += tmp2[u];
                filter_scores[3] += tmp3[u];
                filter_scores[4] += tmp4[u];
            }
        }
        #endif

        for (; j < bpr; j++) {
            const int p = src[j - bpp] + src[j - bpr] - src[j - bpr - bpp];
            const int pa = abs(p - src[j - bpp]);
            const int pb = abs(p - src[j - bpr]);
            const int pc = abs(p - src[j - bpr - bpp]);

            uint8_t d = (pb <= pc) ? src[j - bpr] : src[j - bpr - bpp];
            d = (pa <= pb && pa <= pc) ? src[j - bpp] : d;

            filter_buffers[0][j + 1] = src[j];
            filter_buffers[1][j + 1] = src[j] - src[j - bpp];
            filter_buffers[2][j + 1] = src[j] - src[j - bpr];
            filter_buffers[3][j + 1] = src[j] - ((src[j - bpp] + src[j - bpr]) / 2);
            filter_buffers[4][j + 1] = src[j] - d;

            filter_scores[0] += abs(filter_buffers[0][j + 1]);
            filter_scores[1] += abs(filter_buffers[1][j + 1]);
            filter_scores[2] += abs(filter_buffers[2][j + 1]);
            filter_scores[3] += abs(filter_buffers[3][j + 1]);
            filter_scores[4] += abs(filter_buffers[4][j + 1]);

            /*Overflow Risk:
            In order to overflow
            bpr must be at least UINT64_MAX / UINT8_MAX
            the biggest image format support is RGBA64, which has 4 channels and bit_depth of 16
            bpr = width * 4 * 2
            So, width must be at least UINT64_MAX / (UINT8_MAX * 4 * 2)
            But width is uint32_t. So:*/
            assert(UINT32_MAX < UINT64_MAX / (UINT8_MAX * 4 * 2));
        }
    }
}

#ifdef __SSE2__
static inline __m256i _mm256_avg(const __m256i a, const __m256i b) {
    // average of two integers without converting to a wider bit-width
    return _mm256_add_epi8(_mm256_and_si256(a, b), _mm256_srli_epi8(_mm256_xor_si256(a, b), 1));
}

static inline __m128i _mm_avg(const __m128i a, const __m128i b) {
    // average of two integers without converting to a wider bit-width
    return _mm_add_epi8(_mm_and_si128(a, b), _mm_srli_epi8(_mm_xor_si128(a, b), 1));
}

static inline __m256i _mm256_paeth(const __m256i a, const __m256i b, const __m256i c) {
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

    const __m256i a_lt_c = _mm256_xor_si256(c_le_a, _mm256_set1_epi8(-1));  // !(c <= a)
    const __m256i c_lt_b = _mm256_xor_si256(b_le_c, _mm256_set1_epi8(-1));  // !(b <= c)

    const __m256i pc = _mm256_blendv_epi8(
        _mm256_set1_epi8(-1),
        _mm256_sub_epi8(_mm256_max_epu8(pa, pb), _mm256_min_epu8(pa, pb)),  //
        _mm256_cmpeq_epi8(a_lt_c, c_lt_b)                                   //
    );

    const __m256i pa_le_pb = _mm256_cmple_epu8(pa, pb);
    const __m256i pa_le_pc = _mm256_cmple_epu8(pa, pc);

    const __m256i cond1 = _mm256_and_si256(pa_le_pb, pa_le_pc);
    const __m256i cond2 = _mm256_cmple_epu8(pb, pc);

    __m256i d = _mm256_blendv_epi8(c, b, cond2);
    return _mm256_blendv_epi8(d, a, cond1);
}

static inline __m128i _mm_paeth(const __m128i a, const __m128i b, const __m128i c) {
    // fpnge
    // https://www.lucaversari.it/FJXL_and_FPNGE.pdf
    const __m128i pa = _mm_sub_epi8(_mm_max_epu8(b, c), _mm_min_epu8(b, c));
    const __m128i pb = _mm_sub_epi8(_mm_max_epu8(a, c), _mm_min_epu8(a, c));

    const __m128i a_lt_c = _mm_xor_si128(_mm_cmple_epu8(c, a), _mm_set1_epi8(-1));  // !(c <= a)
    const __m128i c_lt_b = _mm_xor_si128(_mm_cmple_epu8(b, c), _mm_set1_epi8(-1));  // !(b <= c)

    const __m128i pc = _mm_blendv_epi8_compat(
        _mm_set1_epi8(-1),
        _mm_sub_epi8(_mm_max_epu8(pa, pb), _mm_min_epu8(pa, pb)),  //
        _mm_cmpeq_epi8(a_lt_c, c_lt_b)                             //
    );

    const __m128i pa_le_pb = _mm_cmple_epu8(pa, pb);
    const __m128i pa_le_pc = _mm_cmple_epu8(pa, pc);

    const __m128i cond1 = _mm_and_si128(pa_le_pb, pa_le_pc);
    const __m128i cond2 = _mm_cmple_epu8(pb, pc);

    __m128i d = _mm_blendv_epi8_compat(c, b, cond2);
    return _mm_blendv_epi8(d, a, cond1);
}

static inline __m128i _mm_abs_epi16_compat(__m128i v) {
    #ifdef __SSSE3__
    return _mm_abs_epi16(v);
    #else
    __m128i mask = _mm_srai_epi16(v, 15);  // srai will shift in 1 or 0 depends on the msbit
    v = _mm_xor_si128(v, mask);            // do bit flips if signed
    return _mm_sub_epi16(v, mask);         // this is add 1 if signed, for signed number all1 = -1 so --1 = +1
    #endif
}

static inline __m128i _mm_abs_epi8_compat(__m128i v) {
    #ifdef __SSSE3__
    return _mm_abs_epi8(v);
    #else
    __m128i mask = _mm_cmpgt_epi8(_mm_setzero_si128(), v);
    v = _mm_xor_si128(v, mask);
    return _mm_sub_epi8(v, mask);
    #endif
}

static inline __m128i _mm_blendv_epi8_compat(__m128i V1, __m128i V2, __m128i M) {
    #ifdef __SSE4_1__
    return _mm_blendv_epi8(V1, V2, M);
    #else
    return _mm_or_si128(_mm_andnot_si128(M, V1), _mm_and_si128(M, V2));
    #endif
}

static inline __m128i _mm_srli_epi8(__m128i v, const int count) {
    const __m128i mask = _mm_xor_si128(_mm_set1_epi8((1ul << count) - 1), _mm_set1_epi8(-1));
    v = _mm_and_si128(v, mask);
    return _mm_srli_epi64(v, count);
}

static inline __m256i _mm256_srli_epi8(__m256i v, const int count) {
    const __m256i mask = _mm256_xor_si256(_mm256_set1_epi8((1ul << count) - 1), _mm256_set1_epi8(-1));
    v = _mm256_and_si256(v, mask);
    return _mm256_srli_epi64(v, count);
}

static inline __m128i _mm_cmple_epu8(const __m128i a, const __m128i b) {
    return _mm_cmpeq_epi8(_mm_min_epu8(a, b), a);
}

static inline __m256i _mm256_cmple_epu8(const __m256i a, const __m256i b) {
    return _mm256_cmpeq_epi8(_mm256_min_epu8(a, b), a);
}
#endif