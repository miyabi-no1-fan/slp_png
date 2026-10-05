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
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#if defined(__i386__) || defined(__x86_64__)
    #include <immintrin.h>
#endif

#define SLP_PNG_MACROS
#include "slp_image_transform.h"

int slp_image_convert_to_RGBA8(slp_image_t* image) {
    if (slp_image_unpack(image) != 0) return 1;
    if (slp_image_convert_to_8bit(image) != 0) return 1;
    slp_image_t new_image;
    switch (image->channels) {
        case 1: new_image = slp_image_convert_G8_to_RGBA8(image); break;
        case 2: new_image = slp_image_convert_GA8_to_RGBA8(image); break;
        case 3: new_image = slp_image_convert_RGB8_to_RGBA8(image); break;
        case 4: return 0;
        default: return 1;
    }
    slp_image_destroy(image);
    *image = new_image;
    return 0;
}

slp_image_t slp_image_convert_G8_to_RGBA8(slp_image_t* image) {
    if (!image || image->channels != 1 || image->bit_depth != 8)
        return (slp_image_t){ 0 };
    slp_image_t new_image = *image;
    new_image.channels = 4;
    new_image.size = (size_t)new_image.width * (size_t)new_image.height * (size_t)new_image.channels;
    new_image.pixels = (uint8_t*)SLP_MALLOC(new_image.size);
    if (new_image.pixels == NULL) return (slp_image_t){ 0 };
    size_t i = 0;
    #ifdef __AVX2__
    {
        __m256i mask0 = _mm256_setr_epi8(0, 0, 0, -1, 1, 1, 1, -1, 2, 2, 2, -1, 3, 3, 3, -1, 4, 4, 4, -1, 5, 5, 5, -1, 6, 6, 6, -1, 7, 7, 7, -1);
        __m256i mask1 = _mm256_setr_epi8(8, 8, 8, -1, 9, 9, 9, -1, 10, 10, 10, -1, 11, 11, 11, -1, 12, 12, 12, -1, 13, 13, 13, -1, 14, 14, 14, -1, 15, 15, 15, -1);
        __m256i alpha = _mm256_set1_epi32(0xFF000000);
        for (; i + 16 <= (size_t)new_image.width * (size_t)new_image.height; i += 16) {
            __m256i G = _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)(image->pixels + i * image->channels)));
            __m256i x0 = _mm256_or_si256(_mm256_shuffle_epi8(G, mask0), alpha);
            __m256i x1 = _mm256_or_si256(_mm256_shuffle_epi8(G, mask1), alpha);
            _mm256_storeu_si256((__m256i*)(new_image.pixels + i * new_image.channels + 0 * 32), x0);
            _mm256_storeu_si256((__m256i*)(new_image.pixels + i * new_image.channels + 1 * 32), x1);
        }
    }
    #endif
    #ifdef __SSSE3__
    {
        __m128i mask0 = _mm_setr_epi8(0, 0, 0, -1, 1, 1, 1, -1, 2, 2, 2, -1, 3, 3, 3, -1);
        __m128i mask1 = _mm_setr_epi8(4, 4, 4, -1, 5, 5, 5, -1, 6, 6, 6, -1, 7, 7, 7, -1);
        __m128i mask2 = _mm_setr_epi8(8, 8, 8, -1, 9, 9, 9, -1, 10, 10, 10, -1, 11, 11, 11, -1);
        __m128i mask3 = _mm_setr_epi8(12, 12, 12, -1, 13, 13, 13, -1, 14, 14, 14, -1, 15, 15, 15, -1);
        __m128i alpha = _mm_set1_epi32(0xFF000000);
        for (; i + 16 <= (size_t)new_image.width * (size_t)new_image.height; i += 16) {
            __m128i G = _mm_loadu_si128((const __m128i*)(image->pixels + i * image->channels));
            __m128i x0 = _mm_or_si128(_mm_shuffle_epi8(G, mask0), alpha);
            __m128i x1 = _mm_or_si128(_mm_shuffle_epi8(G, mask1), alpha);
            __m128i x2 = _mm_or_si128(_mm_shuffle_epi8(G, mask2), alpha);
            __m128i x3 = _mm_or_si128(_mm_shuffle_epi8(G, mask3), alpha);
            _mm_storeu_si128((__m128i*)(new_image.pixels + i * new_image.channels + 0 * 16), x0);
            _mm_storeu_si128((__m128i*)(new_image.pixels + i * new_image.channels + 1 * 16), x1);
            _mm_storeu_si128((__m128i*)(new_image.pixels + i * new_image.channels + 2 * 16), x2);
            _mm_storeu_si128((__m128i*)(new_image.pixels + i * new_image.channels + 3 * 16), x3);
        }
    }
    #endif
    #ifdef __SSE2__
    for (; i + 16 <= (size_t)new_image.width * (size_t)new_image.height; i += 16) {
        __m128i G = _mm_loadu_si128((const __m128i*)(image->pixels + i * image->channels));
        __m128i GG_lo = _mm_unpacklo_epi8(G, G);
        __m128i GG_hi = _mm_unpackhi_epi8(G, G);
        __m128i GA_lo = _mm_unpacklo_epi8(G, _mm_set1_epi8(-1));
        __m128i GA_hi = _mm_unpackhi_epi8(G, _mm_set1_epi8(-1));
        __m128i GGGA_0 = _mm_unpacklo_epi16(GG_lo, GA_lo);
        __m128i GGGA_1 = _mm_unpackhi_epi16(GG_lo, GA_lo);
        __m128i GGGA_2 = _mm_unpacklo_epi16(GG_hi, GA_hi);
        __m128i GGGA_3 = _mm_unpackhi_epi16(GG_hi, GA_hi);
        _mm_storeu_si128((__m128i*)(new_image.pixels + i * new_image.channels + 0 * 16), GGGA_0);
        _mm_storeu_si128((__m128i*)(new_image.pixels + i * new_image.channels + 1 * 16), GGGA_1);
        _mm_storeu_si128((__m128i*)(new_image.pixels + i * new_image.channels + 2 * 16), GGGA_2);
        _mm_storeu_si128((__m128i*)(new_image.pixels + i * new_image.channels + 3 * 16), GGGA_3);
    }
    #endif
    for (; i < (size_t)new_image.width * (size_t)new_image.height; i++) {
        uint8_t* src = &image->pixels[i * image->channels];
        uint8_t* dst = &new_image.pixels[i * new_image.channels];
        dst[0] = src[0];
        dst[1] = src[0];
        dst[2] = src[0];
        dst[3] = 0xFF;
    }
    return new_image;
}

