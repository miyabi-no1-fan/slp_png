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
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>

#define SLP_IMAGE_HELPER_MACROS
#include <slp_image.h>

#define PREFERED_IO_BUF_SIZE 65536
#define __CHUNK_TYPE(x0, x1, x2, x3) ((((uint32_t)x0) << 24) | (((uint32_t)x1) << 16) | (((uint32_t)x2) << 8) | (((uint32_t)x3) << 0))
#define IDAT __CHUNK_TYPE('I', 'D', 'A', 'T')
#define IEND __CHUNK_TYPE('I', 'E', 'N', 'D')
#define PLTE __CHUNK_TYPE('P', 'L', 'T', 'E')
#define tRNS __CHUNK_TYPE('t', 'R', 'N', 'S')

extern int defilter(uint8_t* restrict buffer, uint8_t* restrict* restrict scanline, const size_t bpp, const size_t bpr, const size_t imtrker);
extern void colortype3_unpack(uint8_t* restrict buffer, slp_image_t* restrict image, const size_t bpr, const size_t imtrker);
extern void index_u32_to_RGBA(slp_image_t* restrict image, const uint8_t* restrict palette);

int decode(slp_image_t* restrict image, FILE* restrict file, const size_t file_size, const int color_type) {
    int return_code = 0;
    const bool is_color_type3 = (color_type == 3);

    #define Err(error) do { return_code = error; goto cleanup; } while(0)

    uint8_t worker[12];
    uint8_t* out = NULL;
    uint8_t* in = NULL;

    size_t data_len = 0;
    bool plte_check = false;
    bool tRNS_check = false;
    bool idat_check = false;
    bool iend_check = false;

    uint8_t* palette = NULL;

    uint8_t* scanline[2] = { NULL, NULL };

    const size_t bpp = is_color_type3 ? 1 : (image->channels * (1 + (image->bit_depth == 16)));
    const size_t bpr = div_ceil((size_t)image->width * (is_color_type3 ? 1 : image->channels) * image->bit_depth, 8);

    const size_t CHUNK = (PREFERED_IO_BUF_SIZE >= bpr + 1) ? PREFERED_IO_BUF_SIZE : bpr + 1;

    while (!iend_check) {
        if (fread(worker, 1, 8, file) != 8)
            Err(FILE_ERR);

        uint32_t chunk_type = big_edian_u32(worker + 4);
        data_len = big_edian_u32(worker);

        switch (chunk_type) {
            case IDAT: {
                if (idat_check)
                    Err(INVALID_FILE);
                idat_check = true;

                z_stream strm = { 0 };
                strm.zalloc = Z_NULL;
                strm.zfree = Z_NULL;
                strm.opaque = Z_NULL;
                strm.avail_in = 0;
                strm.next_in = Z_NULL;
                int ret = inflateInit(&strm);
                if (ret != Z_OK)
                    Err(ZLIB_ERR);

                size_t imtrker = 0;  // track the total row produced
                size_t ai = CHUNK;   // available input
                size_t intrker = 0;  // the 'in' buffer tracker
                size_t offset = 0;   // the 'out' buffer tracker

                out = (uint8_t*)SLP_MALLOC(CHUNK);
                in = (uint8_t*)SLP_MALLOC(CHUNK);
                scanline[0] = (is_color_type3) ? ((uint8_t*)SLP_MALLOC(bpr)) : image->pixels;
                scanline[1] = (is_color_type3) ? ((uint8_t*)SLP_MALLOC(bpr)) : image->pixels;

                if (out == NULL || in == NULL || scanline[0] == NULL || scanline[1] == NULL) {
                    inflateEnd(&strm);
                    Err(ALLOC_ERR);
                }

                // data_len, ++12, data_len,...
                do {  // for each IDAT chunk
                    data_len = big_edian_u32(worker);
                    uint32_t crc = 0x35AF061E;
                    // uint32_t crc = crc32(0, worker + 4, 4); // worker + 4 is obviously "IDAT"

                    if (data_len < ai) {
                        if (fread(in + intrker, 1, data_len, file) != data_len) {
                            inflateEnd(&strm);
                            Err(FILE_ERR);
                        }
                        ai -= data_len;
                        crc = crc32(crc, in + intrker, data_len);
                        intrker += data_len;
                    } else {
                        if (fread(in + intrker, 1, ai, file) != ai) {
                            inflateEnd(&strm);
                            Err(FILE_ERR);
                        }
                        crc = crc32(crc, in + intrker, ai);
                        size_t ftrker = data_len - ai;
                        intrker += ai;
                        // ai = 0;
                        strm.avail_in = intrker;
                        strm.next_in = in;
                        do {
                            do {
                                strm.avail_out = CHUNK - offset;
                                strm.next_out = out + offset;
                                ret = inflate(&strm, Z_NO_FLUSH);
                                size_t have = CHUNK - strm.avail_out;
                                if (ret != Z_OK && ret != Z_STREAM_END) {
                                    inflateEnd(&strm);
                                    Err(ZLIB_ERR);
                                }
                                size_t row_produced = have / (bpr + 1);

                                if (imtrker + row_produced > image->height) {
                                    inflateEnd(&strm);
                                    Err(INVALID_FILE);
                                }

                                offset = have % (bpr + 1);
                                for (size_t i = 0; i < row_produced; i++) {
                                    // defilter to scanline[1] from buffer as raw and scanline[0] as up
                                    if (defilter(out + i * (bpr + 1), scanline, bpp, bpr, imtrker) != 0) {
                                        inflateEnd(&strm);
                                        Err(INVALID_FILE);
                                    }

                                    if (is_color_type3) {
                                        colortype3_unpack(scanline[1], image, bpr, imtrker);

                                        // swap scanline for the next process
                                        uint8_t* temp = scanline[0];
                                        scanline[0] = scanline[1];
                                        scanline[1] = temp;
                                    } else {
                                        // move scanline for the next process
                                        scanline[0] = scanline[1];
                                        scanline[1] += bpr;
                                    }

                                    imtrker++;
                                }
                                SLP_MEMMOVE(out, out + have - offset, offset);
                            } while (strm.avail_in > 0);

                            ai = CHUNK;
                            intrker = 0;
                            if (ftrker < ai) {
                                if (fread(in + intrker, 1, ftrker, file) != ftrker) {
                                    inflateEnd(&strm);
                                    Err(FILE_ERR);
                                }
                                crc = crc32(crc, in + intrker, ftrker);
                                intrker += ftrker;
                                ai -= ftrker;
                                ftrker = 0;
                            } else {
                                if (fread(in + intrker, 1, ai, file) != ai) {
                                    inflateEnd(&strm);
                                    Err(FILE_ERR);
                                }
                                crc = crc32(crc, in + intrker, ai);
                                ftrker -= ai;
                                intrker += ai;
                                ai = 0;
                                strm.avail_in = intrker;
                                strm.next_in = in;
                            }
                        } while (ftrker != 0);
                    }

                    if (fread(worker + 8, 1, 4, file) != 4) {
                        inflateEnd(&strm);
                        Err(FILE_ERR);
                    }

                    if (big_edian_u32(worker + 8) != crc) {
                        inflateEnd(&strm);
                        Err(INVALID_FILE);
                    }

                    if (fread(worker, 1, 8, file) != 8) {
                        inflateEnd(&strm);
                        Err(FILE_ERR);
                    }
                } while (big_edian_u32(worker + 4) == IDAT);

                strm.avail_in = intrker;
                strm.next_in = in;
                do {
                    strm.avail_out = CHUNK - offset;
                    strm.next_out = out + offset;
                    ret = inflate(&strm, Z_NO_FLUSH);
                    size_t have = CHUNK - strm.avail_out;
                    if (ret != Z_OK && ret != Z_STREAM_END) {
                        inflateEnd(&strm);
                        Err(ZLIB_ERR);
                    }
                    size_t row_produced = have / (bpr + 1);

                    if (imtrker + row_produced > image->height) {
                        inflateEnd(&strm);
                        Err(INVALID_FILE);
                    }

                    offset = have % (bpr + 1);
                    for (size_t i = 0; i < row_produced; i++) {
                        // defilter to scanline[1] from buffer and scanline[0]
                        if (defilter(out + i * (bpr + 1), scanline, bpp, bpr, imtrker) != 0) {
                            inflateEnd(&strm);
                            Err(INVALID_FILE);
                        }

                        if (is_color_type3) {
                            colortype3_unpack(scanline[1], image, bpr, imtrker);

                            // swap scanline for the next process
                            uint8_t* temp = scanline[0];
                            scanline[0] = scanline[1];
                            scanline[1] = temp;
                        } else {
                            // move scanline for the next process
                            scanline[0] = scanline[1];
                            scanline[1] += bpr;
                        }

                        imtrker++;
                    }
                    SLP_MEMMOVE(out, out + have - offset, offset);
                } while (ret != Z_STREAM_END);
                inflateEnd(&strm);

                if (offset != 0)
                    Err(INVALID_FILE);

                if (fseek(file, -8, SEEK_CUR) != 0)
                    Err(FILE_ERR);

                SLP_FREE(out);
                out = NULL;
                SLP_FREE(in);
                in = NULL;

                if (is_color_type3) {
                    SLP_FREE(scanline[0]);
                    SLP_FREE(scanline[1]);
                }
                scanline[0] = NULL;
                scanline[1] = NULL;

                break;
            }
            case PLTE: {
                if (plte_check)
                    Err(INVALID_FILE);
                plte_check = true;

                uint8_t* plte = (uint8_t*)SLP_MALLOC(data_len);
                if (plte == NULL)
                    Err(ALLOC_ERR);

                if (fread(plte, 1, data_len, file) != data_len) {
                    SLP_FREE(plte);
                    Err(FILE_ERR);
                }

                uint32_t crc_ = crc32(0, worker + 4, 4);
                crc_ = crc32(crc_, plte, data_len);

                if (fread(worker + 8, 1, 4, file) != 4) {
                    SLP_FREE(plte);
                    Err(FILE_ERR);
                }

                if (big_edian_u32(worker + 8) != crc_) {
                    SLP_FREE(plte);
                    Err(INVALID_FILE);
                }

                if (is_color_type3) {
                    if (data_len % 3 != 0 || data_len / 3 > 256) {
                        SLP_FREE(plte);
                        Err(INVALID_FILE);
                    }

                    palette = (uint8_t*)SLP_CALLOC(256 * 4);  // default to always use 256 entries
                    if (palette == NULL) {
                        SLP_FREE(plte);
                        Err(ALLOC_ERR);
                    }

                    for (size_t i = 0, k = 0; k + 3 <= data_len; i += 4, k += 3) {
                        palette[i + 0] = plte[k + 0];
                        palette[i + 1] = plte[k + 1];
                        palette[i + 2] = plte[k + 2];
                        palette[i + 3] = 255;
                    }
                }

                SLP_FREE(plte);

                break;
            }
            case tRNS: {
                if (tRNS_check)
                    Err(INVALID_FILE);
                tRNS_check = true;

                uint8_t* trns = (uint8_t*)SLP_MALLOC(data_len);
                if (trns == NULL)
                    Err(ALLOC_ERR);

                if (fread(trns, 1, data_len, file) != data_len) {
                    SLP_FREE(trns);
                    Err(FILE_ERR);
                }

                uint32_t crc_ = crc32(0, worker + 4, 4);
                crc_ = crc32(crc_, trns, data_len);

                if (fread(worker + 8, 1, 4, file) != 4) {
                    SLP_FREE(trns);
                    Err(FILE_ERR);
                }

                if (big_edian_u32(worker + 8) != crc_) {
                    SLP_FREE(trns);
                    Err(INVALID_FILE);
                }

                if (is_color_type3) {
                    if (plte_check == 0 || data_len > 256) {
                        SLP_FREE(trns);
                        Err(INVALID_FILE);
                    }
                    for (size_t i = 0; i < data_len; i++) palette[i * 4 + 3] = trns[i];
                }

                SLP_FREE(trns);

                break;
            }
            case IEND: {
                if (!idat_check || (is_color_type3 && !plte_check))
                    Err(INVALID_FILE);

                uint32_t crc_ = crc32(0, worker + 4, 4);
                if (fread(worker + 8, 1, 4, file) != 4)
                    Err(FILE_ERR);
                if (big_edian_u32(worker + 8) != crc_)
                    Err(INVALID_FILE);

                iend_check = true;
                break;
            }
            // else = skip
            default: {
                fseek(file, data_len + 4, SEEK_CUR);
                // we don't have to check for fseek error here,
                // the fread follow after it will fail anyway
                break;
            }
        }
    }

    if ((size_t)ftell(file) != file_size)
        Err(INVALID_FILE);  // IEND != EOF

    if (is_color_type3)
        index_u32_to_RGBA(image, palette);
cleanup:
    if (is_color_type3) {
        SLP_FREE(scanline[0]);
        SLP_FREE(scanline[1]);
    }
    SLP_FREE(palette);
    SLP_FREE(out);
    SLP_FREE(in);
    return return_code;
}
