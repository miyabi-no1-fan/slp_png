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
#include <stdbool.h>
#include <stdio.h>
#include <zlib.h>

#define SLP_PNG_MACROS
#include "slp_png.h"

#ifndef SLP_DEBUG
#define SLP_DEBUG 0
#endif

#if SLP_DEBUG
    #define Err(err) image.bit_depth = err
#else
    #define Err(err) (void)err
#endif

#define PNG_SIGNATURE 0x89504E470D0A1A0Aull
#define IHDR 0x49484452u

extern int decode(slp_image_t* restrict image, FILE* restrict file, const int color_type);

static int get_channels(const int color_type, const int bit_depth);

// read png from a file
slp_image_t slp_png_read(const char* path) {
    slp_image_t image = {};

    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        Err(FILE_ERR);
        image.pixels = NULL;
        return image;
    }

    uint8_t ihdr[33];

    if (fread(ihdr, 1, 33, file) != 33) {
        fclose(file);
        Err(FILE_ERR);
        image.pixels = NULL;
        return image;
    }

    uint32_t crc_ = crc32(0, ihdr + 12, 4);
    crc_ = crc32(crc_, ihdr + 16, 13);

    if (big_edian_u64(ihdr) != PNG_SIGNATURE ||
        big_edian_u32(ihdr + 8) != 13 ||
        big_edian_u32(ihdr + 12) != IHDR ||
        big_edian_u32(ihdr + 29) != crc_)
    {
        fclose(file);
        Err(INVALID_FILE);
        image.pixels = NULL;
        return image;
    }

    const uint32_t width = image.width = big_edian_u32(ihdr + 16);
    const uint32_t height = image.height = big_edian_u32(ihdr + 20);
    const int bit_depth = image.bit_depth = ihdr[24];
    const int color_type = ihdr[25];
    const int channels = image.channels = get_channels(color_type, bit_depth);
    const int compression_method = ihdr[26];
    const int filter_method = ihdr[27];
    const int interlace_method = ihdr[28];

    if (compression_method != 0 || filter_method != 0 || interlace_method != 0 || channels == -1) {
        fclose(file);
        Err(INVALID_FILE);
        image.pixels = NULL;
        return image;
    }

    const size_t image_size = image.image_size = height * div_ceil((size_t)width * channels * ((color_type == 3) ? 8 : bit_depth), 8);

    image.pixels = (uint8_t*)SLP_MALLOC(image_size);
    if (image.pixels == NULL) {
        fclose(file);
        Err(ALLOC_ERR);
        image.pixels = NULL;
        return image;
    }

    int ret = decode(&image, file, color_type);
    if (ret != 0) {
        fclose(file);
        SLP_FREE(image.pixels);
        Err(ret);
        image.pixels = NULL;
        return image;
    }
    image.bit_depth = (color_type == 3) ? 8 : image.bit_depth;

    fclose(file);
    return image;
}

static inline int get_channels(const int color_type, const int bit_depth) {
    switch (color_type) {
        case 0: {
            switch (bit_depth) {
                case 1: break;
                case 2: break;
                case 4: break;
                case 8: break;
                case 16: break;
                default: return 0;
            }
            return 1;
        }
        case 2: {
            switch (bit_depth) {
                case 8: break;
                case 16: break;
                default: return 0;
            }
            return 3;
        }
        case 3: {
            switch (bit_depth) {
                case 1: break;
                case 2: break;
                case 4: break;
                case 8: break;
                default: return 0;
            }
            return 4;
        }
        case 4: {
            switch (bit_depth) {
                case 8: break;
                case 16: break;
                default: return 0;
            }
            return 2;
        }
        case 6: {
            switch (bit_depth) {
                case 8: break;
                case 16: break;
                default: return 0;
            }
            return 4;
        }
    }
    return -1;
}
