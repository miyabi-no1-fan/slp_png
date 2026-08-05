#include "slp_image_transform.h"
#if SLP_IMAGE_TRANSFROM_RELEASE

#if defined(__i386__) || defined(__x86_64__)
    #include <immintrin.h>
#endif

void slp_image_convert_to_8bit(slp_image_t* image) {
    uint8_t* src = image->pixels;
    uint8_t* dest = image->pixels;

    const size_t size = image->width * image->height * image->channels;  // dest size

    size_t i = 0;
    switch (image->bit_depth) {
        case 1: {
            #ifdef __AVX2__
            for (; i + 32 <= size; i += 32) {
                __m256i in = _mm256_loadu_si256((const __m256i*)(src + i));
                in = _mm256_cmpeq_epi8(_mm256_set1_epi8(1), in);
                _mm256_storeu_si256((__m256i*)(dest + i), in);
            }
            #endif
            #ifdef __SSE2__
            for (; i + 16 <= size; i += 16) {
                __m128i in = _mm_loadu_si128((const __m128i*)(src + i));
                in = _mm_cmpeq_epi8(_mm_set1_epi8(1), in);
                _mm_storeu_si128((__m128i*)(dest + i), in);
            }
            #endif
            for (; i < size; i++) dest[i] = src[i] * 0xFF;
            break;
        }
        case 2: {
            #ifdef __AVX2__
            for (; i + 32 <= size; i += 32) {
                __m256i in = _mm256_loadu_si256((const __m256i*)(src + i));
                __m256i in_lo = _mm256_unpacklo_epi8(in, _mm256_setzero_si256());
                __m256i in_hi = _mm256_unpackhi_epi8(in, _mm256_setzero_si256());
                in_lo = _mm256_mullo_epi16(in_lo, _mm256_set1_epi8(85));
                in_hi = _mm256_mullo_epi16(in_hi, _mm256_set1_epi8(85));
                in = _mm256_packus_epi16(in_lo, in_hi);
                _mm256_storeu_si256((__m256i*)(dest + i), in);
            }
            #endif
            #ifdef __SSE2__
            for (; i + 16 <= size; i += 16) {
                __m128i in = _mm_loadu_si128((const __m128i*)(src + i));
                __m128i in_lo = _mm_unpacklo_epi8(in, _mm_setzero_si128());
                __m128i in_hi = _mm_unpackhi_epi8(in, _mm_setzero_si128());
                in_lo = _mm_mullo_epi16(in_lo, _mm_set1_epi8(85));
                in_hi = _mm_mullo_epi16(in_hi, _mm_set1_epi8(85));
                in = _mm_packus_epi16(in_lo, in_hi);
                _mm_storeu_si128((__m128i*)(dest + i), in);
            }
            #endif
            for (; i < size; i++) dest[i] = src[i] * 85;
            break;
        }
        case 4: {
            #ifdef __AVX2__
            for (; i + 32 <= size; i += 32) {
                __m256i in = _mm256_loadu_si256((const __m256i*)(src + i));
                in = _mm256_or_si256(in, _mm256_slli_epi64(in, 4));
                _mm256_storeu_si256((__m256i*)(dest + i), in);
            }
            #endif
            #ifdef __SSE2__
            for (; i + 16 <= size; i += 16) {
                __m128i in = _mm_loadu_si128((const __m128i*)(src + i));
                in = _mm_or_si128(in, _mm_slli_epi64(in, 4));
                _mm_storeu_si128((__m128i*)(dest + i), in);
            }
            #endif
            for (; i < size; i++) dest[i] = src[i] * 17;
            break;
        }
        case 16: {
            #ifdef __AVX2__
            for (; i + 16 <= size; i += 16) {
                __m256i in = _mm256_loadu_si256((const __m256i*)(src + i * 2));
                in = _mm256_srli_epi16(in, 8);
                in = _mm256_packus_epi16(in, _mm256_setzero_si256());
                #ifdef __i386__
                *(uint32_t*)(dest + i + 0 * 4) = _mm_extract_epi32(in, 0);
                *(uint32_t*)(dest + i + 1 * 4) = _mm_extract_epi32(in, 1);
                *(uint32_t*)(dest + i + 2 * 4) = _mm_extract_epi32(in, 4);
                *(uint32_t*)(dest + i + 3 * 4) = _mm_extract_epi32(in, 5);
                #else
                *(uint64_t*)(dest + i + 8 * 0) = _mm256_extract_epi64(in, 0);
                *(uint64_t*)(dest + i + 8 * 1) = _mm256_extract_epi64(in, 2);
                #endif
            }
            #endif
            #ifdef __SSE4_1__
            for (; i + 8 <= size; i += 8) {
                __m128i in = _mm_loadu_si128((const __m128i*)(src + i * 2));
                in = _mm_srli_epi16(in, 8);
                in = _mm_packus_epi16(in, _mm_setzero_si128());

                #ifdef __i386__
                *(uint32_t*)(dest + i + 0 * 4) = _mm_extract_epi32(in, 0);
                *(uint32_t*)(dest + i + 1 * 4) = _mm_extract_epi32(in, 1);
                #else
                *(uint64_t*)(dest + i) = _mm_cvtsi128_si64(in);
                #endif
            }
            #endif
            for (; i < size; i++) dest[i] = src[i * 2];
            break;
        }
        default: return;
    }

    image->bit_depth = 8;
}

