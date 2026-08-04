#pragma once
#include <slp_image.h>

#ifdef __cplusplus
extern "C" {
#endif

slp_image_t slp_png_read(const char* path);
int slp_png_write(slp_image_t image, const char* path);
void slp_image_destroy(slp_image_t* image);

#ifdef __cplusplus
}
#endif