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

#include "slp_png.h"
#include "slp_utils.h"

// helpers
static uint8_t get_color_type(const uint8_t channels);
extern int encode(const slp_image_t* restrict image, slp_png_io png);

static bool default_write(void* src, void* dst, size_t n) {
    return fwrite(src, 1, n, dst) == n;
}

int slp_png_write(const slp_image_t* image, const slp_png_io* png_) {
    if (image == NULL || png_ == NULL) return NULL_ARGS;
    if (image->pixels == NULL ||
        image->height == 0 ||
        image->width == 0 ||
        image->channels == 0 ||
        image->size != image->height * div_ceil((size_t)image->width * (size_t)image->channels * (size_t)image->bit_depth, 8)) {
        return INVALID_PNG;
    }
    switch (image->bit_depth) {
        case 1: break;
        case 2: break;
        case 4: break;
        case 8: break;
        case 16: break;
        default: return INVALID_PNG;
    }
    if (png_->buf == NULL) return IO_ERR;

    slp_png_io png = *png_;
    if (png_->write == NULL) png.write = &default_write;

    const uint64_t PNG_SIGNATURE = to_be_u64(0x89504E470D0A1A0Aull);

    // use to write IHDR
    #pragma pack(push, 1)
    typedef struct {
        uint32_t width;
        uint32_t height;
        uint8_t bit_depth;
        uint8_t color_type;
        uint8_t compression_method;
        uint8_t filter_method;
        uint8_t interlace_method;
    } ihdr_t;
    #pragma pack(pop)

    ihdr_t ihdr = {
        .width = to_be_u32(image->width),
        .height = to_be_u32(image->height),
        .bit_depth = image->bit_depth,
        .color_type = get_color_type(image->channels),
        .compression_method = 0,
        .filter_method = 0,
        .interlace_method = 0
    };

    if (ihdr.color_type == 0xFF) return INVALID_PNG;

    uint32_t crc = crc32(0xA8A1AE0A, (unsigned char*)(&ihdr), 13);
    crc = to_be_u32(crc);
    const uint32_t data_len = to_be_u32(13);

    if (!png.write((void*)&PNG_SIGNATURE, png.buf, 8) ||
        !png.write((void*)&data_len, png.buf, 4) ||
        !png.write("IHDR", png.buf, 4) ||
        !png.write(&ihdr, png.buf, 13) ||
        !png.write(&crc, png.buf, 4)) {
        return IO_ERR;
    }

    int ret = encode(image, png);
    if (ret != 0) return ret;

    return 0;
}

static inline uint8_t get_color_type(const uint8_t channels) {
    switch (channels) {
        case 1: return 0;
        case 2: return 4;
        case 3: return 2;
        case 4: return 6;
        default: return 0xFF;
    }
}
