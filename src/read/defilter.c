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

#define SLP_PNG_MACROS
#include "slp_png.h"

static inline uint8_t paeth(uint8_t a, uint8_t b, uint8_t c);

// prev is up, cur is output
int defilter(uint8_t* restrict buffer, uint8_t* restrict cur, uint8_t* restrict prev, const size_t bpp, const size_t bpr) {
    if (!(0 < bpp && bpp <= 8)) abort();
    if (!(bpp <= bpr)) abort();

    uint8_t filter = *buffer++;
    switch (filter) {
        case 0: {
            SLP_MEMCPY(cur, buffer, bpr);
            break;
        }
        case 1: {
            SLP_MEMCPY(cur, buffer, bpp);

            for (size_t i = bpp; i < bpr; i++)
                cur[i] = buffer[i] + cur[i - bpp];

            break;
        }
        case 2: {
            size_t i = 0;

            #ifdef __AVX2__
            for (; i + 32 <= bpr; i += 32) {
                const __m256i raw = _mm256_loadu_si256((const __m256i*)(buffer + i));
                const __m256i up = _mm256_loadu_si256((const __m256i*)(prev + i));
                _mm256_storeu_si256((__m256i*)(cur + i), _mm256_add_epi8(raw, up));
            }
            #endif

            #ifdef __SSE2__
            for (; i + 16 <= bpr; i += 16) {
                const __m128i raw = _mm_loadu_si128((const __m128i*)(buffer + i));
                const __m128i up = _mm_loadu_si128((const __m128i*)(prev + i));
                _mm_storeu_si128((__m128i*)(cur + i), _mm_add_epi8(raw, up));
            }
            #endif

            for (; i < bpr; i++)
                cur[i] = buffer[i] + prev[i];

            break;
        }
        case 3: {
            size_t i = 0;

            for (; i < bpp; i++)
                cur[i] = buffer[i] + prev[i] / 2;

            for (; i < bpr; i++)
                cur[i] = buffer[i] + (prev[i] + cur[i - bpp]) / 2;

            break;
        }
        case 4: {
            size_t i = 0;

            for (; i < bpp; i++)
                cur[i] = buffer[i] + prev[i];

            for (; i < bpr; i++)
                cur[i] = buffer[i] + paeth(cur[i - bpp], prev[i], prev[i - bpp]);

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
