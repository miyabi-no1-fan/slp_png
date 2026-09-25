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
#include <stddef.h>
#include <stdint.h>

#define SLP_PNG_MACROS
#include "slp_image_transform.h"

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define clamp(v, min_v, max_v) (max(min((v), (max_v)), (min_v)))

slp_image_t slp_image_linear_transform(const slp_image_t* image, const double A[2][2]) {
    // calculate new size, through the 4 corners
    // 0, (H-1) -> -(H-1)/2, (H-1)/2
    // 0, (W-1) -> -(W-1)/2, (W-1)/2
    const double half_height = ((double)image->height - 1.0) / 2.0;  // y
    const double half_width = ((double)image->width - 1.0) / 2.0;    // x

    // 1    2
    // 3    4
    const double x1 = -half_width * A[0][0] + half_height * A[0][1];
    const double x2 = half_width * A[0][0] + half_height * A[0][1];
    const double x3 = -half_width * A[0][0] - half_height * A[0][1];
    const double x4 = half_width * A[0][0] - half_height * A[0][1];

    const double y1 = -half_width * A[1][0] + half_height * A[1][1];
    const double y2 = half_width * A[1][0] + half_height * A[1][1];
    const double y3 = -half_width * A[1][0] - half_height * A[1][1];
    const double y4 = half_width * A[1][0] - half_height * A[1][1];

    const double xmax = max(max(max(x1, x2), x3), x4);
    const double xmin = min(min(min(x1, x2), x3), x4);
    const double ymax = max(max(max(y1, y2), y3), y4);
    const double ymin = min(min(min(y1, y2), y3), y4);

    const uint32_t new_width = (uint32_t)(xmax - xmin + 1);
    const uint32_t new_height = (uint32_t)(ymax - ymin + 1);

    const size_t pixel_size = image->channels * (1 + (image->bit_depth == 16));  // sizeof 1 pixel
    const size_t src_stride = image->width * pixel_size;                         // sizeof 1 src scanline
    const size_t dst_stride = new_width * pixel_size;                            // sizeof 1 dst scanline
    const size_t new_size = (size_t)new_width * (size_t)new_height * (size_t)pixel_size;

    slp_image_t new_image = *image;
    new_image.pixels = NULL;
    new_image.height = new_height;
    new_image.width = new_width;
    new_image.size = new_size;

    if (new_size == 0) return new_image;

    new_image.pixels = (uint8_t*)SLP_CALLOC(new_size);
    if (new_image.pixels == NULL) return new_image;

    // We're using inverse mapping and incremental stepping here
    // Fixed-point numbers are 32x32 -- don't over complicate it, it's just an i64 multiply by 2^32

    const int64_t P = INT64_C(1) << 32;  // 2^32
    const double detA = A[0][0] * A[1][1] - A[0][1] * A[1][0];
    const int64_t inverseA[2][2] = {
        { A[1][1] * (double)P / detA, -A[0][1] * (double)P / detA },
        { -A[1][0] * (double)P / detA, A[0][0] * (double)P / detA },
    };

    // Starting from our top-left corner
    // This is simply inverse_mat * [xmin, ymax]
    const int64_t base_src_x = (int64_t)xmin * inverseA[0][0] + (int64_t)ymax * inverseA[0][1] + (int64_t)(half_width * P);
    const int64_t base_src_y = -(int64_t)xmin * inverseA[1][0] - (int64_t)ymax * inverseA[1][1] + (int64_t)(half_height * P);

    // NOTICE: y is inverted vertically
    // The Euclidean space assume y going **upwards**,
    // whereas our image has y going **downwards**
    // -- you'll notice that y consistently having the opposite sign to x in the code.

    for (size_t i = 0; i < new_height; i++) {
        int64_t src_x = base_src_x - i * inverseA[0][1];
        int64_t src_y = base_src_y + i * inverseA[1][1];

        // This is just a result from solving equations from the brute-force loop
        int64_t start_x, end_x, start_y, end_y;
        if (inverseA[0][0] > 0) {
            start_x = -src_x / inverseA[0][0];
            end_x = (((int64_t)image->width << 32) - src_x) / inverseA[0][0];
        } else if (inverseA[0][0] < 0) {
            end_x = -src_x / inverseA[0][0];
            start_x = (((int64_t)image->width << 32) - src_x) / inverseA[0][0];
        } else {
            if (src_x >= 0 && src_x < ((int64_t)image->width << 32)) {
                start_x = 0;
                end_x = new_width;
            } else {
                start_x = new_width;
                end_x = 0;
            }
        }
        if (inverseA[1][0] > 0) {
            start_y = (src_y - ((int64_t)image->height << 32)) / inverseA[1][0];
            end_y = src_y / inverseA[1][0];
        } else if (inverseA[1][0] < 0) {
            end_y = (src_y - ((int64_t)image->height << 32)) / inverseA[1][0];
            start_y = src_y / inverseA[1][0];
        } else {
            if (src_y >= 0 && src_y < ((int64_t)image->height << 32)) {
                start_y = 0;
                end_y = new_width;
            } else {
                start_y = new_width;
                end_y = 0;
            }
        }

        // `start` = first index where both x and y are valid
        // `end` = first index where not both x and y are valid after start
        int64_t start = clamp(max(start_x, start_y), 0, (int64_t)new_width);
        int64_t end = clamp(max(end_x, end_y), 0, (int64_t)new_width);

        // `start` and `end` might be off by 1
        // calculations like `src_y / inverse_mat[1][0]`,
        // do a `floor` on `start` -- `start` supposed to be `ceil` instead
        // So we should run the brute-force loop
        // this would ensure that `start` and `end` are valid
        while (start < end) {
            int64_t x = start * inverseA[0][0] + src_x;
            int64_t y = src_y - start * inverseA[1][0];
            if (x >= 0 && x < ((int64_t)image->width << 32) && y >= 0 && y < ((int64_t)image->height << 32)) break;
            start += 1;
        }
        while (start < end) {
            int64_t x = (end - 1) * inverseA[0][0] + src_x;
            int64_t y = src_y - (end - 1) * inverseA[1][0];
            if (x >= 0 && x < ((int64_t)image->width << 32) && y >= 0 && y < ((int64_t)image->height << 32)) break;
            end -= 1;
        }

        uint8_t* dst = new_image.pixels + i * dst_stride + start * pixel_size;
        int64_t x = start * inverseA[0][0] + src_x;
        int64_t y = src_y - start * inverseA[1][0];

        for (int64_t _ = start; _ < end; _++) {
            SLP_MEMCPY(dst, image->pixels + (y >> 32) * src_stride + (x >> 32) * pixel_size, pixel_size);
            x += inverseA[0][0];
            y -= inverseA[1][0];
            dst += pixel_size;
        }
    }

    return new_image;
}
