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
#include "slp_image_transform.h"
#if SLP_IMAGE_TRANSFROM_RELEASE

#include <math.h>
#ifdef _WIN32
    #include <windows.h>
#endif
#if defined(__unix__) || defined(__APPLE__)
    #include <unistd.h>
#endif
#if defined(__i386__) || defined(__x86_64__)
    #include <immintrin.h>
#endif

void slp_image_fill(uint8_t* buffer, const size_t buffer_size, const uint8_t* pixel, const uint8_t pixel_size) {
    SLP_MEMCPY(buffer, pixel, pixel_size);
    size_t i = pixel_size;
    for (; i * 2 <= buffer_size; i *= 2)
        SLP_MEMCPY(buffer + i, buffer, i);
    SLP_MEMCPY(buffer + i, buffer, buffer_size - i);
}

slp_image_t slp_image_copy(slp_image_t image) {
    uint8_t* new_buffer = (uint8_t*)SLP_MALLOC(image.image_size);
    if (new_buffer == NULL) {
        image.pixels = NULL;
        return image;
    }
    SLP_MEMCPY(new_buffer, image.pixels, image.image_size);
    image.pixels = new_buffer;
    return image;
}

bool image_crop(slp_image_t* image, const uint32_t new_width, const uint32_t new_height, const uint32_t offset_width, const uint32_t offset_height) {
    if (offset_width + new_width > image->width || offset_height + new_height > image->height) return false;

    const size_t sizeof_1pixel = (size_t)image->channels * (1 + (image->bit_depth == 16));
    const size_t src_stride = (size_t)image->width * sizeof_1pixel;
    const size_t dest_stride = (size_t)new_width * sizeof_1pixel;

    const size_t new_size = dest_stride * new_height;
    uint8_t* new_buffer = (uint8_t*)SLP_MALLOC(new_size);
    if (new_buffer == NULL) return false;

    uint8_t* src = image->pixels + offset_height * src_stride + offset_width * sizeof_1pixel;
    uint8_t* dst = new_buffer;

    for (size_t i = 0; i < new_height; i++) {
        SLP_MEMCPY(dst + i * dest_stride, src + i * src_stride, dest_stride);
    }

    slp_image_destroy(image);

    image->pixels = new_buffer;
    image->width = new_width;
    image->height = new_height;
    image->image_size = new_size;

    return true;
}

void slp_image_bswap16(slp_image_t* image) {
    const size_t size = image->image_size / 2;
    uint16_t* buf = (uint16_t*)image->pixels;

    size_t i = 0;
    #ifdef __AVX2__
    for (; i + 16 <= size; i += 16) {
        const __m256i in = _mm256_loadu_si256((const __m256i*)(buf + i));
        const __m256i out = _mm256_or_si256(_mm256_slli_epi16(in, 8), _mm256_srli_epi16(in, 8));
        _mm256_storeu_si256((__m256i*)(buf + i), out);
    }
    #endif
    #ifdef __SSE2__
    for (; i + 8 <= size; i += 8) {
        const __m128i in = _mm_loadu_si128((const __m128i*)(buf + i));
        const __m128i out = _mm_or_si128(_mm_slli_epi16(in, 8), _mm_srli_epi16(in, 8));
        _mm_storeu_si128((__m128i*)(buf + i), out);
    }
    #endif

    for (; i < size; i++) buf[i] = buf[i] >> 8 | buf[i] << 8;
}

bool slp_image_linear_transform(slp_image_t* restrict image, const double* restrict A, const uint8_t* restrict background) {
    const double detA = A[0] * A[3] - A[1] * A[2];
    if (detA == 0) {
        SLP_MEMSET(image->pixels, 0, image->image_size);
        image->height = 0;
        image->width = 0;
        return true;
    }
    const double inverseA[4] = { A[3] / detA, -A[1] / detA, -A[2] / detA, A[0] / detA };

    // calculate new size, through the 4 corners
    // 0, (H-1) -> -(H-1)/2, (H-1)/2
    // 0, (W-1) -> -(W-1)/2, (W-1)/2
    double half_height = ((double)image->height - 1) / 2;  // y
    double half_width = ((double)image->width - 1) / 2;    // x

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

    const size_t pixel_size = image->channels * (1 + (image->bit_depth == 16));  // sizeof 1 pixel
    const size_t src_stride = image->width * pixel_size;                         // sizeof 1 src scanline
    const size_t dst_stride = new_width * pixel_size;                            // sizeof 1 dst scanline
    const size_t new_size = (size_t)new_width * new_height * pixel_size;

    uint8_t* new_buffer = (uint8_t*)SLP_MALLOC(new_size);
    if (new_buffer == NULL) return false;

    slp_image_fill(new_buffer, new_size, background, pixel_size);
    uint8_t* src = image->pixels;

    // these are some pre-compute for the `incremental stepping`
    // I just optimize the variables out of the loop and got this
    double c1 = vmax * inverseA[1] + half_width;
    double c2 = vmax * inverseA[3] - half_height;
    double X = umin * inverseA[0] + c1;
    double Y = -umin * inverseA[2] - c2;

    for (size_t i = 0; i < new_height; i++) {
        uint8_t* dest = new_buffer + i * dst_stride;

        double x0, x2, y0, y2;

        if (inverseA[0] > 0) {
            x0 = -X / inverseA[0];
            x2 = (image->width - X - 1) / inverseA[0];
        }
        else if (inverseA[0] < 0) {
            x0 = (image->width - X - 1) / inverseA[0];
            x2 = -X / inverseA[0];
        }
        else {
            x0 = 0;
            x2 = new_width;
        }

        if (inverseA[2] > 0) {
            y0 = (Y - image->height + 1) / inverseA[2];
            y2 = Y / inverseA[2];
        }
        else if (inverseA[2] < 0) {
            y0 = Y / inverseA[2];
            y2 = (Y - image->height + 1) / inverseA[2];
        }
        else {
            y0 = 0;
            y2 = new_width;
        }

        ssize_t start = ceil(fmax(x0, y0));
        ssize_t end = fmin(x2, y2);

        dest += start * pixel_size;

        ssize_t mid = start;

        double x1 = mid * inverseA[0] + X;
        double y1 = -mid * inverseA[2] + Y;
        
        for (; mid < end; mid++) { // data
            SLP_MEMCPY(dest, src + ((ssize_t)y1) * src_stride + ((ssize_t)x1) * pixel_size, pixel_size);
            dest += pixel_size;
            x1 += inverseA[0];
            y1 += -inverseA[2];
        }

        c1 -= inverseA[1];
        c2 -= inverseA[3];

        X -= inverseA[1];
        Y += inverseA[3];
    }

    slp_image_destroy(image);

    image->pixels = new_buffer;
    image->width = new_width;
    image->height = new_height;
    image->image_size = new_size;

    return true;
}

#endif