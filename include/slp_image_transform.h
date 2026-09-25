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
#pragma once
#include <stdbool.h>

#include "slp_png.h"

#ifdef __cplusplus
extern "C" {
#endif

// return 1 if unknown bit_depth
int slp_image_convert_to_8bit(slp_image_t* image);

// the returned image.pixel is NULL if allocation failed or unknown bit depth
slp_image_t slp_image_convert_to_16bit(const slp_image_t* image);

// return with image.pixels = NULL if allocation failed, no bound checks provided here, you should do it urself
slp_image_t slp_image_crop(const slp_image_t* image, const uint32_t new_width, const uint32_t new_height, const uint32_t offset_width, const uint32_t offset_height);

// return with image.pixels = NULL if allocation failed
slp_image_t slp_image_copy(const slp_image_t image);

// return with image.pixels = NULL if allocation failed or new image size is 0
slp_image_t slp_image_linear_transform(const slp_image_t* image, const double A[2][2]);

// return 1 if allocation failed or unknown bit depth
int slp_image_pack(slp_image_t* image);

// return 1 if allocation failed or unknown bit depth
int slp_image_unpack(slp_image_t* image);

#ifdef __cplusplus
}
#endif