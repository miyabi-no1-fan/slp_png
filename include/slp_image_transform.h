#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <slp_image.h>

bool slp_image_convert_to_8bit(slp_image_t* image);
bool slp_image_convert_to_16bit(slp_image_t* image);
bool slp_image_crop(slp_image_t* image, const uint32_t new_width, const uint32_t new_height, const uint32_t offset_width, const uint32_t offset_height);
void slp_image_fill(uint8_t* buffer, const size_t buffer_size, const uint8_t* pixel, const uint8_t pixel_size);
bool slp_image_linear_transform(slp_image_t* restrict image, const double* restrict A, const uint8_t* restrict background); // A[4] = a00, a01, a10, a11
/*
bool slp_image_convert_G8_to_RGBA32(slp_image_t* image);
bool slp_image_convert_GA16_to_RGBA32(slp_image_t* image);
bool slp_image_convert_G16_to_RGBA64(slp_image_t* image);
bool slp_image_convert_GA32_to_RGBA64(slp_image_t* image);
*/
slp_image_t slp_image_copy(const slp_image_t image);
bool slp_image_unformat(slp_image_t* image);
bool slp_image_format(slp_image_t* image);
