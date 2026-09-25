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

slp_image_t slp_image_copy(slp_image_t image) {
    uint8_t* new_buffer = (uint8_t*)SLP_MALLOC(image.size);
    if (new_buffer == NULL) {
        image.pixels = NULL;
        return image;
    }
    SLP_MEMCPY(new_buffer, image.pixels, image.size);
    image.pixels = new_buffer;
    return image;
}

slp_image_t slp_image_crop(const slp_image_t* image, const uint32_t new_width, const uint32_t new_height, const uint32_t offset_width, const uint32_t offset_height) {
    const size_t sizeof_1pixel = (size_t)image->channels * (1 + (image->bit_depth == 16));
    const size_t src_stride = (size_t)image->width * sizeof_1pixel;
    const size_t dest_stride = (size_t)new_width * sizeof_1pixel;
    const size_t new_size = dest_stride * new_height;

    slp_image_t new_image = *image;
    new_image.width = new_width;
    new_image.height = new_height;
    new_image.size = new_size;
    new_image.pixels = (uint8_t*)SLP_MALLOC(new_size);
    if (new_image.pixels == NULL) return new_image;

    uint8_t* src = image->pixels + offset_height * src_stride + offset_width * sizeof_1pixel;
    uint8_t* dst = new_image.pixels;

    for (size_t i = 0; i < new_height; i++)
        SLP_MEMCPY(dst + i * dest_stride, src + i * src_stride, dest_stride);

    return new_image;
}