slp_image_t slp_image_convert_GA8_to_RGBA8(slp_image_t* image) {
    if (!image || image->channels != 2 || image->bit_depth != 8)
        return (slp_image_t){ 0 };
    slp_image_t new_image = *image;
    new_image.channels = 4;
    new_image.size = (size_t)new_image.width * (size_t)new_image.height * (size_t)new_image.channels;
    new_image.pixels = (uint8_t*)SLP_MALLOC(new_image.size);
    if (new_image.pixels == NULL) return (slp_image_t){ 0 };
    size_t i = 0;
    #ifdef __AVX2__
    {
        __m256i mask = _mm256_setr_epi8(0, 0, 0, 1, 2, 2, 2, 3, 4, 4, 4, 5, 6, 6, 6, 7, 8, 8, 8, 9, 10, 10, 10, 11, 12, 12, 12, 13, 14, 14, 14, 15);
        for (; i + 8 <= (size_t)new_image.width * (size_t)new_image.height; i += 8) {
            __m256i GA = _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)(image->pixels + i * image->channels)));
            __m256i RGBA = _mm256_shuffle_epi8(GA, mask);
            _mm256_storeu_si256((__m256i*)(new_image.pixels + i * new_image.channels), RGBA);
        }
    }
    #endif
    #ifdef __SSSE3__
    {
        __m128i mask0 = _mm_setr_epi8(0, 0, 0, 1, 2, 2, 2, 3, 4, 4, 4, 5, 6, 6, 6, 7);
        __m128i mask1 = _mm_setr_epi8(8, 8, 8, 9, 10, 10, 10, 11, 12, 12, 12, 13, 14, 14, 14, 15);
        for (; i + 8 <= (size_t)new_image.width * (size_t)new_image.height; i += 8) {
            __m128i GA = _mm_loadu_si128((const __m128i*)(image->pixels + i * image->channels));
            __m128i x0 = _mm_shuffle_epi8(GA, mask0);
            __m128i x1 = _mm_shuffle_epi8(GA, mask1);
            _mm_storeu_si128((__m128i*)(new_image.pixels + i * new_image.channels + 0 * 16), x0);
            _mm_storeu_si128((__m128i*)(new_image.pixels + i * new_image.channels + 1 * 16), x1);
        }
    }
    #endif
    #ifdef __SSE2__
    for (; i + 8 <= (size_t)new_image.width * (size_t)new_image.height; i += 8) {
        __m128i GA = _mm_loadu_si128((const __m128i*)(image->pixels + i * image->channels));
        __m128i GZ = _mm_and_si128(GA, _mm_set1_epi16(0x00FF));
        __m128i ZG = _mm_slli_epi16(GZ, 8);
        __m128i GG = _mm_or_si128(GZ, ZG);
        __m128i GGGA_lo = _mm_unpacklo_epi16(GG, GA);
        __m128i GGGA_hi = _mm_unpackhi_epi16(GG, GA);
        _mm_storeu_si128((__m128i*)(new_image.pixels + i * new_image.channels + 0 * 16), GGGA_lo);
        _mm_storeu_si128((__m128i*)(new_image.pixels + i * new_image.channels + 1 * 16), GGGA_hi);
    }
    #endif
    for (; i < (size_t)new_image.width * (size_t)new_image.height; i++) {
        uint8_t* src = &image->pixels[i * image->channels];
        uint8_t* dst = &new_image.pixels[i * new_image.channels];
        dst[0] = src[0];
        dst[1] = src[0];
        dst[2] = src[0];
        dst[3] = src[1];
    }
    return new_image;
}

