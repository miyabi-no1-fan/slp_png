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

// helpers
static uint8_t get_color_type(const uint8_t channels);
extern int encode(slp_image_t* restrict image, FILE* restrict file);

int slp_png_write(slp_image_t image, const char* path) {
    if (image.height == 0 || image.width == 0 || image.channels == 0) return INVALID_INPUT;
    switch (image.bit_depth) {
        case 1: break;
        case 2: break;
        case 4: break;
        case 8: break;
        case 16: break;
        default: return INVALID_INPUT;
    }

    const uint16_t random_value_for_edian_test = 1;
    const bool is_little_edian = *(uint8_t*)(&random_value_for_edian_test);
    const uint64_t PNG_SIGNATURE = big_edian_u64_in_mem(0x89504E470D0A1A0Aull, is_little_edian);

    FILE* file = fopen(path, "wb");
    if (file == NULL) return FILE_ERR;

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

    ihdr_t header = {
        .width = big_edian_u32_in_mem(image.width, is_little_edian),
        .height = big_edian_u32_in_mem(image.height, is_little_edian),
        .bit_depth = image.bit_depth,
        .color_type = get_color_type(image.channels),
        .compression_method = 0,
        .filter_method = 0,
        .interlace_method = 0
    };

    if (header.color_type == 0xFF) {
        fclose(file);
        return INVALID_INPUT;
    }

    uint32_t crc = crc32(0xA8A1AE0A, (unsigned char*)(&header), 13);
    crc = big_edian_u32_in_mem(crc, is_little_edian);
    const uint32_t data_len = big_edian_u32_in_mem(13, is_little_edian);

    if (fwrite(&PNG_SIGNATURE, 1, 8, file) != 8 ||
        fwrite(&data_len, 1, 4, file) != 4 ||
        fwrite("IHDR", 1, 4, file) != 4 ||
        fwrite(&header, 1, 13, file) != 13 ||
        fwrite(&crc, 1, 4, file) != 4)
    {
        fclose(file);
        return INVALID_FILE;
    }

    int ret = encode(&image, file);
    if (ret != 0) {
        fclose(file);
        return ret;
    }

    fclose(file);
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