// return false = SLP_MALLOC fail or input wrong
bool slp_image_convert_to_16bit(slp_image_t* image) {
    const size_t size = image->height * image->width * image->channels;  // source size

    uint8_t* new_buffer = (uint8_t*)SLP_ALIGNED_ALLOC(size * 2);
    if (new_buffer == NULL) {
        if (image->bit_depth == 16) return true;
        return false;
    }

    uint8_t* src = image->pixels;
    uint8_t* dest = new_buffer;

    size_t i = 0;
    switch (image->bit_depth) {
        case 1: {
            #ifdef __AVX2__
            for (; i + 32 <= size; i += 32) {
                const __m256i in = _mm256_loadu_si256((const __m256i*)(src + i));

                __m256i in_lo = _mm256_unpacklo_epi8(in, in);
                __m256i in_hi = _mm256_unpackhi_epi8(in, in);

                in_lo = _mm256_cmpeq_epi8(_mm256_set1_epi8(1), in_lo);
                in_hi = _mm256_cmpeq_epi8(_mm256_set1_epi8(1), in_hi);

                _mm_storeu_si128((__m128i*)(dest + i * 2 + 0 * 16), _mm256_castsi256_si128(in_lo));
                _mm_storeu_si128((__m128i*)(dest + i * 2 + 1 * 16), _mm256_castsi256_si128(in_hi));
                _mm_storeu_si128((__m128i*)(dest + i * 2 + 2 * 16), _mm256_extracti128_si256(in_lo, 1));
                _mm_storeu_si128((__m128i*)(dest + i * 2 + 3 * 16), _mm256_extracti128_si256(in_hi, 1));
            }
            #endif
            #ifdef __SSE2__
            for (; i + 16 <= size; i += 16) {
                const __m128i in = _mm_loadu_si128((const __m128i*)(src + i));

                __m128i in_lo = _mm_unpacklo_epi8(in, in);
                __m128i in_hi = _mm_unpackhi_epi8(in, in);

                in_lo = _mm_cmpeq_epi8(_mm_set1_epi8(1), in_lo);
                in_hi = _mm_cmpeq_epi8(_mm_set1_epi8(1), in_hi);

                _mm_storeu_si128((__m128i*)(dest + i * 2 + 0 * 16), in_lo);
                _mm_storeu_si128((__m128i*)(dest + i * 2 + 1 * 16), in_hi);
            }
            #endif
            for (; i < size; i++) ((uint16_t*)dest)[i] = src[i] * 0xFFFF;
            break;
        }
        case 2: {
            #ifdef __AVX2__
            {
                const __m256i scalar = _mm256_set1_epi16(65535 / 3);
                for (; i + 32 <= size; i += 32) {
                    const __m256i in = _mm256_loadu_si256((const __m256i*)(src + i));

                    __m256i in_lo = _mm256_unpacklo_epi8(in, _mm256_setzero_si256());
                    __m256i in_hi = _mm256_unpackhi_epi8(in, _mm256_setzero_si256());

                    in_lo = _mm256_mullo_epi16(in_lo, scalar);
                    in_hi = _mm256_mullo_epi16(in_hi, scalar);

                    _mm_storeu_si128((__m128i*)(dest + i * 2 + 0 * 16), _mm256_castsi256_si128(in_lo));
                    _mm_storeu_si128((__m128i*)(dest + i * 2 + 1 * 16), _mm256_castsi256_si128(in_hi));
                    _mm_storeu_si128((__m128i*)(dest + i * 2 + 2 * 16), _mm256_extracti128_si256(in_lo, 1));
                    _mm_storeu_si128((__m128i*)(dest + i * 2 + 3 * 16), _mm256_extracti128_si256(in_hi, 1));
                }
            }
            #endif
            #ifdef __SSE2__
            {
                const __m128i scalar = _mm_set1_epi16(65535 / 3);
                for (; i + 16 <= size; i += 16) {
                    const __m128i in = _mm_loadu_si128((const __m128i*)(src + i));

                    __m128i in_lo = _mm_unpacklo_epi8(in, _mm_setzero_si128());
                    __m128i in_hi = _mm_unpackhi_epi8(in, _mm_setzero_si128());

                    in_lo = _mm_mullo_epi16(in_lo, scalar);
                    in_hi = _mm_mullo_epi16(in_hi, scalar);

                    _mm_storeu_si128((__m128i*)(dest + i * 2 + 0 * 16), in_lo);
                    _mm_storeu_si128((__m128i*)(dest + i * 2 + 1 * 16), in_hi);
                }
            }
            #endif
            for (; i < size; i++) ((uint16_t*)dest)[i] = src[i] * 21845;
            break;
        }
        case 4: {
            #ifdef __AVX2__
            for (; i + 32 <= size; i += 32) {
                __m256i in = _mm256_loadu_si256((const __m256i*)(src + i));
                in = _mm256_or_si256(in, _mm256_slli_epi64(in, 4));

                const __m256i in_lo = _mm256_unpacklo_epi8(in, in);
                const __m256i in_hi = _mm256_unpackhi_epi8(in, in);

                _mm_storeu_si128((__m128i*)(dest + i * 2 + 0 * 16), _mm256_castsi256_si128(in_lo));
                _mm_storeu_si128((__m128i*)(dest + i * 2 + 1 * 16), _mm256_castsi256_si128(in_hi));
                _mm_storeu_si128((__m128i*)(dest + i * 2 + 2 * 16), _mm256_extracti128_si256(in_lo, 1));
                _mm_storeu_si128((__m128i*)(dest + i * 2 + 3 * 16), _mm256_extracti128_si256(in_hi, 1));
            }
            #endif
            #ifdef __SSE2__
            for (; i + 16 <= size; i += 16) {
                __m128i in = _mm_loadu_si128((const __m128i*)(src + i));
                in = _mm_or_si128(in, _mm_slli_epi64(in, 4));

                const __m128i in_lo = _mm_unpacklo_epi8(in, in);
                const __m128i in_hi = _mm_unpackhi_epi8(in, in);

                _mm_storeu_si128((__m128i*)(dest + i * 2 + 0 * 16), in_lo);
                _mm_storeu_si128((__m128i*)(dest + i * 2 + 1 * 16), in_hi);
            }
            #endif
            for (; i < size; i++) ((uint16_t*)dest)[i] = src[i] * 4369;
            break;
        }
        case 8: {
            #ifdef __AVX2__
            for (; i + 32 <= size; i += 32) {
                const __m256i in = _mm256_loadu_si256((const __m256i*)(src + i));
                const __m256i in_lo = _mm256_unpacklo_epi8(in, in);
                const __m256i in_hi = _mm256_unpackhi_epi8(in, in);
                _mm_storeu_si128((__m128i*)(dest + i + 0 * 8), _mm256_castsi256_si128(in_lo));
                _mm_storeu_si128((__m128i*)(dest + i + 1 * 8), _mm256_castsi256_si128(in_hi));
                _mm_storeu_si128((__m128i*)(dest + i + 2 * 8), _mm256_extracti128_si256(in_lo, 1));
                _mm_storeu_si128((__m128i*)(dest + i + 3 * 8), _mm256_extracti128_si256(in_hi, 1));
            }
            #endif
            #ifdef __SSE2__
            for (; i + 16 <= size; i += 16) {
                const __m128i in = _mm_loadu_si128((const __m128i*)(src + i));
                const __m128i in_lo = _mm_unpacklo_epi8(in, in);
                const __m128i in_hi = _mm_unpackhi_epi8(in, in);
                _mm_storeu_si128((__m128i*)(dest + i + 0 * 8), in_lo);
                _mm_storeu_si128((__m128i*)(dest + i + 1 * 8), in_hi);
            }
            #endif
            for (; i < size; i++) dest[i] = src[i] * 257;
            break;
        }
        case 16: {
            SLP_ALIGNED_FREE(new_buffer);
            return true;
        }
        default: {
            SLP_ALIGNED_FREE(new_buffer);
            return false;
        }
    }

    slp_image_destroy(image);

    image->pixels = new_buffer;
    image->bit_depth = 16;
    image->image_size = size * 2;
    image->allocated_size = SLP_ALIGN_SIZE(size * 2);

    return true;
}

#endif