slp_image_t slp_image_convert_RGB8_to_RGBA8(slp_image_t* image) {
    if (!image || image->channels != 3 || image->bit_depth != 8)
        return (slp_image_t){ 0 };
    slp_image_t new_image = *image;
    new_image.channels = 4;
    new_image.size = (size_t)new_image.width * (size_t)new_image.height * (size_t)new_image.channels;
    new_image.pixels = (uint8_t*)SLP_MALLOC(new_image.size);
    if (new_image.pixels == NULL) return (slp_image_t){ 0 };
    size_t i = 0;
    #ifdef __AVX2__
    {
        __m256i mask = _mm256_broadcastsi128_si256(_mm_setr_epi8(0, 1, 2, -1, 3, 4, 5, -1, 6, 7, 8, -1, 9, 10, 11, -1));
        __m256i alpha = _mm256_set1_epi32(0xFF000000);

        for (; i + 10 <= (size_t)new_image.width * (size_t)new_image.height; i += 8) {
            __m128i RGB_lo = _mm_loadu_si128((const __m128i*)(image->pixels + i * image->channels));
            __m128i RGB_hi = _mm_loadu_si128((const __m128i*)(image->pixels + i * image->channels + 12));
            __m256i RGB = _mm256_setr_m128i(RGB_lo, RGB_hi);
            __m256i RGBA = _mm256_or_si256(_mm256_shuffle_epi8(RGB, mask), alpha);
            _mm256_storeu_si256((__m256i*)(new_image.pixels + i * new_image.channels), RGBA);
        }
    }
    #endif
    #ifdef __SSSE3__
    {
        __m128i mask = _mm_setr_epi8(0, 1, 2, -1, 3, 4, 5, -1, 6, 7, 8, -1, 9, 10, 11, -1);
        __m128i alpha = _mm_set1_epi32(0xFF000000);
        for (; i + 6 <= (size_t)new_image.width * (size_t)new_image.height; i += 4) {
            __m128i RGB = _mm_loadu_si128((const __m128i*)(image->pixels + i * image->channels));
            __m128i RGBZ = _mm_shuffle_epi8(RGB, mask);
            __m128i RGBA = _mm_or_si128(RGBZ, alpha);
            _mm_storeu_si128((__m128i*)(new_image.pixels + i * new_image.channels), RGBA);
        }
    }
    #endif
    for (; i < (size_t)new_image.width * (size_t)new_image.height; i++) {
        uint8_t* src = &image->pixels[i * image->channels];
        uint8_t* dst = &new_image.pixels[i * new_image.channels];
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = 0xFF;
    }
    return new_image;
}
