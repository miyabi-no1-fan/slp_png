#include "slp_image_transform.h"
#if SLP_IMAGE_TRANSFROM_RELEASE

#if defined(__i386__) || defined(__x86_64__)
    #include <immintrin.h>
#endif

bool slp_image_unpack(slp_image_t* image) {
    const size_t size = (size_t)image->width * image->height * image->channels * (1 + (image->bit_depth == 16));  // dest size

    uint8_t* new_buffer = (uint8_t*)SLP_MALLOC(size);
    if (new_buffer == NULL) {
        if (image->bit_depth == 8) return true;
        return false;
    }

    uint8_t* src = image->pixels;
    uint8_t* dest = new_buffer;

    size_t i = 0;
    switch (image->bit_depth) {
        case 1: {
            #ifdef __SSE2__
            for (; i + 128 <= size; i += 128) {
                const __m128i in = _mm_loadu_si128((const __m128i*)(src + i / 8));

                const __m128i in0 = _mm_and_si128(_mm_srli_epi64(in, 0), _mm_set1_epi8(1));
                const __m128i in1 = _mm_and_si128(_mm_srli_epi64(in, 1), _mm_set1_epi8(1));
                const __m128i in2 = _mm_and_si128(_mm_srli_epi64(in, 2), _mm_set1_epi8(1));
                const __m128i in3 = _mm_and_si128(_mm_srli_epi64(in, 3), _mm_set1_epi8(1));
                const __m128i in4 = _mm_and_si128(_mm_srli_epi64(in, 4), _mm_set1_epi8(1));
                const __m128i in5 = _mm_and_si128(_mm_srli_epi64(in, 5), _mm_set1_epi8(1));
                const __m128i in6 = _mm_and_si128(_mm_srli_epi64(in, 6), _mm_set1_epi8(1));
                const __m128i in7 = _mm_and_si128(_mm_srli_epi64(in, 7), _mm_set1_epi8(1));

                const __m128i a01_lo = _mm_unpacklo_epi8(in0, in1);
                const __m128i a01_hi = _mm_unpackhi_epi8(in0, in1);
                const __m128i a23_lo = _mm_unpacklo_epi8(in2, in3);
                const __m128i a23_hi = _mm_unpackhi_epi8(in2, in3);
                const __m128i a45_lo = _mm_unpacklo_epi8(in4, in5);
                const __m128i a45_hi = _mm_unpackhi_epi8(in4, in5);
                const __m128i a67_lo = _mm_unpacklo_epi8(in6, in7);
                const __m128i a67_hi = _mm_unpackhi_epi8(in6, in7);

                const __m128i a0123lo_lo = _mm_unpacklo_epi16(a01_lo, a23_lo);
                const __m128i a0123lo_hi = _mm_unpackhi_epi16(a01_lo, a23_lo);
                const __m128i a0123hi_lo = _mm_unpacklo_epi16(a01_hi, a23_hi);
                const __m128i a0123hi_hi = _mm_unpackhi_epi16(a01_hi, a23_hi);
                const __m128i a4567lo_lo = _mm_unpacklo_epi16(a45_lo, a67_lo);
                const __m128i a4567lo_hi = _mm_unpackhi_epi16(a45_lo, a67_lo);
                const __m128i a4567hi_lo = _mm_unpacklo_epi16(a45_hi, a67_hi);
                const __m128i a4567hi_hi = _mm_unpackhi_epi16(a45_hi, a67_hi);

                const __m128i a01234567lo_lo_lo = _mm_unpacklo_epi32(a0123lo_lo, a4567lo_lo);
                const __m128i a01234567lo_lo_hi = _mm_unpackhi_epi32(a0123lo_lo, a4567lo_lo);
                const __m128i a01234567lo_hi_lo = _mm_unpacklo_epi32(a0123lo_hi, a4567lo_hi);
                const __m128i a01234567lo_hi_hi = _mm_unpackhi_epi32(a0123lo_hi, a4567lo_hi);
                const __m128i a01234567hi_lo_lo = _mm_unpacklo_epi32(a0123hi_lo, a4567hi_lo);
                const __m128i a01234567hi_lo_hi = _mm_unpackhi_epi32(a0123hi_lo, a4567hi_lo);
                const __m128i a01234567hi_hi_lo = _mm_unpacklo_epi32(a0123hi_hi, a4567hi_hi);
                const __m128i a01234567hi_hi_hi = _mm_unpackhi_epi32(a0123hi_hi, a4567hi_hi);

                _mm_storeu_si128((__m128i*)(dest + i + 0 * 16), a01234567lo_lo_lo);
                _mm_storeu_si128((__m128i*)(dest + i + 1 * 16), a01234567lo_lo_hi);
                _mm_storeu_si128((__m128i*)(dest + i + 2 * 16), a01234567lo_hi_lo);
                _mm_storeu_si128((__m128i*)(dest + i + 3 * 16), a01234567lo_hi_hi);
                _mm_storeu_si128((__m128i*)(dest + i + 4 * 16), a01234567hi_lo_lo);
                _mm_storeu_si128((__m128i*)(dest + i + 5 * 16), a01234567hi_lo_hi);
                _mm_storeu_si128((__m128i*)(dest + i + 6 * 16), a01234567hi_hi_lo);
                _mm_storeu_si128((__m128i*)(dest + i + 7 * 16), a01234567hi_hi_hi);
            }
            #endif
            for (; i + 8 <= size; i += 8) {
                dest[i + 0] = (src[i] >> 7) & 1;
                dest[i + 1] = (src[i] >> 6) & 1;
                dest[i + 2] = (src[i] >> 5) & 1;
                dest[i + 3] = (src[i] >> 4) & 1;
                dest[i + 4] = (src[i] >> 3) & 1;
                dest[i + 5] = (src[i] >> 2) & 1;
                dest[i + 6] = (src[i] >> 1) & 1;
                dest[i + 7] = (src[i] >> 0) & 1;
            }
            break;
        }
        case 2: {
            #ifdef __SSE2__
            for (; i + 64 <= size; i += 64) {
                const __m128i in = _mm_loadu_si128((const __m128i*)(src + i / 4));

                const __m128i in0 = _mm_and_si128(_mm_srli_epi64(in, 0), _mm_set1_epi8(3));  // 0b11
                const __m128i in1 = _mm_and_si128(_mm_srli_epi64(in, 2), _mm_set1_epi8(3));
                const __m128i in2 = _mm_and_si128(_mm_srli_epi64(in, 4), _mm_set1_epi8(3));
                const __m128i in3 = _mm_and_si128(_mm_srli_epi64(in, 6), _mm_set1_epi8(3));

                const __m128i in01_lo = _mm_unpacklo_epi8(in0, in1);
                const __m128i in01_hi = _mm_unpackhi_epi8(in0, in1);
                const __m128i in23_lo = _mm_unpacklo_epi8(in2, in3);
                const __m128i in23_hi = _mm_unpackhi_epi8(in2, in3);

                const __m128i in0123lo_lo = _mm_unpacklo_epi16(in01_lo, in23_lo);
                const __m128i in0123lo_hi = _mm_unpackhi_epi16(in01_lo, in23_lo);
                const __m128i in0123hi_lo = _mm_unpacklo_epi16(in01_hi, in23_hi);
                const __m128i in0123hi_hi = _mm_unpackhi_epi16(in01_hi, in23_hi);

                _mm_storeu_si128((__m128i*)(dest + i + 0 * 16), in0123lo_lo);
                _mm_storeu_si128((__m128i*)(dest + i + 1 * 16), in0123lo_hi);
                _mm_storeu_si128((__m128i*)(dest + i + 2 * 16), in0123hi_lo);
                _mm_storeu_si128((__m128i*)(dest + i + 3 * 16), in0123hi_hi);
            }
            #endif
            for (; i + 4 <= size; i+=4) {
                dest[i + 0] = (src[i] >> 6) & 3;
                dest[i + 1] = (src[i] >> 4) & 3;
                dest[i + 2] = (src[i] >> 2) & 3;
                dest[i + 3] = (src[i] >> 0) & 3;
            }
            break;
        }
        case 4: {
            #ifdef __AVX2__
            for (; i + 64 <= size; i += 64) {
                const __m256i in = _mm256_loadu_si256((const __m256i*)(src + i / 2));

                const __m256i in0 = _mm256_and_si256(_mm256_srli_epi64(in, 0), _mm256_set1_epi8(0x0F));
                const __m256i in1 = _mm256_and_si256(_mm256_srli_epi64(in, 4), _mm256_set1_epi8(0x0F));

                const __m256i a0 = _mm256_unpacklo_epi8(in0, in1);  // 0 x 1 lo
                const __m256i a1 = _mm256_unpackhi_epi8(in0, in1);  // 0 x 1 hi

                _mm_storeu_si128((__m128i*)(dest + i + 0 * 16), _mm256_castsi256_si128(a0));
                _mm_storeu_si128((__m128i*)(dest + i + 1 * 16), _mm256_castsi256_si128(a1));
                _mm_storeu_si128((__m128i*)(dest + i + 2 * 16), _mm256_extracti128_si256(a0, 1));
                _mm_storeu_si128((__m128i*)(dest + i + 3 * 16), _mm256_extracti128_si256(a1, 1));
            }
            #endif
            #ifdef __SSE2__
            for (; i + 32 <= size; i += 32) {
                const __m128i in = _mm_loadu_si128((const __m128i*)(src + i / 2));

                const __m128i in0 = _mm_and_si128(_mm_srli_epi64(in, 0), _mm_set1_epi8(0x0F));
                const __m128i in1 = _mm_and_si128(_mm_srli_epi64(in, 4), _mm_set1_epi8(0x0F));

                const __m128i a0 = _mm_unpacklo_epi8(in0, in1);  // 0 x 1 lo
                const __m128i a1 = _mm_unpackhi_epi8(in0, in1);  // 0 x 1 hi

                _mm_storeu_si128((__m128i*)(dest + i + 0 * 16), a0);
                _mm_storeu_si128((__m128i*)(dest + i + 1 * 16), a1);
            }
            #endif
            for (; i + 2 <= size; i+=2) {
                dest[i + 0] = (src[i] >> 4) & 0x0F;
                dest[i + 1] = (src[i] >> 0) & 0x0F;
            }
            break;
        }
        case 8: {
            SLP_FREE(new_buffer);
            return true;
        }
        case 16: {
            SLP_FREE(new_buffer);
            return true;
        }
        default: {
            SLP_FREE(new_buffer);
            return false;
        }
    }

    slp_image_destroy(image);

    image->pixels = new_buffer;
    image->image_size = size;

    return true;
}



