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

#define SLP_PNG_MACROS
#include "slp_png.h"

static inline uint8_t paeth(uint8_t a, uint8_t b, uint8_t c);

// scanline[0] is up, scanline[1] is output
int defilter(uint8_t* restrict buffer, uint8_t* restrict* restrict scanline, const size_t bpp, const size_t bpr, const size_t imtrker) {
    if (!(bpp <= 8)) __builtin_unreachable();
    if (!(bpp <= bpr)) __builtin_unreachable();

    uint8_t filter = *buffer++;
    switch (filter) {
        case 0: {
            SLP_MEMCPY(scanline[1], buffer, bpr);
            break;
        }
        case 1: {
            SLP_MEMCPY(scanline[1], buffer, bpp);
            for (size_t i = bpp; i < bpr; i++) scanline[1][i] = buffer[i] + scanline[1][i - bpp];
            break;
        }
        case 2: {
            if (imtrker == 0)
                SLP_MEMCPY(scanline[1], buffer, bpr);
            else {
                size_t i = 0;
                #ifdef __AVX2__
                for (; i + 32 <= bpr; i += 32) {
                    const __m256i raw = _mm256_loadu_si256((const __m256i*)(buffer + i));
                    const __m256i up = _mm256_loadu_si256((const __m256i*)(scanline[0] + i));
                    _mm256_storeu_si256((__m256i*)(scanline[1] + i), _mm256_add_epi8(raw, up));
                }
                #endif
                #ifdef __SSE2__
                for (; i + 16 <= bpr; i += 16) {
                    const __m128i raw = _mm_loadu_si128((const __m128i*)(buffer + i));
                    const __m128i up = _mm_loadu_si128((const __m128i*)(scanline[0] + i));
                    _mm_storeu_si128((__m128i*)(scanline[1] + i), _mm_add_epi8(raw, up));
                }
                #endif
                for (; i < bpr; i++) scanline[1][i] = buffer[i] + scanline[0][i];
            }
            break;
        }
        case 3: {
            if (imtrker == 0) {
                SLP_MEMCPY(scanline[1], buffer, bpp);
                for (size_t i = bpp; i < bpr; i++) scanline[1][i] = buffer[i] + (scanline[1][i - bpp] >> 1);
            } else {
                size_t i = 0;
                for (; i < bpp; i++) scanline[1][i] = buffer[i] + ((scanline[0][i]) >> 1);
                for (; i < bpr; i++) scanline[1][i] = buffer[i] + ((scanline[0][i] + scanline[1][i - bpp]) >> 1);
            }
            break;
        }
        case 4: {
            if (imtrker == 0) {
                SLP_MEMCPY(scanline[1], buffer, bpp);
                for (size_t i = bpp; i < bpr; i++) scanline[1][i] = buffer[i] + scanline[1][i - bpp];
            } else {
                size_t i = 0;
                for (; i < bpp; i++) scanline[1][i] = buffer[i] + scanline[0][i];
                for (; i < bpr; i++) scanline[1][i] = buffer[i] + paeth(scanline[1][i - bpp], scanline[0][i], scanline[0][i - bpp]);
            }
            break;
        }
        default: return 1;
    }
    return 0;
}

static inline uint8_t paeth(const uint8_t a, const uint8_t b, const uint8_t c) {
    const int p = (int)a + b - c;
    const int pa = abs(p - a);
    const int pb = abs(p - b);
    const int pc = abs(p - c);

    uint8_t d = (pb <= pc) ? b : c;
    d = (pa <= pb && pa <= pc) ? a : d;

    return d;
}
