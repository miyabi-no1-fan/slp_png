#define SLP_IMAGE_TRANSFROM_RELEASE 0  // not release yet, we didn't have any test for it at all

#if SLP_IMAGE_TRANSFROM_RELEASE

#pragma once
#include <slp_png.h>
#include <stdbool.h>

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