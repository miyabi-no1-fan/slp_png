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
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define SLP_IMAGE_HELPER_MACROS
#include <slp_image.h>
#include <slp_png.h>

// constants
#define COMPRESSION_LEVEL 6
#define CHUNK 65536  // sizeof 1 IDAT chunk

// helpers
static uint8_t slp_get_color_type(const uint8_t channels);
static int slp_png_encode(slp_image_t* restrict image, FILE* restrict file);
extern void slp_png_filter(uint8_t* restrict image_buffer, int8_t* restrict* restrict filter_buffers, uint64_t* restrict filter_scores, const size_t i, const size_t bpr, const size_t bpp);

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
        .color_type = slp_get_color_type(image.channels),
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

    int ret = slp_png_encode(&image, file);
    if (ret != 0) {
        fclose(file);
        return ret;
    }

    fclose(file);
    return 0;
}

static inline uint8_t slp_get_color_type(const uint8_t channels) {
    switch (channels) {
        case 1: return 0;
        case 2: return 4;
        case 3: return 2;
        case 4: return 6;
        default: return 0xFF;
    }
}

static inline int slp_png_encode(slp_image_t* restrict image, FILE* restrict file) {
    // initialize variables
    int return_code = 0;
    #define Err(v) do { return_code = v; goto cleanup; } while(0)

    const uint16_t random_value_for_edian_test = 1;
    const bool is_little_edian = *(uint8_t*)(&random_value_for_edian_test);

    const size_t width = image->width;
    const size_t height = image->height;
    const size_t channels = image->channels;
    const size_t bit_depth = image->bit_depth;

    const size_t bpp = channels * (1 + (bit_depth == 16));
    const size_t bpr = div_ceil(width * channels * bit_depth, 8);  // bytes per row

    size_t have = 0;
    size_t data_len = 0;

    uint8_t* mem_ptr = NULL;

    mem_ptr = (uint8_t*)SLP_MALLOC((bpr + 1) * 5 + CHUNK + 12);
    if (mem_ptr == NULL)
        Err(ALLOC_ERR);

    int8_t* filter_buffers[5];
    filter_buffers[0] = (int8_t*)mem_ptr + (bpr + 1) * 0;
    filter_buffers[1] = (int8_t*)mem_ptr + (bpr + 1) * 1;
    filter_buffers[2] = (int8_t*)mem_ptr + (bpr + 1) * 2;
    filter_buffers[3] = (int8_t*)mem_ptr + (bpr + 1) * 3;
    filter_buffers[4] = (int8_t*)mem_ptr + (bpr + 1) * 4;
    uint8_t* out = mem_ptr + (bpr + 1) * 5;

    SLP_MEMCPY(out + 4, "IDAT", 4);
    filter_buffers[0][0] = 0;
    filter_buffers[1][0] = 1;
    filter_buffers[2][0] = 2;
    filter_buffers[3][0] = 3;
    filter_buffers[4][0] = 4;
    // end initialize variables

    // CHUNK BEFORE IDAT STAY HERE

    // writting IDAT
    z_stream strm = {0};
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    int ret = deflateInit2(&strm, COMPRESSION_LEVEL, Z_DEFLATED, 15, 9, Z_FILTERED);
    if (ret != Z_OK)
        Err(ZLIB_ERR);

    strm.avail_out = CHUNK;

    for (size_t i = 0; i < height; i++) {
        uint64_t filter_scores[5] = {0};
        slp_png_filter(image->pixels, filter_buffers, filter_scores, i, bpr, bpp);

        unsigned int filter_type = 0;
        for (unsigned int i = 0; i < 5; i++)
            filter_type = (filter_scores[i] < filter_scores[filter_type]) ? i : filter_type;

        strm.next_in = (uint8_t*)filter_buffers[filter_type];
        strm.avail_in = bpr + 1;
        do {
            strm.next_out = out + 8 + have;
            ret = deflate(&strm, Z_NO_FLUSH);
            if (ret != Z_OK) {
                deflateEnd(&strm);
                Err(ZLIB_ERR);
            }
            have = CHUNK - strm.avail_out;
            if (strm.avail_out == 0) {
                data_len = (uint32_t)(have);
                data_len = big_edian_u32_in_mem(data_len, is_little_edian);
                SLP_MEMCPY(out, &data_len, 4);
                uint32_t crc_ = crc32(0, out + 4, 4);
                crc_ = crc32(crc_, out + 8, have);
                crc_ = big_edian_u32_in_mem(crc_, is_little_edian);
                SLP_MEMCPY(out + 8 + have, &crc_, 4);

                if (fwrite(out, 1, 8 + have + 4, file) != 8 + have + 4) {
                    deflateEnd(&strm);
                    Err(FILE_ERR);
                }

                strm.avail_out = CHUNK;
                have = 0;
            }
        } while (strm.avail_in > 0);
    }

    do {
        strm.next_out = out + 8 + have;
        ret = deflate(&strm, Z_FINISH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            deflateEnd(&strm);
            Err(ZLIB_ERR);
        }
        have = CHUNK - strm.avail_out;
        if (strm.avail_out == 0) {
            data_len = (uint32_t)(have);
            data_len = big_edian_u32_in_mem(data_len, is_little_edian);
            SLP_MEMCPY(out, &data_len, 4);
            uint32_t crc_ = crc32(0, out + 4, 4 + have);
            crc_ = big_edian_u32_in_mem(crc_, is_little_edian);
            SLP_MEMCPY(out + 8 + have, &crc_, 4);
            if (fwrite(out, 1, 8 + have + 4, file) != 8 + have + 4) {
                deflateEnd(&strm);
                Err(FILE_ERR);
            }
            strm.avail_out = CHUNK;
            have = 0;
        }
    } while (ret != Z_STREAM_END);
    deflateEnd(&strm);

    data_len = (uint32_t)(have);
    data_len = big_edian_u32_in_mem(data_len, is_little_edian);
    SLP_MEMCPY(out, &data_len, 4);
    uint32_t crc_ = crc32(0, out + 4, 4 + have);
    crc_ = big_edian_u32_in_mem(crc_, is_little_edian);
    SLP_MEMCPY(out + 8 + have, &crc_, 4);
    if (fwrite(out, 1, 8 + have + 4, file) != 8 + have + 4)
        Err(FILE_ERR);
    // finish writting IDAT

    // CHUNK AFTER IDAT STAY HERE

    // writting IEND
    const uint8_t IENDsig[12] = {0, 0, 0, 0, 'I', 'E', 'N', 'D', 0xAE, 0x42, 0x60, 0x82};
    if (fwrite(IENDsig, 1, 12, file) != 12)
        Err(FILE_ERR);
cleanup:
    SLP_FREE(mem_ptr);
    return return_code;
}
