/*
Copyright 2026 slp-c

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
#include <slp_image.h>
#include <slp_image_transform.h>
#include <slp_png.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __unix__
#include <unistd.h>
#endif

#if defined(__i386__) || defined(__x86_64__)
#include <immintrin.h>
#endif

static int get_nproc(void) {
    #ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
    #endif
    #ifdef __unix__
    return sysconf(_SC_NPROCESSORS_ONLN);
    #endif
}



bool slp_image_convert_to_8bit(slp_image_t* image) {

    uint8_t* src = image->buffer;
    uint8_t* dest = image->buffer;

    const size_t size = image->width * image->height * image->channels; // dest size

    size_t i = 0;
    switch (image->bit_depth) {
        case 1: {
            #ifdef __AVX2__
            for (; i + 32 <= size; i += 32) {
                __m256i in = _mm256_loadu_si256((const __m256i*)(src + i));
                in = _mm256_blendv_epi8(_mm256_setzero_si256(), _mm256_set1_epi8(-1), in);
                _mm256_storeu_si256((__m256i*)(dest + i), in);
            }
            #endif
            #ifdef __SSE2__
            for (; i + 16 <= size; i += 16) {
                __m128i in = _mm_loadu_si128((const __m128i*)(src + i));
                #ifdef __SSE4_1__
                in = _mm_blendv_epi8(_mm_setzero_si128(), _mm_set1_epi8(-1), in);
                #else
                __m128i in_lo = _mm_unpacklo_epi8(in, _mm_setzero_si128());
                __m128i in_hi = _mm_unpackhi_epi8(in, _mm_setzero_si128());
                in_lo = _mm_mullo_epi16(in_lo, _mm_set1_epi8(-1));
                in_hi = _mm_mullo_epi16(in_hi, _mm_set1_epi8(-1));
                in = _mm_packus_epi16(in_lo, in_hi);
                #endif
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
        case 8: return true;
        case 16: {
            uint16_t random_value_for_edian_test = 1;
            const bool is_little_edian = *(uint8_t*)(&random_value_for_edian_test);

            #ifdef __AVX2__
            for (; i + 16 <= size; i += 16) {
                __m256i in = _mm256_loadu_si256((const __m256i*)(src + i*2));
                in = _mm256_srli_epi16(in, 8);
                in = _mm256_packus_epi16(in, _mm256_setzero_si256());
                #ifdef __i386__
                *(uint32_t*)(dest + i + 0*4) = _mm_extract_epi32(in, 0);
                *(uint32_t*)(dest + i + 1*4) = _mm_extract_epi32(in, 1);
                *(uint32_t*)(dest + i + 2*4) = _mm_extract_epi32(in, 4);
                *(uint32_t*)(dest + i + 3*4) = _mm_extract_epi32(in, 5);
                #else
                *(uint64_t*)(dest + i + 8 * 0) = _mm256_extract_epi64(in, 0);
                *(uint64_t*)(dest + i + 8 * 1) = _mm256_extract_epi64(in, 2);
                #endif
            }
            #endif
            #ifdef __SSE4_1__
            for (; i + 8 <= size; i += 8) {
                __m128i in = _mm_loadu_si128((const __m128i*)(src + i*2));
                in = _mm_srli_epi16(in, 8);
                in = _mm_packus_epi16(in, _mm_setzero_si128());

                #ifdef __i386__
                *(uint32_t*)(dest + i + 0*4) = _mm_extract_epi32(in, 0);
                *(uint32_t*)(dest + i + 1*4) = _mm_extract_epi32(in, 1);
                #else
                *(uint64_t*)(dest + i) = _mm_cvtsi128_si64(in);
                #endif
            }
            #endif
            for (; i < size; i++) dest[i] = src[i*2 + is_little_edian];
            break;
        }
        default: return false;
    }
    image->bit_depth = 8;
    return true;
}



// return false = SLP_MALLOC fail or input wrong
bool slp_image_convert_to_16bit(slp_image_t* image) {

    const size_t size = image->height * image->width * image->channels; // source size

    uint8_t* new_buffer = (uint8_t*)SLP_ALIGNED_ALLOC(size * 2);
    if (new_buffer == NULL) {
        if (image->bit_depth == 16) return true;
        return false;
    }

    uint8_t* src = image->buffer;
    uint8_t* dest = new_buffer;

    size_t i = 0;
    switch (image->bit_depth) {
        case 1: {
            #ifdef __AVX2__
            for (; i + 32 <= size; i += 32) {
                const __m256i in = _mm256_loadu_si256((const __m256i*)(src + i));

                __m256i in_lo = _mm256_unpacklo_epi8(in, in);
                __m256i in_hi = _mm256_unpackhi_epi8(in, in);

                in_lo = _mm256_blendv_epi8(_mm256_setzero_si256(), _mm256_set1_epi8(-1), in_lo);
                in_hi = _mm256_blendv_epi8(_mm256_setzero_si256(), _mm256_set1_epi8(-1), in_hi);

                _mm_storeu_si128((__m128i*)(dest + i*2 + 0*16), _mm256_castsi256_si128(in_lo));
                _mm_storeu_si128((__m128i*)(dest + i*2 + 1*16), _mm256_castsi256_si128(in_hi));
                _mm_storeu_si128((__m128i*)(dest + i*2 + 2*16), _mm256_extracti128_si256(in_lo, 1));
                _mm_storeu_si128((__m128i*)(dest + i*2 + 3*16), _mm256_extracti128_si256(in_hi, 1));
            }
            #endif
            #ifdef __SSE2__
            for (; i + 16 <= size; i += 16) {
                const __m128i in = _mm_loadu_si128((const __m128i*)(src + i));

                #ifdef __SSE4_1__
                __m128i in_lo = _mm_unpacklo_epi8(in, in);
                __m128i in_hi = _mm_unpackhi_epi8(in, in);
                in_lo = _mm_blendv_epi8(_mm_setzero_si128(), _mm_set1_epi8(-1), in_lo);
                in_hi = _mm_blendv_epi8(_mm_setzero_si128(), _mm_set1_epi8(-1), in_hi);
                #else
                __m128i in_lo = _mm_unpacklo_epi8(in, _mm_setzero_si128());
                __m128i in_hi = _mm_unpackhi_epi8(in, _mm_setzero_si128());
                in_lo = _mm_mullo_epi16(in_lo, _mm_set1_epi16(-1));
                in_hi = _mm_mullo_epi16(in_hi, _mm_set1_epi16(-1));
                #endif

                _mm_storeu_si128((__m128i*)(dest + i*2 + 0*16), in_lo);
                _mm_storeu_si128((__m128i*)(dest + i*2 + 1*16), in_hi);
            }
            #endif
            for (; i < size; i++) ((uint16_t*)dest)[i] = src[i] * 0xFFFF;
            break;
        }
        case 2: {
            #ifdef __AVX2__
            {
                const __m256i scalar = _mm256_set1_epi16(21845);// = 65535/3
                for (; i + 32 <= size; i += 32) {
                    const __m256i in = _mm256_loadu_si256((const __m256i*)(src + i));

                    __m256i in_lo = _mm256_unpacklo_epi8(in, _mm256_setzero_si256());
                    __m256i in_hi = _mm256_unpackhi_epi8(in, _mm256_setzero_si256());

                    in_lo = _mm256_mullo_epi16(in_lo, scalar);
                    in_hi = _mm256_mullo_epi16(in_hi, scalar);

                    _mm_storeu_si128((__m128i*)(dest + i*2 + 0*16), _mm256_castsi256_si128(in_lo));
                    _mm_storeu_si128((__m128i*)(dest + i*2 + 1*16), _mm256_castsi256_si128(in_hi));
                    _mm_storeu_si128((__m128i*)(dest + i*2 + 2*16), _mm256_extracti128_si256(in_lo, 1));
                    _mm_storeu_si128((__m128i*)(dest + i*2 + 3*16), _mm256_extracti128_si256(in_hi, 1));
                }
            }
            #endif
            #ifdef __SSE2__
            {
                const __m128i scalar = _mm_set1_epi16(21845);// = 65535/3
                for (; i + 16 <= size; i += 16) {
                    const __m128i in = _mm_loadu_si128((const __m128i*)(src + i));

                    __m128i in_lo = _mm_unpacklo_epi8(in, _mm_setzero_si128());
                    __m128i in_hi = _mm_unpackhi_epi8(in, _mm_setzero_si128());

                    in_lo = _mm_mullo_epi16(in_lo, scalar);
                    in_hi = _mm_mullo_epi16(in_hi, scalar);

                    _mm_storeu_si128((__m128i*)(dest + i*2 + 0*16), in_lo);
                    _mm_storeu_si128((__m128i*)(dest + i*2 + 1*16), in_hi);
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

                _mm_storeu_si128((__m128i*)(dest + i*2 + 0*16), _mm256_castsi256_si128(in_lo));
                _mm_storeu_si128((__m128i*)(dest + i*2 + 1*16), _mm256_castsi256_si128(in_hi));
                _mm_storeu_si128((__m128i*)(dest + i*2 + 2*16), _mm256_extracti128_si256(in_lo, 1));
                _mm_storeu_si128((__m128i*)(dest + i*2 + 3*16), _mm256_extracti128_si256(in_hi, 1));
            }
            #endif
            #ifdef __SSE2__
            for (; i + 16 <= size; i += 16) {
                __m128i in = _mm_loadu_si128((const __m128i*)(src + i));
                in = _mm_or_si128(in, _mm_slli_epi64(in, 4));

                const __m128i in_lo = _mm_unpacklo_epi8(in, in);
                const __m128i in_hi = _mm_unpackhi_epi8(in, in);

                _mm_storeu_si128((__m128i*)(dest + i*2 + 0*16), in_lo);
                _mm_storeu_si128((__m128i*)(dest + i*2 + 1*16), in_hi);
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
            SLP_FREE(new_buffer);
            return true;
        }
        default: {
            SLP_FREE(new_buffer);
            return false;
        }
    }

    SLP_FREE(image->buffer);
    image->buffer = new_buffer;
    image->bit_depth = 16;
    image->image_size = size * 2;
    image->allocated_size = SLP_ALIGN_SIZE(size * 2);

    return true;
}



struct slp_image_crop_thread_data {
    size_t c;
    size_t src_stride;
    size_t dest_stride;
    uint32_t offset_width;
    uint32_t offset_height;
    slp_image_t *image;
    uint8_t* new_buffer;
    size_t block;
    size_t last_block;
    int s;
    int P;
};

static void* slp_image_crop_thread_task(void* arg) {
    struct slp_image_crop_thread_data data = *(struct slp_image_crop_thread_data*)arg;
    uint8_t* src = data.image->buffer + (size_t)(data.offset_height + data.s * data.block) * data.src_stride + (size_t)data.offset_width * data.c;
    uint8_t* dest = data.new_buffer + data.s * data.block * data.dest_stride;
    for (size_t i = data.s * data.block; i < data.s * data.block + ((data.s == data.P - 1) ? data.last_block : data.block); i++) SLP_MEMCPY(dest + i * data.dest_stride, src + i * data.src_stride, data.dest_stride);
    return NULL;
}

bool image_crop(slp_image_t* image, const uint32_t new_width, const uint32_t new_height, const uint32_t offset_width, const uint32_t offset_height) {
    if (offset_width + new_width > image->width || offset_height + new_height > image->height) return false;

    const size_t c = (size_t)image->channels * (1 + (image->bit_depth == 16)); // sizeof 1 pixel
    const size_t src_stride = (size_t)image->width * c;
    const size_t dest_stride = (size_t)new_width * c;

    const size_t new_size = dest_stride * new_height;
    uint8_t* new_buffer = (uint8_t*)SLP_ALIGNED_ALLOC(new_size);
    if (new_buffer == NULL) return false;

    const int P = (get_nproc() <= 1) ? (2) : (get_nproc());

    struct slp_image_crop_thread_data *threads_arg = (struct slp_image_crop_thread_data*)SLP_MALLOC(P * sizeof(*threads_arg));
    if (threads_arg == NULL) {
        SLP_FREE(new_buffer);
        return false;
    }

    pthread_t* threads = (pthread_t*)SLP_MALLOC(P * sizeof(*threads));
    if (threads == NULL) {
        SLP_FREE(new_buffer);
        SLP_FREE(threads_arg);
        return false;
    }

    const size_t block = new_height / (P - 1); // P threads, P-1 work for block of scanline, the remain 1 thread works for whatever remains
    const size_t last_block = new_height - block * (P-1); // == new_height % (P-1)

    int s = 0;
    for (; s < P-1; s++) {
        threads_arg[s].c = c;
        threads_arg[s].src_stride = src_stride;
        threads_arg[s].dest_stride = dest_stride;
        threads_arg[s].offset_width = offset_width;
        threads_arg[s].offset_height = offset_height;
        threads_arg[s].image = image;
        threads_arg[s].new_buffer = new_buffer;
        threads_arg[s].block = block;
        threads_arg[s].last_block = last_block;
        threads_arg[s].s = s;
        threads_arg[s].P = P;

        if (pthread_create(threads + s, NULL, slp_image_crop_thread_task, threads_arg + s) != 0) {
            for (int i = 0; i < s; i++) {
                pthread_join(threads[i], NULL);
            }
            SLP_FREE(threads);
            SLP_FREE(new_buffer);
            SLP_FREE(threads_arg);
            return false;
        }
    }
    
    threads_arg[s].c = c;
    threads_arg[s].src_stride = src_stride;
    threads_arg[s].dest_stride = dest_stride;
    threads_arg[s].offset_width = offset_width;
    threads_arg[s].offset_height = offset_height;
    threads_arg[s].image = image;
    threads_arg[s].new_buffer = new_buffer;
    threads_arg[s].block = block;
    threads_arg[s].last_block = last_block;
    threads_arg[s].s = s;
    threads_arg[s].P = P;

    if (pthread_create(threads + s, NULL, slp_image_crop_thread_task, threads_arg + s) != 0) {
        for (int i = 0; i < s; i++) {
            pthread_join(threads[i], NULL);
        }
        SLP_FREE(threads);
        SLP_FREE(new_buffer);
        SLP_FREE(threads_arg);
        return false;
    }

    for (int i = 0; i < P; i++) pthread_join(threads[i], NULL);

    SLP_FREE(threads);
    SLP_FREE(threads_arg);
    SLP_FREE(image->buffer);

    image->buffer = new_buffer;
    image->width = new_width;
    image->height = new_height;
    image->image_size = new_size;
    image->allocated_size = SLP_ALIGN_SIZE(new_size);

    return true;
}



void slp_image_fill(uint8_t* buffer, const size_t buffer_size, const uint8_t* pixel, const uint8_t pixel_size) {
    SLP_MEMCPY(buffer, pixel, pixel_size);
    size_t i = pixel_size;
    for (; i * 2 <= buffer_size; i *= 2)
        SLP_MEMCPY(buffer + i, buffer, i);
    SLP_MEMCPY(buffer + i, buffer, buffer_size - i);
}



struct slp_image_linear_transform_thread_arg {
    slp_image_t *image;
    uint8_t *background;
    double *inverseA;
    double half_height;
    double half_width;
    double umin;
    double vmax;
    uint32_t new_width;
    size_t pixel_size;
    size_t src_stride;
    size_t dst_stride;
    uint8_t* new_buffer;
    uint8_t* src;
    int P;
    size_t block;
    size_t last_block;
    int s;
    pthread_mutex_t* mtx;
    bool* start_exec;
};

static void* slp_image_linear_transform_thread_task(void* arg) {

    struct slp_image_linear_transform_thread_arg data = *(struct slp_image_linear_transform_thread_arg*)arg;

    size_t i = data.s * data.block;

    double c1 = (-i + data.vmax) * data.inverseA[1] + data.half_width;
    double c2 = (-i + data.vmax) * data.inverseA[3] - data.half_height;

    double X = data.umin * data.inverseA[0] + c1;
    double Y = -data.umin * data.inverseA[2] - c2;

    int flagx;
    if (data.inverseA[0] > 0) flagx = 1;
    else if (data.inverseA[0] < 0) flagx = -1;
    else flagx = 0;

    int flagy;
    if (data.inverseA[2] > 0) flagy = 1;
    else if (data.inverseA[2] < 0) flagy = -1;
    else flagy = 0;

    pthread_mutex_lock(data.mtx);
    if (!*data.start_exec) return NULL;

    for (; i < data.s * data.block + ((data.s == data.P - 1) ? data.last_block : data.block); i++) {
        
        uint8_t* dest = data.new_buffer + i * data.dst_stride;

        double x0, x2, y0, y2;

        switch (flagx) {
            case 1: {
                x0 = -X / data.inverseA[0];
                x2 = (data.image->width - X - 1) / data.inverseA[0];
                break;
            }
            case -1: {
                x0 = (data.image->width - X - 1) / data.inverseA[0];
                x2 = -X / data.inverseA[0];
                break;
            }
            case 0: {
                x0 = 0;
                x2 = data.new_width;
                break;
            }
        }

        switch (flagy) {
            case 1: {
                y0 = (Y - data.image->height + 1) / data.inverseA[2];
                y2 = Y / data.inverseA[2];
                break;
            }
            case -1: {
                y0 = Y / data.inverseA[2];
                y2 = (Y - data.image->height + 1) / data.inverseA[2];
                break;
            }
            case 0: {
                y0 = 0;
                y2 = data.new_width;
                break;
            }
        }

        ssize_t start = ceil(fmax(x0, y0));
        ssize_t end = fmin(x2, y2);
        
        dest += start * data.pixel_size;

        ssize_t mid = start;

        double x1 = mid * data.inverseA[0] + X;
        double y1 = -mid * data.inverseA[2] + Y;
        
        for (; mid < end; mid++) { // data
            SLP_MEMCPY(dest, data.src + ((ssize_t)y1) * data.src_stride + ((ssize_t)x1) * data.pixel_size, data.pixel_size);
            dest += data.pixel_size;
            x1 += data.inverseA[0];
            y1 += -data.inverseA[2];
        }

        c1 -= data.inverseA[1];
        c2 -= data.inverseA[3];

        X -= data.inverseA[1];
        Y += data.inverseA[3];
    }

    pthread_mutex_unlock(data.mtx);
    return NULL;
}

bool slp_image_linear_transform(slp_image_t* restrict image, const double* restrict A, const uint8_t* restrict background) {
    const double detA = A[0] * A[3] - A[1] * A[2];
    if (detA == 0) {
        SLP_MEMSET(image->buffer, 0, image->image_size);
        image->height = 0;
        image->width = 0;
        return true;
    }
    const double inverseA[4] = { A[3] / detA, -A[1] / detA, -A[2] / detA, A[0] / detA };
    // first we will calculate its new size, through the 4 corners
    // 0, (H-1) -> -(H-1)/2, (H-1)/2
    // 0, (W-1) -> -(W-1)/2, (W-1)/2
    double half_height = ((double)image->height-1)/2;// y
    double half_width = ((double)image->width-1)/2;// x

    // 1    2
    // 3    4
    const double u1 = -half_width * A[0] + half_height * A[1];
    const double u2 = half_width * A[0] + half_height * A[1];
    const double u3 = -half_width * A[0] + -half_height * A[1];
    const double u4 = half_width * A[0] + -half_height * A[1];

    const double v1 = -half_width * A[2] + half_height * A[3];
    const double v2 = half_width * A[2] + half_height * A[3];
    const double v3 = -half_width * A[2] + -half_height * A[3];
    const double v4 = half_width * A[2] + -half_height * A[3];

    const double umax = fmax(fmax(fmax(u1, u2), u3), u4);
    const double umin = fmin(fmin(fmin(u1, u2), u3), u4);
    const double vmax = fmax(fmax(fmax(v1, v2), v3), v4);
    const double vmin = fmin(fmin(fmin(v1, v2), v3), v4);

    const uint32_t new_width = (uint32_t)(umax - umin + 1);
    const uint32_t new_height = (uint32_t)(vmax - vmin + 1);

    const size_t pixel_size = image->channels * (1 + (image->bit_depth == 16)); // sizeof 1 pixel
    const size_t src_stride = image->width * pixel_size; // sizeof 1 scanline
    const size_t dst_stride = new_width * pixel_size; // sizeof 1 dest scanline
    const size_t new_size = (size_t)new_width * new_height * pixel_size;

    bool return_code = true;
    uint8_t* new_buffer = NULL;
    pthread_t* threads = NULL;
    struct slp_image_linear_transform_thread_arg* threads_arg = NULL;
    pthread_mutex_t* mtx = NULL;

    pthread_mutexattr_t mtxattr;
    if (pthread_mutexattr_init(&mtxattr) != 0) return false;
    const int P = (get_nproc() <= 1) ? 2 : get_nproc();

    new_buffer = (uint8_t*)SLP_ALIGNED_ALLOC(new_size);
    threads = (pthread_t*)SLP_MALLOC(P * sizeof(*threads));
    threads_arg = (struct slp_image_linear_transform_thread_arg*)SLP_MALLOC(P * sizeof(*threads_arg));
    mtx = (pthread_mutex_t*)SLP_MALLOC(P * sizeof(*mtx));
    if (new_buffer == NULL ||
        threads == NULL ||
        threads_arg == NULL ||
        mtx == NULL)
    {
        return_code = false;
        goto cleanup;
    }

    slp_image_fill(new_buffer, new_size, background, pixel_size);
    uint8_t* src = image->buffer;

    const size_t block = new_height / (P - 1); // P threads, P-1 work for block of scanline, the remain 1 thread works for what remain:)
    const size_t last_block = new_height - block * (P-1); // == new_height % (P-1)

    int s = 0;
    for (; s < P-1; s++) {
        threads_arg[s].image = image;
        threads_arg[s].background = (uint8_t*)background;
        threads_arg[s].inverseA = (double*)inverseA;
        threads_arg[s].half_height = half_height;
        threads_arg[s].half_width = half_width;
        threads_arg[s].umin = umin;
        threads_arg[s].vmax = vmax;
        threads_arg[s].new_width = new_width;
        threads_arg[s].pixel_size = pixel_size;
        threads_arg[s].src_stride = src_stride;
        threads_arg[s].dst_stride = dst_stride;
        threads_arg[s].new_buffer = new_buffer;
        threads_arg[s].src = src;
        threads_arg[s].P = P;
        threads_arg[s].block = block;
        threads_arg[s].last_block = last_block;
        threads_arg[s].s = s;
        threads_arg[s].mtx = mtx + s;
        threads_arg[s].start_exec = &return_code;
        
        if (pthread_mutex_init(mtx + s, &mtxattr) != 0 ||
            pthread_mutex_lock(mtx + s) != 0 ||
            pthread_create(threads + s, NULL, slp_image_linear_transform_thread_task, threads_arg + s) != 0)
        {
            return_code = false;
            for (int i = 0; i < s; i++) {
                pthread_mutex_unlock(mtx + i);
                pthread_mutex_destroy(mtx + i);
            }
            for (int i = 0; i < s; i++) pthread_join(threads[i], NULL);
            goto cleanup;
        }
    }

    threads_arg[s].image = image;
    threads_arg[s].background = (uint8_t*)background;
    threads_arg[s].inverseA = (double*)inverseA;
    threads_arg[s].half_height = half_height;
    threads_arg[s].half_width = half_width;
    threads_arg[s].umin = umin;
    threads_arg[s].vmax = vmax;
    threads_arg[s].new_width = new_width;
    threads_arg[s].pixel_size = pixel_size;
    threads_arg[s].src_stride = src_stride;
    threads_arg[s].dst_stride = dst_stride;
    threads_arg[s].new_buffer = new_buffer;
    threads_arg[s].src = src;
    threads_arg[s].P = P;
    threads_arg[s].block = block;
    threads_arg[s].last_block = last_block;
    threads_arg[s].s = s;

    if (pthread_mutex_init(mtx + s, &mtxattr) != 0 ||
        pthread_mutex_lock(mtx + s) != 0 ||
        pthread_create(threads + s, NULL, slp_image_linear_transform_thread_task, threads_arg + s) != 0)
    {
        return_code = false;
        for (int i = 0; i < s; i++) {
            pthread_mutex_unlock(mtx + i);
            pthread_mutex_destroy(mtx + i);
        }
        for (int i = 0; i < s; i++) pthread_join(threads[i], NULL);
        goto cleanup;
    }

    for (int i = 0; i < s; i++) {
        pthread_mutex_unlock(mtx + i);
        pthread_mutex_destroy(mtx + i);
    }
    for (int i = 0; i < P; i++) pthread_join(threads[i], NULL);

    image->buffer = new_buffer;
    image->width = new_width;
    image->height = new_height;
    image->image_size = new_size;
    image->allocated_size = SLP_ALIGN_SIZE(new_size);

cleanup:
    SLP_FREE(threads);
    SLP_FREE(threads_arg);
    pthread_mutexattr_destroy(&mtxattr);
    SLP_FREE(mtx);
    if (return_code) SLP_FREE(image->buffer);
    else SLP_FREE(new_buffer);
    return return_code;
}



bool slp_image_format(slp_image_t* image) {

    const size_t size = (size_t)image->width * image->height * image->channels * (1 + (image->bit_depth == 16)); // dest size

    uint8_t *new_buffer = (uint8_t*)SLP_ALIGNED_ALLOC(size);
    if (new_buffer == NULL) {
        if (image->bit_depth == 8) return true;
        return false;
    }

    uint8_t *src = image->buffer;
    uint8_t *dest = new_buffer;

    size_t i = 0;
    switch (image->bit_depth) {
        case 1: {
            #ifdef __SSE2__
            for (; i + 128 <= size; i += 128) {
                const __m128i in = _mm_loadu_si128((const __m128i*)(src + i/8));

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
                const __m128i in = _mm_loadu_si128((const __m128i*)(src + i/4));

                const __m128i in0 = _mm_and_si128(_mm_srli_epi64(in, 0), _mm_set1_epi8(3));//0b11
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
                const __m256i in = _mm256_loadu_si256((const __m256i*)(src + i/2));
                
                const __m256i in0 = _mm256_and_si256(_mm256_srli_epi64(in, 0), _mm256_set1_epi8(0x0F));
                const __m256i in1 = _mm256_and_si256(_mm256_srli_epi64(in, 4), _mm256_set1_epi8(0x0F));
                
                const __m256i a0 = _mm256_unpacklo_epi8(in0, in1); // 0 x 1 lo
                const __m256i a1 = _mm256_unpackhi_epi8(in0, in1); // 0 x 1 hi

                _mm_storeu_si128((__m128i*)(dest + i + 0 * 16), _mm256_castsi256_si128(a0));
                _mm_storeu_si128((__m128i*)(dest + i + 1 * 16), _mm256_castsi256_si128(a1));
                _mm_storeu_si128((__m128i*)(dest + i + 2 * 16), _mm256_extracti128_si256(a0, 1));
                _mm_storeu_si128((__m128i*)(dest + i + 3 * 16), _mm256_extracti128_si256(a1, 1));
            }
            #endif
            #ifdef __SSE2__
            for (; i + 32 <= size; i += 32) {
                const __m128i in = _mm_loadu_si128((const __m128i*)(src + i/2));
                
                const __m128i in0 = _mm_and_si128(_mm_srli_epi64(in, 0), _mm_set1_epi8(0x0F));
                const __m128i in1 = _mm_and_si128(_mm_srli_epi64(in, 4), _mm_set1_epi8(0x0F));
                
                const __m128i a0 = _mm_unpacklo_epi8(in0, in1); // 0 x 1 lo
                const __m128i a1 = _mm_unpackhi_epi8(in0, in1); // 0 x 1 hi

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
            const uint16_t random_value_for_edian_test = 1;
            if (!(*(uint8_t*)(&random_value_for_edian_test))) {// if big edian
                SLP_FREE(new_buffer);
                return true;
            }

            #ifdef __AVX2__
            for (; i + 32 <= size; i += 32) {
                const __m256i in = _mm256_loadu_si256((const __m256i*)(src + i));
                const __m256i out = _mm256_or_si256(_mm256_slli_epi16(in, 8), _mm256_srli_epi16(in, 8));
                _mm256_storeu_si256((__m256i*)(dest + i), out);
            }
            #endif
            #ifdef __SSE2__
            for (; i + 16 <= size; i += 16) {
                const __m128i in = _mm_loadu_si128((const __m128i*)(src + i));
                const __m128i out = _mm_or_si128(_mm_slli_epi16(in, 8), _mm_srli_epi16(in, 8));
                _mm_storeu_si128((__m128i*)(dest + i), out);
            }
            #endif
            for (; i < size; i+=2) {
                dest[i + 0] = src[i + 1];
                dest[i + 1] = src[i + 0];
            }
            break;
        }
        default: {
            SLP_FREE(new_buffer);
            return false;
        }
    }

    SLP_FREE(image->buffer);
    image->buffer = new_buffer;
    image->image_size = size;
    image->allocated_size = SLP_ALIGN_SIZE(size);

    return true;
}



bool slp_image_unformat(slp_image_t* image) {

    const size_t size = (size_t)image->height * image->width * image->channels * (1 + (image->bit_depth == 16));
    const size_t new_size = image->image_size;
    
    uint8_t* new_buffer = (uint8_t*)SLP_ALIGNED_ALLOC(new_size);
    if (new_buffer == NULL) return false;

    uint8_t* src = (uint8_t*)(image->buffer);
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

                    in = _mm256_and_si256(in, _mm256_set1_epi8(1)); // take last 1 bit

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

                    *(uint16_t*)(dest + i/8 + 0) = _mm256_extract_epi16(out, 0);
                    *(uint16_t*)(dest + i/8 + 2) = _mm256_extract_epi16(out, 8);
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

                    in = _mm_and_si128(in, _mm_set1_epi8(1)); // take last 1 bit

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

                    *(uint16_t*)(dest + i/8) = _mm_extract_epi16(out, 0);
                }
            }
            #endif
            for (; i + 8 <= size; i += 8) {
                dest[i/8] = (src[i + 0] & 1) << 7 | 
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
                    in = _mm256_and_si256(in, _mm256_set1_epi8(3)); // take last 2 bit

                    const __m256i b1 = _mm256_slli_si256(_mm256_slli_epi32(_mm256_and_si256(in, mask1), 6), 0);
                    const __m256i b2 = _mm256_slli_si256(_mm256_slli_epi32(_mm256_and_si256(in, mask2), 4), 1);
                    const __m256i b3 = _mm256_slli_si256(_mm256_slli_epi32(_mm256_and_si256(in, mask3), 2), 2);
                    const __m256i b4 = _mm256_slli_si256(_mm256_slli_epi32(_mm256_and_si256(in, mask4), 0), 3);

                    __m256i out = _mm256_or_si256(_mm256_or_si256(b1, b2), _mm256_or_si256(b3, b4));
                    out = _mm256_shuffle_epi8(out, extract);

                    *(uint32_t*)(dest + i/4 + 0) = _mm256_extract_epi32(out, 0);
                    *(uint32_t*)(dest + i/4 + 4) = _mm256_extract_epi32(out, 4);
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
                    in = _mm_and_si128(in, _mm_set1_epi8(3)); // take last 2 bit

                    const __m128i b1 = _mm_slli_si128(_mm_slli_epi32(_mm_and_si128(in, mask1), 6), 0);
                    const __m128i b2 = _mm_slli_si128(_mm_slli_epi32(_mm_and_si128(in, mask2), 4), 1);
                    const __m128i b3 = _mm_slli_si128(_mm_slli_epi32(_mm_and_si128(in, mask3), 2), 2);
                    const __m128i b4 = _mm_slli_si128(_mm_slli_epi32(_mm_and_si128(in, mask4), 0), 3);

                    __m128i out = _mm_or_si128(_mm_or_si128(b1, b2), _mm_or_si128(b3, b4));
                    out = _mm_shuffle_epi8(out, extract);

                    *(uint32_t*)(dest + i/4) = _mm_cvtsi128_si32(out);
                }
            }
            #endif
            for (; i + 4 <= size; i += 4) {
                dest[i/4] = (src[i + 0] & 3) << 6 | 
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
                    in = _mm256_and_si256(in, _mm256_set1_epi8(0x0F)); // take the first 4 bit

                    const __m256i b1 = _mm256_slli_si256(_mm256_slli_epi16(_mm256_and_si256(in, mask1), 4), 0);
                    const __m256i b2 = _mm256_slli_si256(_mm256_slli_epi16(_mm256_and_si256(in, mask2), 0), 1);

                    __m256i out = _mm256_or_si256(b1, b2);
                    out = _mm256_shuffle_epi8(out, extract);

                    #ifdef __i386__
                    *(uint32_t*)(dest + i/2 + 0)  = _mm256_extract_epi32(out, 0);
                    *(uint32_t*)(dest + i/2 + 4)  = _mm256_extract_epi32(out, 1);
                    *(uint32_t*)(dest + i/2 + 8)  = _mm256_extract_epi32(out, 4);
                    *(uint32_t*)(dest + i/2 + 12) = _mm256_extract_epi32(out, 5);
                    #else
                    *(uint64_t*)(dest + i/2 + 0) = _mm256_extract_epi64(out, 0);
                    *(uint64_t*)(dest + i/2 + 8) = _mm256_extract_epi64(out, 2);
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
                    in = _mm_and_si128(in, _mm_set1_epi8(0x0F)); // take the last 4 bit

                    const __m128i b1 = _mm_slli_si128(_mm_slli_epi16(_mm_and_si128(in, mask1), 4), 0);
                    const __m128i b2 = _mm_slli_si128(_mm_slli_epi16(_mm_and_si128(in, mask2), 0), 1);

                    __m128i out = _mm_or_si128(b1, b2);
                    out = _mm_shuffle_epi8(out, extract);

                    #ifdef __i386__
                    *(uint32_t*)(dest + i/2 + 0) = _mm_cvtsi128_si32(out);
                    *(uint16_t*)(dest + i/2 + 4) = _mm_extract_epi16(out, 2);
                    *(uint16_t*)(dest + i/2 + 6) = _mm_extract_epi16(out, 3);
                    #else
                    *(uint64_t*)(dest + i/2) = _mm_cvtsi128_si64(out);
                    #endif
                }
            }
            #endif
            for (; i+2 <= size; i+=2) {
                dest[i/2] = (src[i + 0] & 0x0F) << 4 |
                            (src[i + 1] & 0x0F) << 0;
            }
            break;
        }
        case 8: {
            SLP_FREE(new_buffer);
            return true;
            break;
        }
        case 16: {
            const uint16_t random_value_for_edian_test = 1;
            if (!(*(uint8_t*)(&random_value_for_edian_test))) {
                SLP_FREE(new_buffer);
                return true;
            }

            #ifdef __AVX2__
            for (; i + 32 <= size; i += 32) {
                const __m256i in = _mm256_loadu_si256((const __m256i*)(src + i));
                const __m256i out = _mm256_or_si256(_mm256_slli_epi16(in, 8), _mm256_srli_epi16(in, 8));
                _mm256_storeu_si256((__m256i*)(dest + i), out);
            }
            #endif
            #ifdef __SSE2__
            for (; i + 16 <= size; i += 16) {
                const __m128i in = _mm_loadu_si128((const __m128i*)(src + i));
                const __m128i out = _mm_or_si128(_mm_slli_epi16(in, 8), _mm_srli_epi16(in, 8));
                _mm_storeu_si128((__m128i*)(dest + i), out);
            }
            #endif
            for (; i + 2 <= size; i += 2) {
                dest[i + 0] = src[i + 1];
                dest[i + 1] = src[i + 0];
            }
            break;
        }
        default: {
            SLP_FREE(new_buffer);
            return false;
        }
    }

    SLP_FREE(image->buffer);
    image->buffer = new_buffer;
    image->image_size = new_size;
    image->allocated_size = SLP_ALIGN_SIZE(new_size);

    return true;
}



bool slp_image_convert_G8_to_RGBA32(slp_image_t* image) {
    if (image->channels != 1 || image->bit_depth != 8) return false;

    const size_t size = (size_t)image->width * image->height * image->channels * (1 + (image->bit_depth == 16));

    uint8_t *new_buffer = (uint8_t*)SLP_ALIGNED_ALLOC(size * 4);
    if (new_buffer == NULL) {
        return false;
    }

    uint8_t *src = image->buffer;
    uint8_t *dest = new_buffer;

    size_t i = 0;
    #ifdef __SSE2__
    const __m128i FF = _mm_set1_epi8(-1);
    for (; i + 16 <= size; i+=16) {
        const __m128i in = _mm_loadu_si128((const __m128i*)(src + i));

        const __m128i inin1 = _mm_unpacklo_epi8(in, in);
        const __m128i inin2 = _mm_unpackhi_epi8(in, in);

        const __m128i inFF1 = _mm_unpacklo_epi8(in, FF);
        const __m128i inFF2 = _mm_unpackhi_epi8(in, FF);

        const __m128i inininFF_lo1 = _mm_unpacklo_epi16(inin1, inFF1);
        const __m128i inininFF_hi1 = _mm_unpackhi_epi16(inin1, inFF1);

        const __m128i inininFF_lo2 = _mm_unpacklo_epi16(inin2, inFF2);
        const __m128i inininFF_hi2 = _mm_unpackhi_epi16(inin2, inFF2);

        _mm_storeu_si128((__m128i*)(dest + i + 16 * 0), inininFF_lo1);
        _mm_storeu_si128((__m128i*)(dest + i + 16 * 1), inininFF_hi1);
        _mm_storeu_si128((__m128i*)(dest + i + 16 * 2), inininFF_lo2);
        _mm_storeu_si128((__m128i*)(dest + i + 16 * 3), inininFF_hi2);
    }
    #endif
    for (; i < size; i++) {
        dest[i] = src[i];//R
        dest[i] = src[i];//G
        dest[i] = src[i];//B
        dest[i+1] = 0xFF;//A
    }

    SLP_FREE(image->buffer);
    image->buffer = new_buffer;
    image->channels = 4;
    image->image_size = size * 4;
    image->allocated_size = SLP_ALIGN_SIZE(size * 4);

    return true;
}



bool slp_image_convert_GA16_to_RGBA32(slp_image_t* image) {
    if (image->channels != 2 || image->bit_depth != 8) return false;

    const size_t src_size_in_element = (size_t)image->width * image->height * 2;// size in element
    const size_t new_size = (size_t)image->width * image->height * 8;
    uint8_t *new_buffer = (uint8_t*)SLP_ALIGNED_ALLOC(new_size);
    if (new_buffer == NULL) return false;

    uint16_t *src = (uint16_t*)image->buffer;
    uint16_t *dest = (uint16_t*)new_buffer;

    size_t i = 0;
    #ifdef __SSE2__
    const __m128i FF00 = _mm_set1_epi16(0xFF);
    for (; i + 8 <= src_size_in_element; i+=8) {
        const __m128i inAA = _mm_loadu_si128((const __m128i*)(src + i));

        const __m128i in00 = _mm_and_si128(inAA, FF00);
        const __m128i _00in = _mm_srli_epi16(inAA, 8);

        const __m128i inin = _mm_or_si128(in00, _00in);

        const __m128i inininAA_lo = _mm_unpacklo_epi16(inin, inAA);
        const __m128i inininAA_hi = _mm_unpackhi_epi16(inin, inAA);

        _mm_storeu_si128((__m128i*)(dest + i + 8 * 0), inininAA_lo);
        _mm_storeu_si128((__m128i*)(dest + i + 8 * 1), inininAA_hi);
    }
    #endif
    for (; i < src_size_in_element; i+=2) {
        dest[i] = src[i];//R
        dest[i] = src[i];//G
        dest[i] = src[i];//B
        dest[i+1] = src[i+1];//A
    }

    SLP_FREE(image->buffer);
    image->buffer = new_buffer;
    image->channels = 4;
    image->image_size = new_size;
    image->allocated_size = SLP_ALIGN_SIZE(new_size);

    return true;
}



bool slp_image_convert_G16_to_RGBA64(slp_image_t* image) {
    if (image->channels != 1 || image->bit_depth != 16) return false;

    const size_t src_size_in_element = (size_t)image->width * image->height * 2;
    const size_t new_size = (size_t)image->width * image->height * 8;

    uint8_t *new_buffer = (uint8_t*)SLP_ALIGNED_ALLOC(new_size);
    if (new_buffer == NULL) {
        return false;
    }

    uint16_t *src = (uint16_t*)image->buffer;
    uint16_t *dest = (uint16_t*)new_buffer;

    size_t i = 0;
    #ifdef __SSE2__
    const __m128i FF = _mm_set1_epi16(-1);
    for (; i + 8 <= src_size_in_element; i+=8) {
        const __m128i in = _mm_loadu_si128((const __m128i*)(src + i));

        const __m128i inin1 = _mm_unpacklo_epi16(in, in);
        const __m128i inin2 = _mm_unpackhi_epi16(in, in);

        const __m128i inFF1 = _mm_unpacklo_epi16(in, FF);
        const __m128i inFF2 = _mm_unpackhi_epi16(in, FF);

        const __m128i inininFF_lo1 = _mm_unpacklo_epi32(inin1, inFF1);
        const __m128i inininFF_lo2 = _mm_unpacklo_epi32(inin2, inFF2);

        const __m128i inininFF_hi1 = _mm_unpackhi_epi32(inin1, inFF1);
        const __m128i inininFF_hi2 = _mm_unpackhi_epi32(inin2, inFF2);

        _mm_storeu_si128((__m128i*)(dest + i + 8 * 0), inininFF_lo1);
        _mm_storeu_si128((__m128i*)(dest + i + 8 * 1), inininFF_hi1);
        _mm_storeu_si128((__m128i*)(dest + i + 8 * 2), inininFF_lo2);
        _mm_storeu_si128((__m128i*)(dest + i + 8 * 3), inininFF_hi2);
    }
    #endif
    for (; i < src_size_in_element; i++) {
        dest[i + 0] = src[i];
        dest[i + 1] = src[i];
        dest[i + 2] = src[i];
        dest[i + 4] = 0xFFFF;
    }

    SLP_FREE(image->buffer);
    image->buffer = new_buffer;
    image->channels = 4;
    image->image_size = new_size;
    image->allocated_size = SLP_ALIGN_SIZE(new_size);

    return true;
}



bool slp_image_convert_GA32_to_RGBA64(slp_image_t* image) {
    if (image->channels != 2 || image->bit_depth != 16) return false;

    const size_t src_size_in_element = (size_t)image->width * image->height * 4;
    const size_t new_size = (size_t)image->width * image->height * 8;

    uint8_t *new_buffer = (uint8_t*)SLP_ALIGNED_ALLOC(new_size);
    if (new_buffer == NULL) {
        return false;
    }

    uint16_t *src = (uint16_t*)image->buffer;
    uint16_t *dest = (uint16_t*)new_buffer;

    size_t i = 0;
    #ifdef __SSE2__
    const __m128i FF00 = _mm_set1_epi32(0xFFFF);
    for (; i + 8 <= src_size_in_element; i+=8) {
        const __m128i inAA = _mm_loadu_si128((const __m128i*)(src + i));

        const __m128i in00 = _mm_and_si128(inAA, FF00);
        const __m128i _00in = _mm_srli_epi16(inAA, 16);

        const __m128i inin = _mm_or_si128(in00, _00in);

        const __m128i inininAA_lo = _mm_unpacklo_epi32(inin, inAA);
        const __m128i inininAA_hi = _mm_unpackhi_epi32(inin, inAA);

        _mm_storeu_si128((__m128i*)(dest + i + 8 * 0), inininAA_lo);
        _mm_storeu_si128((__m128i*)(dest + i + 8 * 1), inininAA_hi);
    }
    #endif
    for (; i < src_size_in_element; i+=2) {
        dest[i + 0] = src[i];
        dest[i + 1] = src[i];
        dest[i + 2] = src[i];
        dest[i + 4] = src[i + 1];
    }

    SLP_FREE(image->buffer);
    image->buffer = new_buffer;
    image->channels = 4;
    image->image_size = new_size;
    image->allocated_size = SLP_ALIGN_SIZE(new_size);

    return true;
}



slp_image_t slp_image_copy(slp_image_t image) {
    uint8_t* new_buffer = (uint8_t*)SLP_ALIGNED_ALLOC(image.allocated_size);
    if (new_buffer == NULL) {
        image.buffer = NULL;
        return image;
    }
    SLP_MEMCPY(new_buffer, image.buffer, image.allocated_size);
    image.buffer = new_buffer;
    return image;
}
