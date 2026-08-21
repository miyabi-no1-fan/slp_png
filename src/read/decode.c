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

#define PREFERED_IO_BUF_SIZE 65536
#define _CHUNK_TYPE(x0, x1, x2, x3) ((((uint32_t)x0) << 24) | (((uint32_t)x1) << 16) | (((uint32_t)x2) << 8) | (((uint32_t)x3) << 0))
#define IDAT _CHUNK_TYPE('I', 'D', 'A', 'T')
#define IEND _CHUNK_TYPE('I', 'E', 'N', 'D')
#define PLTE _CHUNK_TYPE('P', 'L', 'T', 'E')
#define tRNS _CHUNK_TYPE('t', 'R', 'N', 'S')
#define Err(error) do { return_code = error; goto cleanup; } while(0)

extern int defilter(uint8_t* restrict buffer, uint8_t* restrict cur, uint8_t* restrict prev, const size_t bpp, const size_t bpr);
extern void colortype3_unpack(slp_image_t* restrict image, uint8_t* restrict buffer, const size_t bpr, const size_t imtrker);
extern void index_u32_to_RGBA(slp_image_t* restrict image, const uint8_t* restrict palette);

static inline int parse_idats(slp_png_io png, slp_image_t* restrict image, const int color_type, uint8_t worker[12]);

int decode(slp_png_io png, slp_image_t* restrict image, const int color_type) {
    int return_code = 0;

    uint8_t worker[12];       // buffer for parsing chunk's metadata
    uint8_t* palette = NULL;  // color type 3 palette

    bool plte_check = false;
    bool tRNS_check = false;
    bool idat_check = false;
    bool iend_check = false;

    uint32_t chunk_len;
    uint32_t chunk_type;
    bool should_update_chunk_type_and_len = true;

    while (!iend_check) {
        if (should_update_chunk_type_and_len) {
            if (!png.read(worker, png.buf, 8))
                Err(IO_ERR);
        }
        should_update_chunk_type_and_len = true;

        chunk_len = big_edian_u32(worker);
        chunk_type = big_edian_u32(worker + 4);

        switch (chunk_type) {
            case IDAT: {
                if (idat_check)
                    Err(INVALID_PNG);
                idat_check = true;

                // idat decode is too long so we have to seperate it into another function
                int ret = parse_idats(png, image, color_type, worker);
                if (ret != 0)
                    Err(ret);

                should_update_chunk_type_and_len = false;
                break;
            }
            case PLTE: {
                if (plte_check)
                    Err(INVALID_PNG);
                plte_check = true;

                // max chunk length for a PLTE is for color type 3 with 256 entries of RGB
                if (chunk_len > 256 * 3)
                    Err(INVALID_PNG);

                uint8_t* plte = (uint8_t*)SLP_MALLOC(chunk_len);
                if (plte == NULL)
                    Err(ALLOC_ERR);

                if (!png.read(plte, png.buf, chunk_len)) {
                    SLP_FREE(plte, chunk_len);
                    Err(IO_ERR);
                }

                uint32_t crc_ = crc32(0, worker + 4, 4);
                crc_ = crc32(crc_, plte, chunk_len);

                if (!png.read(worker + 8, png.buf, 4)) {
                    SLP_FREE(plte, chunk_len);
                    Err(IO_ERR);
                }

                if (big_edian_u32(worker + 8) != crc_) {
                    SLP_FREE(plte, chunk_len);
                    Err(INVALID_PNG);
                }

                if (color_type == 3) {
                    if (chunk_len % 3 != 0 || chunk_len / 3 > 256) {
                        SLP_FREE(plte, chunk_len);
                        Err(INVALID_PNG);
                    }

                    palette = (uint8_t*)SLP_CALLOC(256 * 4);  // default to always use 256 entries
                    if (palette == NULL) {
                        SLP_FREE(plte, chunk_len);
                        Err(ALLOC_ERR);
                    }

                    for (size_t i = 0, k = 0; k + 3 <= chunk_len; i += 4, k += 3) {
                        palette[i + 0] = plte[k + 0];
                        palette[i + 1] = plte[k + 1];
                        palette[i + 2] = plte[k + 2];
                        palette[i + 3] = 255;
                    }
                }

                SLP_FREE(plte, chunk_len);

                break;
            }
            case tRNS: {
                if (tRNS_check)
                    Err(INVALID_PNG);
                tRNS_check = true;

                // max chunk length for a tRNS is for color type 3 with 256 entries of alpha channel
                if (chunk_len > 256)
                    Err(INVALID_PNG);

                uint8_t* trns = (uint8_t*)SLP_MALLOC(chunk_len);
                if (trns == NULL)
                    Err(ALLOC_ERR);

                if (!png.read(trns, png.buf, chunk_len)) {
                    SLP_FREE(trns, chunk_len);
                    Err(IO_ERR);
                }

                uint32_t crc_ = crc32(0, worker + 4, 4);
                crc_ = crc32(crc_, trns, chunk_len);

                if (!png.read(worker + 8, png.buf, 4)) {
                    SLP_FREE(trns, chunk_len);
                    Err(IO_ERR);
                }

                if (big_edian_u32(worker + 8) != crc_) {
                    SLP_FREE(trns, chunk_len);
                    Err(INVALID_PNG);
                }

                if (color_type == 3) {
                    if (plte_check == 0 || chunk_len > 256) {
                        SLP_FREE(trns, chunk_len);
                        Err(INVALID_PNG);
                    }
                    for (size_t i = 0; i < chunk_len; i++) palette[i * 4 + 3] = trns[i];
                }

                SLP_FREE(trns, chunk_len);

                break;
            }
            case IEND: {
                if (!idat_check || (color_type == 3 && !plte_check) || chunk_len != 0)
                    Err(INVALID_PNG);

                uint32_t crc_ = crc32(0, worker + 4, 4);
                if (!png.read(worker + 8, png.buf, 4))
                    Err(IO_ERR);
                if (big_edian_u32(worker + 8) != crc_)
                    Err(INVALID_PNG);

                iend_check = true;
                break;
            }
            default: {
                // else = skip
                if (!png.seek(png.buf, chunk_len + 4))
                    Err(IO_ERR);
                break;
            }
        }
    }

    if (color_type == 3)
        index_u32_to_RGBA(image, palette);
cleanup:
    SLP_FREE(palette, 256 * 4);
    return return_code;
}

