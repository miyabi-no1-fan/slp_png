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
#define SLP_IMAGE_TRANSFROM_RELEASE 1  // not release yet, we didn't have any test for it at all

#if SLP_IMAGE_TRANSFROM_RELEASE

#pragma once
#include <stdbool.h>

#include "slp_png.h"

#ifdef __cplusplus
extern "C" {
#endif

void slp_image_convert_to_8bit(slp_image_t* image);
bool slp_image_convert_to_16bit(slp_image_t* image);
void slp_image_bswap16(slp_image_t* image);

bool slp_image_crop(slp_image_t* image, const uint32_t new_width, const uint32_t new_height, const uint32_t offset_width, const uint32_t offset_height);

void slp_image_fill(uint8_t* buffer, const size_t buffer_size, const uint8_t* pixel, const uint8_t pixel_size);
slp_image_t slp_image_copy(const slp_image_t image);

bool slp_image_linear_transform(slp_image_t* restrict image, const double* restrict A, const uint8_t* restrict background);  // A[4] = a00, a01, a10, a11

bool slp_image_pack(slp_image_t* image);
bool slp_image_unpack(slp_image_t* image);

#ifdef __cplusplus
}
#endif

#endif