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
#include <zconf.h>
#include <zlib.h>

#define SLP_PNG_MACROS
#include "slp_png.h"

// constants
#define COMPRESSION_LEVEL 6
#define CHUNK 65536  // sizeof 1 IDAT chunk

extern void filter(uint8_t* restrict image_buffer, int8_t* restrict* restrict filter_buffers, uint64_t* restrict filter_scores, const size_t i, const size_t bpr, const size_t bpp);

int encode(const slp_image_t* restrict image, slp_png_io png) {
    int return_code = 0;
    #define Err(v) do { return_code = v; goto cleanup; } while(0)

    const uint16_t random_value_for_edian_test = 1;
    const bool is_little_edian = *(uint8_t*)(&random_value_for_edian_test);

    const size_t width = image->width;
    const size_t height = image->height;
    const size_t channels = image->channels;
    const size_t bit_depth = image->bit_depth;

    const size_t bpp = channels * (1 + (bit_depth == 16));
    const size_t bpr = div_ceil(width * channels * bit_depth, 8);

    int8_t* filter_buffers[5] = {
        (int8_t*)SLP_MALLOC(bpr + 1),
        (int8_t*)SLP_MALLOC(bpr + 1),
        (int8_t*)SLP_MALLOC(bpr + 1),
        (int8_t*)SLP_MALLOC(bpr + 1),
        (int8_t*)SLP_MALLOC(bpr + 1),
    };
    uint8_t* out = (uint8_t*)SLP_MALLOC(CHUNK + 12);

    if (filter_buffers[0] == NULL ||
        filter_buffers[1] == NULL ||
        filter_buffers[2] == NULL ||
        filter_buffers[3] == NULL ||
        filter_buffers[4] == NULL ||
        out == NULL)
    {
        Err(ALLOC_ERR);
    }

    SLP_MEMCPY(out + 4, "IDAT", 4);
    filter_buffers[0][0] = 0;
    filter_buffers[1][0] = 1;
    filter_buffers[2][0] = 2;
    filter_buffers[3][0] = 3;
    filter_buffers[4][0] = 4;

    // init zlib.
    // Adler-32 checksum disabled.
    // > Gemini: Z_FILTERED is a good strategy for images
    z_stream strm = {};
    int ret = deflateInit2(&strm, COMPRESSION_LEVEL, Z_DEFLATED, MAX_WBITS, MAX_MEM_LEVEL, Z_FILTERED);
    if (ret != Z_OK)
        Err(ZLIB_ERR);

    // let zlib track our available output capacity
    strm.avail_out = CHUNK;
    // we only track the output len
    uint32_t out_len = 0;

    // for each line
    for (size_t i = 0; i < height; i++) {
        // calculate all 5 filters
        uint64_t filter_scores[5] = {0};
        filter(image->pixels, filter_buffers, filter_scores, i, bpr, bpp);

        // pick the best filter_type (lowest score)
        unsigned int filter_type = 0;
        for (unsigned int i = 0; i < 5; i++)
            filter_type = (filter_scores[i] < filter_scores[filter_type]) ? i : filter_type;

        // deflate loop
        strm.next_in = (uint8_t*)filter_buffers[filter_type];
        strm.avail_in = bpr + 1;
        do {
            strm.next_out = out + 8 + out_len;
            ret = deflate(&strm, Z_NO_FLUSH);
            if (ret != Z_OK) {
                deflateEnd(&strm);
                Err(ZLIB_ERR);
            }
            out_len = CHUNK - strm.avail_out;

            // flush if out of output capacity
            if (strm.avail_out == 0) {
                uint32_t chunk_len = big_edian_u32_in_mem(out_len, is_little_edian);
                SLP_MEMCPY(out, &chunk_len, 4);
                uint32_t crc_ = crc32(0, out + 4, 4);
                crc_ = crc32(crc_, out + 8, out_len);
                crc_ = big_edian_u32_in_mem(crc_, is_little_edian);
                SLP_MEMCPY(out + 8 + out_len, &crc_, 4);

                if (!png.write(out, png.buf, 8 + out_len + 4)) {
                    deflateEnd(&strm);
                    Err(IO_ERR);
                }

                strm.avail_out = CHUNK;
                out_len = 0;
            }
        } while (strm.avail_in > 0);
    }

    // finish
    do {
        strm.next_out = out + 8 + out_len;
        ret = deflate(&strm, Z_FINISH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            deflateEnd(&strm);
            Err(ZLIB_ERR);
        }
        out_len = CHUNK - strm.avail_out;
        if (strm.avail_out == 0) {
            uint32_t chunk_len = big_edian_u32_in_mem(out_len, is_little_edian);
            SLP_MEMCPY(out, &chunk_len, 4);
            uint32_t crc_ = crc32(0, out + 4, 4 + out_len);
            crc_ = big_edian_u32_in_mem(crc_, is_little_edian);
            SLP_MEMCPY(out + 8 + out_len, &crc_, 4);
            if (!png.write(out, png.buf, 8 + out_len + 4)) {
                deflateEnd(&strm);
                Err(IO_ERR);
            }
            strm.avail_out = CHUNK;
            out_len = 0;
        }
    } while (ret != Z_STREAM_END);
    deflateEnd(&strm);

    // flush
    uint32_t chunk_len = big_edian_u32_in_mem(out_len, is_little_edian);
    SLP_MEMCPY(out, &chunk_len, 4);
    uint32_t crc_ = crc32(0, out + 4, 4 + out_len);
    crc_ = big_edian_u32_in_mem(crc_, is_little_edian);
    SLP_MEMCPY(out + 8 + out_len, &crc_, 4);
    if (!png.write(out, png.buf, 8 + out_len + 4))
        Err(IO_ERR);

    // writting IEND
    const uint8_t IENDsig[12] = {0, 0, 0, 0, 'I', 'E', 'N', 'D', 0xAE, 0x42, 0x60, 0x82};
    if (!png.write((void*)IENDsig, png.buf, 12))
        Err(IO_ERR);
cleanup:
    SLP_FREE(filter_buffers[0], bpr + 1);
    SLP_FREE(filter_buffers[1], bpr + 1);
    SLP_FREE(filter_buffers[2], bpr + 1);
    SLP_FREE(filter_buffers[3], bpr + 1);
    SLP_FREE(filter_buffers[4], bpr + 1);
    SLP_FREE(out, CHUNK + 12);
    return return_code;
}