typedef struct {
    z_stream strm;

    uint8_t* out;
    size_t out_len;
    size_t offset;

    uint8_t* cur;
    uint8_t* prev;

    size_t current_row;
    size_t bpp;
    size_t bpr;
    uint8_t color_type;
} idat_decoder_t;

static inline int idat_decoder_init(idat_decoder_t* restrict decoder, slp_image_t* restrict image, const size_t bpp, const size_t bpr, const int color_type) {
    memset(decoder, 0, sizeof(*decoder));

    int ret = inflateInit2(&decoder->strm, MAX_WBITS);
    if (ret != Z_OK)
        return ZLIB_ERR;

    decoder->out_len = (PREFERED_IO_BUF_SIZE < bpr + 1) ? bpr + 1 : PREFERED_IO_BUF_SIZE;
    decoder->out = (uint8_t*)SLP_MALLOC(decoder->out_len);

    // commit 87d0911712e0e8632ffcc1903ddf64e59043fd4b
    decoder->prev = (color_type == 3) ? ((uint8_t*)SLP_CALLOC(bpr)) : (image->pixels + image->image_size - bpr);
    decoder->cur = (color_type == 3) ? ((uint8_t*)SLP_CALLOC(bpr)) : image->pixels;

    if (decoder->out == NULL || decoder->prev == NULL || decoder->cur == NULL) {
        inflateEnd(&decoder->strm);
        SLP_FREE(decoder->out, decode->out_len);
        if (color_type == 3) {
            SLP_FREE(decoder->prev, bpr);
            SLP_FREE(decoder->cur, bpr);
        }
        return ALLOC_ERR;
    }

    decoder->bpp = bpp;
    decoder->bpr = bpr;
    decoder->color_type = color_type;

    return 0;
}

