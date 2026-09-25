/*
Copyright 2026 miyabi-no1-fan

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#define SLP_PNG_MACROS
#include "slp_image_transform.h"

#if defined(__i386__) || defined(__x86_64__)
    #include <immintrin.h>
#endif
#include <stddef.h>
#include <stdint.h>

int slp_image_convert_to_8bit(slp_image_t* image) {
    uint8_t* src = image->pixels;
    uint8_t* dest = image->pixels;

    const size_t size = image->width * image->height * image->channels;  // dest size

    size_t i = 0;
    switch (image->bit_depth) {
        case 1: {
            #ifdef __AVX2__
            for (; i + 32 <= size; i += 32) {
                __m256i in = _mm256_loadu_si256((const __m256i*)(src + i));
                in = _mm256_sub_epi8(_mm256_setzero_si256(), in);
                _mm256_storeu_si256((__m256i*)(dest + i), in);
            }
            #endif
            #ifdef __SSE2__
            for (; i + 16 <= size; i += 16) {
                __m128i in = _mm_loadu_si128((const __m128i*)(src + i));
                in = _mm_sub_epi8(_mm_setzero_si128(), in);
                _mm_storeu_si128((__m128i*)(dest + i), in);
            }
            #endif
            for (; i < size; i++) dest[i] = -src[i];
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
            for (; i < size; i++) dest[i] = src[i] | src[i] << 4;
            break;
        }
        case 16: {
            #ifdef __AVX2__
            for (; i + 16 <= size; i += 16) {
                __m256i in = _mm256_loadu_si256((const __m256i*)(src + i * 2));
                in = _mm256_srli_epi16(in, 8);
                in = _mm256_packus_epi16(in, _mm256_setzero_si256());
                in = _mm256_permute4x64_epi64(in, _MM_SHUFFLE(0, 0, 2, 0));
                __m128i out = _mm256_castsi256_si128(in);
                _mm_store_si128((__m128i*)(dest + i), out);
            }
            #endif
            #ifdef __SSE2__
            for (; i + 8 <= size; i += 8) {
                __m128i in = _mm_loadu_si128((const __m128i*)(src + i * 2));
                in = _mm_srli_epi16(in, 8);
                in = _mm_packus_epi16(in, _mm_setzero_si128());
                _mm_storel_epi64((__m128i*)(dest + i), in);
            }
            #endif
            for (; i < size; i++) dest[i] = ((uint16_t*)src)[i] >> 8;
            break;
        }
        default: return 1;
    }

    image->bit_depth = 8;
    image->size = size;

    return 0;
}

slp_image_t slp_image_convert_to_16bit(const slp_image_t* image) {
    if (image->bit_depth == 16) return slp_image_copy(*image);

    const size_t size = (size_t)image->height * (size_t)image->width * (size_t)image->channels;  // source size

    slp_image_t new_image = *image;
    new_image.size = size * 2;
    new_image.bit_depth = 16;
    new_image.pixels = (uint8_t*)SLP_MALLOC(size * 2);
    if (new_image.pixels == NULL) return new_image;

    uint8_t* src = image->pixels;
    uint8_t* dest = new_image.pixels;

    size_t i = 0;
    switch (image->bit_depth) {
        case 1: {
            #ifdef __AVX2__
            for (; i + 32 <= size; i += 32) {
                __m256i in = _mm256_loadu_si256((const __m256i*)(src + i));
                in = _mm256_sub_epi8(_mm256_setzero_si256(), in);
                __m256i in_lo = _mm256_unpacklo_epi8(in, in);
                __m256i in_hi = _mm256_unpackhi_epi8(in, in);
                _mm_storeu_si128((__m128i*)(dest + i * 2 + 0 * 16), _mm256_castsi256_si128(in_lo));
                _mm_storeu_si128((__m128i*)(dest + i * 2 + 1 * 16), _mm256_castsi256_si128(in_hi));
                _mm_storeu_si128((__m128i*)(dest + i * 2 + 2 * 16), _mm256_extracti128_si256(in_lo, 1));
                _mm_storeu_si128((__m128i*)(dest + i * 2 + 3 * 16), _mm256_extracti128_si256(in_hi, 1));
            }
            #endif
            #ifdef __SSE2__
            for (; i + 16 <= size; i += 16) {
                __m128i in = _mm_loadu_si128((const __m128i*)(src + i));
                in = _mm_sub_epi8(_mm_setzero_si128(), in);
                __m128i in_lo = _mm_unpacklo_epi8(in, in);
                __m128i in_hi = _mm_unpackhi_epi8(in, in);
                _mm_storeu_si128((__m128i*)(dest + i * 2 + 0 * 16), in_lo);
                _mm_storeu_si128((__m128i*)(dest + i * 2 + 1 * 16), in_hi);
            }
            #endif
            for (; i < size; i++) ((uint16_t*)dest)[i] = -src[i];
            break;
        }
        case 2: {
            #ifdef __AVX2__
            for (; i + 32 <= size; i += 32) {
                const __m256i in = _mm256_loadu_si256((const __m256i*)(src + i));
                __m256i in_lo = _mm256_unpacklo_epi8(in, _mm256_setzero_si256());
                __m256i in_hi = _mm256_unpackhi_epi8(in, _mm256_setzero_si256());
                in_lo = _mm256_mullo_epi16(in_lo, _mm256_set1_epi16(65535 / 3));
                in_hi = _mm256_mullo_epi16(in_hi, _mm256_set1_epi16(65535 / 3));
                _mm_storeu_si128((__m128i*)(dest + i * 2 + 0 * 16), _mm256_castsi256_si128(in_lo));
                _mm_storeu_si128((__m128i*)(dest + i * 2 + 1 * 16), _mm256_castsi256_si128(in_hi));
                _mm_storeu_si128((__m128i*)(dest + i * 2 + 2 * 16), _mm256_extracti128_si256(in_lo, 1));
                _mm_storeu_si128((__m128i*)(dest + i * 2 + 3 * 16), _mm256_extracti128_si256(in_hi, 1));
            }
            #endif
            #ifdef __SSE2__
            for (; i + 16 <= size; i += 16) {
                const __m128i in = _mm_loadu_si128((const __m128i*)(src + i));
                __m128i in_lo = _mm_unpacklo_epi8(in, _mm_setzero_si128());
                __m128i in_hi = _mm_unpackhi_epi8(in, _mm_setzero_si128());
                in_lo = _mm_mullo_epi16(in_lo, _mm_set1_epi16(65535 / 3));
                in_hi = _mm_mullo_epi16(in_hi, _mm_set1_epi16(65535 / 3));
                _mm_storeu_si128((__m128i*)(dest + i * 2 + 0 * 16), in_lo);
                _mm_storeu_si128((__m128i*)(dest + i * 2 + 1 * 16), in_hi);
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
        default: {
            slp_image_destroy(&new_image);
            return new_image;
        }
    }

    return new_image;
}
