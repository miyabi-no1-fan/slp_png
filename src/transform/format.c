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

#define SLP_PNG_MACROS
#include "slp_image_transform.h"

slp_image_t slp_image_convert_G8_to_RGBA8(slp_image_t* image) {
    assert(image->channels == 1);
    assert(image->bit_depth == 8);
    slp_image_t new_image = *image;
    new_image.channels = 4;
    new_image.size = new_image.width * new_image.height * new_image.channels;
    new_image.pixels = (uint8_t*)SLP_MALLOC(new_image.size);
    if (new_image.pixels == NULL) return new_image;
    for (size_t i = 0; i < new_image.width * new_image.height; i++) {
        uint8_t* src = &image->pixels[i * image->channels];
        uint8_t* dst = &new_image.pixels[i * new_image.channels];
        dst[i + 0] = src[i];
        dst[i + 1] = src[i];
        dst[i + 2] = src[i];
        dst[i + 3] = 0xFF;
    }
    return new_image;
}

slp_image_t slp_image_convert_GA8_to_RGBA8(slp_image_t* image) {
    assert(image->channels == 2);
    assert(image->bit_depth == 8);
    slp_image_t new_image = *image;
    new_image.channels = 4;
    new_image.size = new_image.width * new_image.height * new_image.channels;
    new_image.pixels = (uint8_t*)SLP_MALLOC(new_image.size);
    if (new_image.pixels == NULL) return new_image;
    for (size_t i = 0; i < new_image.width * new_image.height; i++) {
        uint8_t* src = &image->pixels[i * image->channels];
        uint8_t* dst = &new_image.pixels[i * new_image.channels];
        dst[i + 0] = src[i + 0];
        dst[i + 1] = src[i + 0];
        dst[i + 2] = src[i + 0];
        dst[i + 3] = src[i + 1];
    }
    return new_image;
}

slp_image_t slp_image_convert_RGB8_to_RGBA8(slp_image_t* image) {
    assert(image->channels == 3);
    assert(image->bit_depth == 8);
    slp_image_t new_image = *image;
    new_image.channels = 4;
    new_image.size = new_image.width * new_image.height * new_image.channels;
    new_image.pixels = (uint8_t*)SLP_MALLOC(new_image.size);
    if (new_image.pixels == NULL) return new_image;
    for (size_t i = 0; i < new_image.width * new_image.height; i++) {
        uint8_t* src = &image->pixels[i * image->channels];
        uint8_t* dst = &new_image.pixels[i * new_image.channels];
        dst[i + 0] = src[i + 0];
        dst[i + 1] = src[i + 1];
        dst[i + 2] = src[i + 2];
        dst[i + 3] = 0xFF;
    }
    return new_image;
}
