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

#define PNG_SIGNATURE 0x89504E470D0A1A0Aull
#define IHDR 0x49484452u

#define Err(err) do { return err; } while(0)

extern int decode(slp_png_io png, slp_image_t* restrict image, const int color_type);
static int get_channels(const int color_type, const int bit_depth);
static int read_ihdr(slp_image_t* image, const slp_png_io png, int* color_type);

// default limit as 12k resolution
_Thread_local uint32_t with_limit = 12288;
_Thread_local uint32_t height_limit = 6480;

bool default_read(void* dst, void* src, size_t n) {
    return fread(dst, 1, n, src) == n;
}

bool default_seek(void* src, uint32_t n) {
    return fseek(src, n, SEEK_CUR) == 0;
}

// read png from a file
int slp_png_read(slp_image_t* image, const slp_png_io* png_) {
    if (image == NULL || png_ == NULL) return NULL_ARGS;
    if (png_->buf == NULL) return IO_ERR;

    slp_png_io png = *png_;
    if (png_->read == NULL) png.read = &default_read;
    if (png_->seek == NULL) png.seek = &default_seek;
    png.write = NULL;  // unused so we set it to NULL

    int color_type;
    int ret = read_ihdr(image, png, &color_type);
    if (ret != 0) Err(ret);

    image->pixels = (uint8_t*)SLP_CALLOC(image->image_size);
    if (image->pixels == NULL) Err(ALLOC_ERR);

    ret = decode(png, image, color_type);
    if (ret != 0) {
        SLP_FREE(image->pixels, image->image_size);
        image->pixels = NULL;
        Err(ret);
    }
    image->bit_depth = (color_type == 3) ? 8 : image->bit_depth;

    return 0;
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

static int read_ihdr(slp_image_t* image, const slp_png_io png, int* color_type) {
    uint8_t ihdr[33];

    if (!png.read(ihdr, png.buf, 33))
        Err(IO_ERR);

    uint32_t crc_ = crc32(0, ihdr + 12, 4);
    crc_ = crc32(crc_, ihdr + 16, 13);

    if (big_edian_u64(ihdr) != PNG_SIGNATURE ||
        big_edian_u32(ihdr + 8) != 13 ||
        big_edian_u32(ihdr + 12) != IHDR ||
        big_edian_u32(ihdr + 29) != crc_)
    {
        Err(INVALID_PNG);
    }

    image->width = big_edian_u32(ihdr + 16);
    image->height = big_edian_u32(ihdr + 20);
    image->bit_depth = ihdr[24];
    *color_type = ihdr[25];
    image->channels = get_channels(*color_type, image->bit_depth);

    const int compression_method = ihdr[26];
    const int filter_method = ihdr[27];
    const int interlace_method = ihdr[28];

    if (image->width > with_limit || image->height > height_limit)
        Err(INVALID_PNG);

    if (compression_method != 0 || filter_method != 0 || interlace_method != 0 || (int)image->channels == -1)
        Err(INVALID_PNG);

    image->image_size = image->height * div_ceil((size_t)image->width * image->channels * ((*color_type == 3) ? 8 : image->bit_depth), 8);

    return 0;
}