static inline int idat_decoder_flush(idat_decoder_t* decoder, size_t avail_in, uint8_t* next_in, slp_image_t* restrict image) {
    int ret = 0;

    decoder->strm.avail_in = avail_in;
    decoder->strm.next_in = next_in;

    // loop until all input consumed or stream end
    while (decoder->strm.avail_in > 0 && ret != Z_STREAM_END) {
        decoder->strm.avail_out = decoder->out_len - decoder->offset;
        decoder->strm.next_out = decoder->out + decoder->offset;

        ret = inflate(&decoder->strm, Z_NO_FLUSH);
        if (!(ret == Z_OK || ret == Z_STREAM_END))
            return ZLIB_ERR;

        size_t have = decoder->out_len - decoder->strm.avail_out;

        size_t row_produced = have / (decoder->bpr + 1);
        if (decoder->current_row + row_produced > image->height)
            return INVALID_PNG;

        // for each new row produced
        for (size_t i = 0; i < row_produced; i++) {
            // defilter to cur from buffer as raw and prev as up
            if (defilter(decoder->out + i * (decoder->bpr + 1), decoder->cur, decoder->prev, decoder->bpp, decoder->bpr) != 0)
                return INVALID_PNG;

            if (decoder->color_type == 3) {
                // unpack tightly packed idexes into u32 little-edian array
                colortype3_unpack(image, decoder->cur, decoder->bpr, decoder->current_row);

                // swap scanline for the next process
                uint8_t* temp = decoder->prev;
                decoder->prev = decoder->cur;
                decoder->cur = temp;
            }
            else {
                // move scanline forward for the next process
                decoder->prev = decoder->cur;
                decoder->cur += decoder->bpr;
            }

            decoder->current_row++;
        }

        // move the residues back to the head of the output buffer
        decoder->offset = have % (decoder->bpr + 1);
        SLP_MEMMOVE(decoder->out, decoder->out + have - decoder->offset, decoder->offset);
    }

    if (ret == Z_STREAM_END) {
        if (decoder->strm.avail_in > 0)
            return ZLIB_ERR;

        // all data are consumed
        // there shouldn't be any residues left
        if (decoder->offset != 0 || decoder->current_row != image->height)
            return INVALID_PNG;
    }

    return 0;
}

static inline void idat_decoder_destroy(idat_decoder_t* decoder) {
    if (decoder == NULL) return;

    inflateEnd(&decoder->strm);
    SLP_FREE(decoder->out, decode->out_len);
    if (decoder->color_type == 3) {
        SLP_FREE(decoder->prev, bpr);
        SLP_FREE(decoder->cur, bpr);
    }
}

static inline int parse_idats(slp_png_io png, slp_image_t* restrict image, const int color_type, uint8_t worker[12]) {
    int return_code = 0;

    const size_t _c = (color_type == 3 ? 1 : image->channels);  // for indexed, channels are 1
    const size_t bpp = _c * div_ceil((size_t)image->bit_depth, 8);
    const size_t bpr = div_ceil((size_t)image->width * _c * image->bit_depth, 8);

    uint8_t* in = NULL;
    idat_decoder_t decoder = {0};

    int ret = idat_decoder_init(&decoder, image, bpp, bpr, color_type);
    if (ret != 0)
        Err(ret);

    const size_t IN_LEN = PREFERED_IO_BUF_SIZE;
    in = (uint8_t*)SLP_MALLOC(IN_LEN);
    if (in == NULL)
        Err(ALLOC_ERR);

    size_t remaining_in_cap = IN_LEN;  // remaining in capacity

    uint32_t chunk_type = IDAT;
    uint32_t chunk_len = big_edian_u32(worker);

    // for each IDAT chunk
    do {
        // `uint32_t crc = crc32(0, worker + 4, 4);`
        // `worker + 4` is "IDAT" - the loop's condition
        // so => `uint32_t crc = crc32(0, "IDAT", 4);`
        uint32_t crc = 0x35AF061E;

        while (chunk_len > remaining_in_cap) {
            // read as much as remaining input capacity has
            if (!png.read(in + IN_LEN - remaining_in_cap, png.buf, remaining_in_cap))
                Err(IO_ERR);

            crc = crc32(crc, in + IN_LEN - remaining_in_cap, remaining_in_cap);
            chunk_len -= remaining_in_cap;
            remaining_in_cap = 0;

            // flush the input buffer
            int ret = idat_decoder_flush(&decoder, IN_LEN - remaining_in_cap, in, image);
            if (ret != 0)
                Err(ret);

            // all input consumed so in is now empty
            remaining_in_cap = IN_LEN;
        }

        // read as much as chunk_len remains
        if (!png.read(in + IN_LEN - remaining_in_cap, png.buf, chunk_len))
            Err(IO_ERR);
        crc = crc32(crc, in + IN_LEN - remaining_in_cap, chunk_len);
        remaining_in_cap -= chunk_len;

        // read chunk's crc
        if (!png.read(worker + 8, png.buf, 4))
            Err(IO_ERR);

        // validate chunk's crc
        if (big_edian_u32(worker + 8) != crc)
            Err(INVALID_PNG);

        // read the next chunk's header
        if (!png.read(worker, png.buf, 8))
            Err(IO_ERR);
        chunk_len = big_edian_u32(worker);
        chunk_type = big_edian_u32(worker + 4);

        // break if chunk_type is not IDAT
    } while (chunk_type == IDAT);

    // finish + flush
    ret = idat_decoder_flush(&decoder, IN_LEN - remaining_in_cap, in, image);
    if (ret != 0)
        Err(ret);

cleanup:
    idat_decoder_destroy(&decoder);
    SLP_FREE(in, IN_LEN);
    return return_code;
}