bool slp_image_pack(slp_image_t* image) {
    const size_t size = (size_t)image->height * image->width * image->channels * (1 + (image->bit_depth == 16));
    const size_t new_size = image->image_size;

    uint8_t* new_buffer = (uint8_t*)SLP_MALLOC(new_size);
    if (new_buffer == NULL) return false;

    uint8_t* src = (uint8_t*)(image->pixels);
    uint8_t* dest = (uint8_t*)(new_buffer);

    size_t i = 0;
    switch (image->bit_depth) {
        case 1: {
            #ifdef __AVX2__
            {
                const __m256i mask1 = _mm256_setr_epi8(-1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0);
                const __m256i mask2 = _mm256_setr_epi8(0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0);
                const __m256i mask3 = _mm256_setr_epi8(0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0);
                const __m256i mask4 = _mm256_setr_epi8(0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0);
                const __m256i mask5 = _mm256_setr_epi8(0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0);
                const __m256i mask6 = _mm256_setr_epi8(0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
                const __m256i mask7 = _mm256_setr_epi8(0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0);
                const __m256i mask8 = _mm256_setr_epi8(0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1);
                const __m256i extract = _mm256_setr_epi8(0, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);

                for (; i + 32 <= size; i += 32) {
                    __m256i in = _mm256_loadu_si256((const __m256i*)(src + i));

                    in = _mm256_and_si256(in, _mm256_set1_epi8(1));  // take last 1 bit

                    const __m256i bit1 = _mm256_slli_si256(_mm256_slli_epi64(_mm256_and_si256(in, mask1), 7), 0);
                    const __m256i bit2 = _mm256_slli_si256(_mm256_slli_epi64(_mm256_and_si256(in, mask2), 6), 1);
                    const __m256i bit3 = _mm256_slli_si256(_mm256_slli_epi64(_mm256_and_si256(in, mask3), 5), 2);
                    const __m256i bit4 = _mm256_slli_si256(_mm256_slli_epi64(_mm256_and_si256(in, mask4), 4), 3);
                    const __m256i bit5 = _mm256_slli_si256(_mm256_slli_epi64(_mm256_and_si256(in, mask5), 3), 4);
                    const __m256i bit6 = _mm256_slli_si256(_mm256_slli_epi64(_mm256_and_si256(in, mask6), 2), 5);
                    const __m256i bit7 = _mm256_slli_si256(_mm256_slli_epi64(_mm256_and_si256(in, mask7), 1), 6);
                    const __m256i bit8 = _mm256_slli_si256(_mm256_slli_epi64(_mm256_and_si256(in, mask8), 0), 7);

                    __m256i out = _mm256_or_si256(_mm256_or_si256(_mm256_or_si256(bit1, bit2), _mm256_or_si256(bit3, bit4)), _mm256_or_si256(_mm256_or_si256(bit5, bit6), _mm256_or_si256(bit7, bit8)));
                    out = _mm256_shuffle_epi8(out, extract);

                    *(uint16_t*)(dest + i / 8 + 0) = _mm256_extract_epi16(out, 0);
                    *(uint16_t*)(dest + i / 8 + 2) = _mm256_extract_epi16(out, 8);
                }
            }
            #endif
            #ifdef __SSSE3__
            {
                const __m128i mask1 = _mm_setr_epi8(-1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0);
                const __m128i mask2 = _mm_setr_epi8(0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0);
                const __m128i mask3 = _mm_setr_epi8(0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0);
                const __m128i mask4 = _mm_setr_epi8(0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0);
                const __m128i mask5 = _mm_setr_epi8(0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0);
                const __m128i mask6 = _mm_setr_epi8(0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
                const __m128i mask7 = _mm_setr_epi8(0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0);
                const __m128i mask8 = _mm_setr_epi8(0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1);
                const __m128i extract = _mm_setr_epi8(0, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);

                for (; i + 16 <= size; i += 16) {
                    __m128i in = _mm_loadu_si128((const __m128i*)(src + i));

                    in = _mm_and_si128(in, _mm_set1_epi8(1));  // take last 1 bit

                    const __m128i bit1 = _mm_slli_si128(_mm_slli_epi64(_mm_and_si128(in, mask1), 7), 0);
                    const __m128i bit2 = _mm_slli_si128(_mm_slli_epi64(_mm_and_si128(in, mask2), 6), 1);
                    const __m128i bit3 = _mm_slli_si128(_mm_slli_epi64(_mm_and_si128(in, mask3), 5), 2);
                    const __m128i bit4 = _mm_slli_si128(_mm_slli_epi64(_mm_and_si128(in, mask4), 4), 3);
                    const __m128i bit5 = _mm_slli_si128(_mm_slli_epi64(_mm_and_si128(in, mask5), 3), 4);
                    const __m128i bit6 = _mm_slli_si128(_mm_slli_epi64(_mm_and_si128(in, mask6), 2), 5);
                    const __m128i bit7 = _mm_slli_si128(_mm_slli_epi64(_mm_and_si128(in, mask7), 1), 6);
                    const __m128i bit8 = _mm_slli_si128(_mm_slli_epi64(_mm_and_si128(in, mask8), 0), 7);

                    __m128i out = _mm_or_si128(_mm_or_si128(_mm_or_si128(bit1, bit2), _mm_or_si128(bit3, bit4)), _mm_or_si128(_mm_or_si128(bit5, bit6), _mm_or_si128(bit7, bit8)));
                    out = _mm_shuffle_epi8(out, extract);

                    *(uint16_t*)(dest + i / 8) = _mm_extract_epi16(out, 0);
                }
            }
            #endif
            for (; i + 8 <= size; i += 8) {
                dest[i / 8] = (src[i + 0] & 1) << 7 |
                              (src[i + 1] & 1) << 6 |
                              (src[i + 2] & 1) << 5 |
                              (src[i + 3] & 1) << 4 |
                              (src[i + 4] & 1) << 3 |
                              (src[i + 5] & 1) << 2 |
                              (src[i + 6] & 1) << 1 |
                              (src[i + 7] & 1) << 0;
            }
            break;
        }
        case 2: {
            #ifdef __AVX2__
            {
                const __m256i mask1 = _mm256_setr_epi8(-1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0);
                const __m256i mask2 = _mm256_setr_epi8(0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0);
                const __m256i mask3 = _mm256_setr_epi8(0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0);
                const __m256i mask4 = _mm256_setr_epi8(0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1);
                const __m256i extract = _mm256_setr_epi8(0, 4, 8, 12, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 4, 8, 12, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);

                for (; i + 32 <= size; i += 32) {
                    __m256i in = _mm256_loadu_si256((const __m256i*)(src + i));
                    in = _mm256_and_si256(in, _mm256_set1_epi8(3));  // take last 2 bit

                    const __m256i b1 = _mm256_slli_si256(_mm256_slli_epi32(_mm256_and_si256(in, mask1), 6), 0);
                    const __m256i b2 = _mm256_slli_si256(_mm256_slli_epi32(_mm256_and_si256(in, mask2), 4), 1);
                    const __m256i b3 = _mm256_slli_si256(_mm256_slli_epi32(_mm256_and_si256(in, mask3), 2), 2);
                    const __m256i b4 = _mm256_slli_si256(_mm256_slli_epi32(_mm256_and_si256(in, mask4), 0), 3);

                    __m256i out = _mm256_or_si256(_mm256_or_si256(b1, b2), _mm256_or_si256(b3, b4));
                    out = _mm256_shuffle_epi8(out, extract);

                    *(uint32_t*)(dest + i / 4 + 0) = _mm256_extract_epi32(out, 0);
                    *(uint32_t*)(dest + i / 4 + 4) = _mm256_extract_epi32(out, 4);
                }
            }
            #endif
            #ifdef __SSSE3__
            {
                const __m128i mask1 = _mm_setr_epi8(-1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0);
                const __m128i mask2 = _mm_setr_epi8(0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0);
                const __m128i mask3 = _mm_setr_epi8(0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0);
                const __m128i mask4 = _mm_setr_epi8(0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1);
                const __m128i extract = _mm_setr_epi8(0, 4, 8, 12, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);

                for (; i + 16 <= size; i += 16) {
                    __m128i in = _mm_loadu_si128((const __m128i*)(src + i));
                    in = _mm_and_si128(in, _mm_set1_epi8(3));  // take last 2 bit

                    const __m128i b1 = _mm_slli_si128(_mm_slli_epi32(_mm_and_si128(in, mask1), 6), 0);
                    const __m128i b2 = _mm_slli_si128(_mm_slli_epi32(_mm_and_si128(in, mask2), 4), 1);
                    const __m128i b3 = _mm_slli_si128(_mm_slli_epi32(_mm_and_si128(in, mask3), 2), 2);
                    const __m128i b4 = _mm_slli_si128(_mm_slli_epi32(_mm_and_si128(in, mask4), 0), 3);

                    __m128i out = _mm_or_si128(_mm_or_si128(b1, b2), _mm_or_si128(b3, b4));
                    out = _mm_shuffle_epi8(out, extract);

                    *(uint32_t*)(dest + i / 4) = _mm_cvtsi128_si32(out);
                }
            }
            #endif
            for (; i + 4 <= size; i += 4) {
                dest[i / 4] = (src[i + 0] & 3) << 6 |
                              (src[i + 1] & 3) << 4 |
                              (src[i + 2] & 3) << 2 |
                              (src[i + 3] & 3) << 0;
            }
            break;
        }
        case 4: {
            #ifdef __AVX2__
            {
                const __m256i mask1 = _mm256_setr_epi8(-1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0);
                const __m256i mask2 = _mm256_setr_epi8(0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1);
                const __m256i extract = _mm256_setr_epi8(0, 2, 4, 6, 8, 10, 12, 14, -1, -1, -1, -1, -1, -1, -1, -1, 0, 2, 4, 6, 8, 10, 12, 14, -1, -1, -1, -1, -1, -1, -1, -1);

                for (; i + 32 <= size; i += 32) {
                    __m256i in = _mm256_loadu_si256((const __m256i*)(src + i));
                    in = _mm256_and_si256(in, _mm256_set1_epi8(0x0F));  // take the first 4 bit

                    const __m256i b1 = _mm256_slli_si256(_mm256_slli_epi16(_mm256_and_si256(in, mask1), 4), 0);
                    const __m256i b2 = _mm256_slli_si256(_mm256_slli_epi16(_mm256_and_si256(in, mask2), 0), 1);

                    __m256i out = _mm256_or_si256(b1, b2);
                    out = _mm256_shuffle_epi8(out, extract);

                    #ifdef __i386__
                    *(uint32_t*)(dest + i / 2 + 0) = _mm256_extract_epi32(out, 0);
                    *(uint32_t*)(dest + i / 2 + 4) = _mm256_extract_epi32(out, 1);
                    *(uint32_t*)(dest + i / 2 + 8) = _mm256_extract_epi32(out, 4);
                    *(uint32_t*)(dest + i / 2 + 12) = _mm256_extract_epi32(out, 5);
                    #else
                    *(uint64_t*)(dest + i / 2 + 0) = _mm256_extract_epi64(out, 0);
                    *(uint64_t*)(dest + i / 2 + 8) = _mm256_extract_epi64(out, 2);
                    #endif
                }
            }
            #endif
            #ifdef __SSSE3__
            {
                const __m128i mask1 = _mm_setr_epi8(-1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0);
                const __m128i mask2 = _mm_setr_epi8(0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1);
                const __m128i extract = _mm_setr_epi8(0, 2, 4, 6, 8, 10, 12, 14, -1, -1, -1, -1, -1, -1, -1, -1);

                for (; i + 16 <= size; i += 16) {
                    __m128i in = _mm_loadu_si128((const __m128i*)(src + i));
                    in = _mm_and_si128(in, _mm_set1_epi8(0x0F));  // take the last 4 bit

                    const __m128i b1 = _mm_slli_si128(_mm_slli_epi16(_mm_and_si128(in, mask1), 4), 0);
                    const __m128i b2 = _mm_slli_si128(_mm_slli_epi16(_mm_and_si128(in, mask2), 0), 1);

                    __m128i out = _mm_or_si128(b1, b2);
                    out = _mm_shuffle_epi8(out, extract);

                    #ifdef __i386__
                    *(uint32_t*)(dest + i / 2 + 0) = _mm_cvtsi128_si32(out);
                    *(uint16_t*)(dest + i / 2 + 4) = _mm_extract_epi16(out, 2);
                    *(uint16_t*)(dest + i / 2 + 6) = _mm_extract_epi16(out, 3);
                    #else
                    *(uint64_t*)(dest + i / 2) = _mm_cvtsi128_si64(out);
                    #endif
                }
            }
            #endif
            for (; i+2 <= size; i+=2) {
                dest[i / 2] = (src[i + 0] & 0x0F) << 4 |
                              (src[i + 1] & 0x0F) << 0;
            }
            break;
        }
        case 8: {
            SLP_FREE(new_buffer);
            return true;
        }
        case 16: {
            SLP_FREE(new_buffer);
            return true;
        }
        default: {
            SLP_FREE(new_buffer);
            return false;
        }
    }

    slp_image_destroy(image);

    image->pixels = new_buffer;
    image->image_size = new_size;

    return true;
}

#endif