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
#if defined(__i386__) || defined(__x86_64__)
    #include <immintrin.h>
#endif
#include <stdlib.h>

#include "slp_png.h"

void colortype3_unpack(slp_image_t* restrict image, uint8_t* restrict buffer, const size_t bpr, const size_t imtrker) {
    if (image->channels != 4)
        abort();  // invalid parameter = abort

    uint8_t* src = buffer;
    uint8_t* dest = image->pixels + imtrker * image->width * image->channels;

    #ifdef __SSE2__
    // convert each u8 into u32 and store
    #define convert_store_u8_to_u32(indx, src) do {                                                           \
        const __m128i zeroes = _mm_setzero_si128();                                                 \
        const __m128i x0 = _mm_unpacklo_epi8(src, zeroes);                                          \
        const __m128i x1 = _mm_unpackhi_epi8(src, zeroes);                                          \
        const __m128i p0 = _mm_unpacklo_epi16(x0, zeroes);                                          \
        const __m128i p1 = _mm_unpackhi_epi16(x0, zeroes);                                          \
        const __m128i p2 = _mm_unpacklo_epi16(x1, zeroes);                                          \
        const __m128i p3 = _mm_unpackhi_epi16(x1, zeroes);                                          \
        _mm_storeu_si128((__m128i*)(dest + i * (32 / image->bit_depth) + (indx * 4 + 0) * 16), p0); \
        _mm_storeu_si128((__m128i*)(dest + i * (32 / image->bit_depth) + (indx * 4 + 1) * 16), p1); \
        _mm_storeu_si128((__m128i*)(dest + i * (32 / image->bit_depth) + (indx * 4 + 2) * 16), p2); \
        _mm_storeu_si128((__m128i*)(dest + i * (32 / image->bit_depth) + (indx * 4 + 3) * 16), p3); \
    } while (0)
    #endif

    size_t i = 0;
    switch (image->bit_depth) {
        case 1: {
            #ifdef __SSE2__
            for (; i + 16 <= bpr; i += 16) {
                const __m128i in = _mm_loadu_si128((const __m128i*)(src + i));

                const __m128i in0 = _mm_and_si128(_mm_srli_epi64(in, 7), _mm_set1_epi8(1));
                const __m128i in1 = _mm_and_si128(_mm_srli_epi64(in, 6), _mm_set1_epi8(1));
                const __m128i in2 = _mm_and_si128(_mm_srli_epi64(in, 5), _mm_set1_epi8(1));
                const __m128i in3 = _mm_and_si128(_mm_srli_epi64(in, 4), _mm_set1_epi8(1));
                const __m128i in4 = _mm_and_si128(_mm_srli_epi64(in, 3), _mm_set1_epi8(1));
                const __m128i in5 = _mm_and_si128(_mm_srli_epi64(in, 2), _mm_set1_epi8(1));
                const __m128i in6 = _mm_and_si128(_mm_srli_epi64(in, 1), _mm_set1_epi8(1));
                const __m128i in7 = _mm_and_si128(_mm_srli_epi64(in, 0), _mm_set1_epi8(1));

                const __m128i a01_lo = _mm_unpacklo_epi8(in0, in1);
                const __m128i a01_hi = _mm_unpackhi_epi8(in0, in1);
                const __m128i a23_lo = _mm_unpacklo_epi8(in2, in3);
                const __m128i a23_hi = _mm_unpackhi_epi8(in2, in3);
                const __m128i a45_lo = _mm_unpacklo_epi8(in4, in5);
                const __m128i a45_hi = _mm_unpackhi_epi8(in4, in5);
                const __m128i a67_lo = _mm_unpacklo_epi8(in6, in7);
                const __m128i a67_hi = _mm_unpackhi_epi8(in6, in7);

                const __m128i a0123_part1 = _mm_unpacklo_epi16(a01_lo, a23_lo);
                const __m128i a0123_part2 = _mm_unpackhi_epi16(a01_lo, a23_lo);
                const __m128i a0123_part3 = _mm_unpacklo_epi16(a01_hi, a23_hi);
                const __m128i a0123_part4 = _mm_unpackhi_epi16(a01_hi, a23_hi);
                const __m128i a4567_part1 = _mm_unpacklo_epi16(a45_lo, a67_lo);
                const __m128i a4567_part2 = _mm_unpackhi_epi16(a45_lo, a67_lo);
                const __m128i a4567_part3 = _mm_unpacklo_epi16(a45_hi, a67_hi);
                const __m128i a4567_part4 = _mm_unpackhi_epi16(a45_hi, a67_hi);

                const __m128i out1 = _mm_unpacklo_epi32(a0123_part1, a4567_part1);
                const __m128i out2 = _mm_unpackhi_epi32(a0123_part1, a4567_part1);
                const __m128i out3 = _mm_unpacklo_epi32(a0123_part2, a4567_part2);
                const __m128i out4 = _mm_unpackhi_epi32(a0123_part2, a4567_part2);
                const __m128i out5 = _mm_unpacklo_epi32(a0123_part3, a4567_part3);
                const __m128i out6 = _mm_unpackhi_epi32(a0123_part3, a4567_part3);
                const __m128i out7 = _mm_unpacklo_epi32(a0123_part4, a4567_part4);
                const __m128i out8 = _mm_unpackhi_epi32(a0123_part4, a4567_part4);

                convert_store_u8_to_u32(0, out1);
                convert_store_u8_to_u32(1, out2);
                convert_store_u8_to_u32(2, out3);
                convert_store_u8_to_u32(3, out4);
                convert_store_u8_to_u32(4, out5);
                convert_store_u8_to_u32(5, out6);
                convert_store_u8_to_u32(6, out7);
                convert_store_u8_to_u32(7, out8);
            }
            #endif
            for (; i < bpr; i++) {
                for (size_t j = 0; j < 8 && i * 32 + j * 4 < image->width * image->channels; j++) {
                    dest[i * 32 + j * 4] = (src[i] >> (7 - j)) & 1;
                }
                /*dest[i * 32 + 0 * 4] = (src[i] >> 7) & 1;
                dest[i * 32 + 1 * 4] = (src[i] >> 6) & 1;
                dest[i * 32 + 2 * 4] = (src[i] >> 5) & 1;
                dest[i * 32 + 3 * 4] = (src[i] >> 4) & 1;
                dest[i * 32 + 4 * 4] = (src[i] >> 3) & 1;
                dest[i * 32 + 5 * 4] = (src[i] >> 2) & 1;
                dest[i * 32 + 6 * 4] = (src[i] >> 1) & 1;
                dest[i * 32 + 7 * 4] = (src[i] >> 0) & 1;*/
            }
            break;
        }
        case 2: {
            #ifdef __SSE2__
            for (; i + 16 <= bpr; i += 16) {
                const __m128i in = _mm_loadu_si128((const __m128i*)(src + i));

                const __m128i in0 = _mm_and_si128(_mm_srli_epi64(in, 6), _mm_set1_epi8(3));
                const __m128i in1 = _mm_and_si128(_mm_srli_epi64(in, 4), _mm_set1_epi8(3));
                const __m128i in2 = _mm_and_si128(_mm_srli_epi64(in, 2), _mm_set1_epi8(3));
                const __m128i in3 = _mm_and_si128(_mm_srli_epi64(in, 0), _mm_set1_epi8(3));

                const __m128i in01_lo = _mm_unpacklo_epi8(in0, in1);
                const __m128i in01_hi = _mm_unpackhi_epi8(in0, in1);
                const __m128i in23_lo = _mm_unpacklo_epi8(in2, in3);
                const __m128i in23_hi = _mm_unpackhi_epi8(in2, in3);

                const __m128i in0123lo_lo = _mm_unpacklo_epi16(in01_lo, in23_lo);
                const __m128i in0123lo_hi = _mm_unpackhi_epi16(in01_lo, in23_lo);
                const __m128i in0123hi_lo = _mm_unpacklo_epi16(in01_hi, in23_hi);
                const __m128i in0123hi_hi = _mm_unpackhi_epi16(in01_hi, in23_hi);

                convert_store_u8_to_u32(0, in0123lo_lo);
                convert_store_u8_to_u32(1, in0123lo_hi);
                convert_store_u8_to_u32(2, in0123hi_lo);
                convert_store_u8_to_u32(3, in0123hi_hi);
            }
            #endif
            for (; i < bpr; i++) {
                for (size_t j = 0; j < 4 && i * 16 + j * 4 < image->width * image->channels; j++) {
                    dest[i * 16 + j * 4] = (src[i] >> (6 - j * 2)) & 3;
                }
                /*dest[i * 16 + 0 * 4] = (src[i] >> 6) & 3;
                dest[i * 16 + 1 * 4] = (src[i] >> 4) & 3;
                dest[i * 16 + 2 * 4] = (src[i] >> 2) & 3;
                dest[i * 16 + 3 * 4] = (src[i] >> 0) & 3;*/
            }
            break;
        }
        case 4: {
            #ifdef __SSE2__
            for (; i + 16 <= bpr; i += 16) {
                const __m128i in = _mm_loadu_si128((const __m128i*)(src + i));

                const __m128i in0 = _mm_and_si128(_mm_srli_epi64(in, 4), _mm_set1_epi8(0x0F));
                const __m128i in1 = _mm_and_si128(_mm_srli_epi64(in, 0), _mm_set1_epi8(0x0F));

                const __m128i out_lo = _mm_unpacklo_epi8(in0, in1);
                const __m128i out_hi = _mm_unpackhi_epi8(in0, in1);

                convert_store_u8_to_u32(0, out_lo);
                convert_store_u8_to_u32(1, out_hi);
            }
            #endif
            for (; i < bpr; i++) {
                for (size_t j = 0; j < 2 && i * 8 + j * 4 < image->width * image->channels; j++) {
                    dest[i * 8 + j * 4] = (src[i] >> (4 - j * 4)) & 0x0F;
                }
                /*dest[i * 8 + 0 * 4] = (src[i] >> 4) & 0x0F;
                dest[i * 8 + 1 * 4] = (src[i] >> 0) & 0x0F;*/
            }
            break;
        }
        case 8: {
            #ifdef __SSE2__
            for (; i + 16 <= bpr; i += 16)
                convert_store_u8_to_u32(0, _mm_loadu_si128((const __m128i*)(src + i)));
            #endif
            for (; i < bpr; i++)
                dest[i * 4] = src[i];
            break;
        }
        default:
            abort();  // invalid parameter = abort
    }
}

void index_u32_to_RGBA(slp_image_t* restrict image, const uint8_t* restrict palette) {
    if (image->channels != 4)
        abort();  // invalid parameter = abort

    for (size_t i = 0; i + image->channels <= image->image_size; i += image->channels) {
        int index = image->pixels[i] * image->channels;
        for (size_t k = 0; k < image->channels; k++) image->pixels[i + k] = palette[index + k];
    }
}
