/*
Copyright 2026 slp-c

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
#include <slp_image.h>
#include <slp_png.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <zlib.h>

#if defined(__i386__) || defined(__x86_64__)
#include <immintrin.h>
#endif

// helper
// functions
static int slp_png_get_channels(const int color_type,const int bit_depth);
static int slp_png_defilter(uint8_t* restrict buffer, uint8_t* restrict* restrict scanline, const size_t bpp, const size_t bpr, const size_t imtrker); // defilter, using scanline[0] as the up scanline and scanline[1] as the stream scanline each time
static void slp_png_decode(slp_image_t* restrict slp_png_stream, FILE* restrict file, const size_t file_size, const int color_type);
static void slp_png_colortype3_unpack(uint8_t* restrict buffer, slp_image_t* restrict slp_png_stream, const size_t bpr, const size_t imtrker);
static void slp_png_index_u32_to_RGBA(slp_image_t* restrict slp_png_stream, const uint8_t* restrict palette);

// constants
enum {
    CHUNK = 65536,
    IHDR = 'I' << 24 | 'H' << 16 | 'D' << 8 | 'R',
    IDAT = 'I' << 24 | 'D' << 16 | 'A' << 8 | 'T',
    IEND = 'I' << 24 | 'E' << 16 | 'N' << 8 | 'D',
    PLTE = 'P' << 24 | 'L' << 16 | 'T' << 8 | 'E',
    tRNS = 't' << 24 | 'R' << 16 | 'N' << 8 | 'S'
};



// read png from file
slp_image_t slp_png_read(const char* path) {
    const uint64_t PNG_SIGNATURE = 0x89504E470D0A1A0A;

    slp_image_t slp_png_stream = {0};
    FILE* file;

    file = fopen(path, "rb");
    if (file == NULL) {
        slp_png_stream.bit_depth = 1;
        slp_png_stream.buffer = NULL;
        return slp_png_stream;
    }

    int ret = fseek(file, 0, SEEK_END);
    if (ret != 0) {
        fclose(file);
        slp_png_stream.bit_depth = 1;
        slp_png_stream.buffer = NULL;
        return slp_png_stream;
    }

    size_t file_size = ftell(file);
    if (file_size < 57) {// minimal size required for PNGSIG + IHDR + IDAT(with data len = 0) + IEND
        fclose(file);
        slp_png_stream.bit_depth = 2;
        slp_png_stream.buffer = NULL;
        return slp_png_stream;
    }

    ret = fseek(file, 0, SEEK_SET);
    if (ret != 0) {
        fclose(file);
        slp_png_stream.bit_depth = 1;
        slp_png_stream.buffer = NULL;
        return slp_png_stream;
    }

    uint8_t worker[33];

    if (fread(worker, 1, 33, file) < 33) {
        fclose(file);
        slp_png_stream.bit_depth = 1;
        slp_png_stream.buffer = NULL;
        return slp_png_stream;
    }

    uint32_t crc_ = crc32(0, worker + 12, 4);
    crc_ = crc32(crc_, worker + 16, 13);

    if (big_edian_u64(worker) != PNG_SIGNATURE ||
        big_edian_u32(worker + 8) != 13 ||
        big_edian_u32(worker + 12) != IHDR ||
        big_edian_u32(worker + 29) != crc_)
    {
        fclose(file);
        slp_png_stream.bit_depth = 2;
        slp_png_stream.buffer = NULL;
        return slp_png_stream;
    }

    const uint32_t width = slp_png_stream.width = big_edian_u32(worker + 16);
    const uint32_t height = slp_png_stream.height = big_edian_u32(worker + 20);
    const int bit_depth = slp_png_stream.bit_depth = worker[24];
    const int color_type = worker[25];
    const int channels = slp_png_stream.channels = slp_png_get_channels(color_type, bit_depth);
    const int compression_method = worker[26];
    const int filter_method = worker[27];
    const int interlace_method = worker[28];

    if (compression_method != 0 || filter_method != 0 || interlace_method != 0 || channels == 0) {
        fclose(file);
        slp_png_stream.bit_depth = 2;
        slp_png_stream.buffer = NULL;
        return slp_png_stream;
    }

    const size_t image_size = slp_png_stream.image_size = div_round_up((size_t)height * width * channels * bit_depth, 8);

    #if SLP_USE_ALIGN_ALLOC
    const size_t allocated_size = slp_png_stream.allocated_size = SLP_ALIGN_SIZE(image_size);
    slp_png_stream.buffer = (uint8_t*)SLP_ALIGNED_ALLOC(allocated_size);
    #else
    const size_t allocated_size = slp_png_stream.allocated_size = image_size;
    slp_png_stream.buffer = (uint8_t*)SLP_MALLOC(allocated_size);
    #endif

    if (slp_png_stream.buffer == NULL) {
        fclose(file);
        slp_png_stream.bit_depth = 255;
        slp_png_stream.buffer = NULL;
        return slp_png_stream;
    }

    slp_png_decode(&slp_png_stream, file, file_size, color_type);
    if (slp_png_stream.bit_depth != bit_depth) {
        fclose(file);
        SLP_FREE(slp_png_stream.buffer);
        slp_png_stream.buffer = NULL;
        return slp_png_stream;
    }
    slp_png_stream.bit_depth = (color_type == 3) ? 8 : slp_png_stream.bit_depth;

    fclose(file);
    return slp_png_stream;
}



static inline int slp_png_get_channels(const int color_type, const int bit_depth) {
    int channels;
    switch (color_type) {
        case 0: {
            channels = 1;
            switch (bit_depth) {
                case 1: break;
                case 2: break;
                case 4: break;
                case 8: break;
                case 16: break;
                default: return 0;
            }
            break;
        }
        case 2: {
            channels = 3;
            switch (bit_depth) {
                case 8: break;
                case 16: break;
                default: return 0;
            }
            break;
        }
        case 3: {
            channels = 4;
            switch (bit_depth) {
                case 1: break;
                case 2: break;
                case 4: break;
                case 8: break;
                default: return 0;
            }
            break;
        }
        case 4: {
            channels = 2;
            switch (bit_depth) {
                case 8: break;
                case 16: break;
                default: return 0;
            }
            break;
        }
        case 6: {
            channels = 4;
            switch (bit_depth) {
                case 8: break;
                case 16: break;
                default: return 0;
            }
            break;
        }
        default: return 0;
    }
    return channels;
}


static inline void slp_png_decode(slp_image_t* restrict slp_png_stream, FILE* restrict file, const size_t file_size, const int color_type) {

    const bool is_color_type3 = (color_type == 3);
    
    uint8_t worker[12];
    uint8_t* out = NULL;
    uint8_t* in = NULL;

    size_t data_len = 0;
    int plte_check = 0;
    int tRNS_check = 0;
    int idat_check = 0;
    int iend_check = 0;

    uint8_t* palette = NULL;

    uint8_t* scanline[2] = {NULL, NULL};

    const size_t bpp = is_color_type3 ? 1 : (slp_png_stream->channels * (1 + (slp_png_stream->bit_depth == 16)));
    const size_t bpr = div_round_up(slp_png_stream->width * (is_color_type3 ? 1 : slp_png_stream->channels) * slp_png_stream->bit_depth, 8);

    do {
        if (fread(worker, 1, 8, file) != 8) {
            slp_png_stream->bit_depth = 1;
            goto cleanup;
        }

        uint32_t chunk_type = big_edian_u32(worker + 4);
        data_len = big_edian_u32(worker);


        switch (chunk_type) {

            // ADD MORE CASES HERE

            // IDAT
            case IDAT: {
                idat_check++;
                if (idat_check > 1) {
                    slp_png_stream->bit_depth = 2;
                    goto cleanup;
                }


                z_stream strm = {0};
                strm.zalloc = Z_NULL;
                strm.zfree = Z_NULL;
                strm.opaque = Z_NULL;
                strm.avail_in = 0;
                strm.next_in = Z_NULL;
                int ret = inflateInit(&strm);
                if (ret != Z_OK) {
                    slp_png_stream->bit_depth = 3;
                    goto cleanup;
                }


                size_t imtrker = 0;
                size_t ai = CHUNK; // available input
                size_t intrker = 0; // the 'in' buffer tracker
                size_t offset = 0; // the 'out' buffer tracker
                size_t row_count = 0;

                out = (uint8_t*)SLP_MALLOC(CHUNK);
                in = (uint8_t*)SLP_MALLOC(CHUNK);
                scanline[0] = (is_color_type3) ? ((uint8_t*)SLP_MALLOC(bpr)) : slp_png_stream->buffer;
                scanline[1] = (is_color_type3) ? ((uint8_t*)SLP_MALLOC(bpr)) : slp_png_stream->buffer;

                if (out == NULL || in == NULL || scanline[0] == NULL || scanline[1] == NULL) {
                    slp_png_stream->bit_depth = 255;
                    inflateEnd(&strm);
                    goto cleanup;
                }


                // data_len, ++12, data_len,...
                do { // for each IDAT chunk
                    data_len = big_edian_u32(worker);
                    uint32_t crc = 0x35AF061E;
                    // uint32_t crc = crc32(0, worker + 4, 4) -- worker + 4 is obviously "IDAT"

                    if (data_len < ai) {
                        if (fread(in + intrker, 1, data_len, file) != data_len) {
                            slp_png_stream->bit_depth = 1;
                            inflateEnd(&strm);
                            goto cleanup;
                        }
                        ai -= data_len;
                        crc = crc32(crc, in + intrker, data_len);
                        intrker += data_len;
                    }
                    else {
                        if (fread(in + intrker, 1, ai, file) != ai) {
                            slp_png_stream->bit_depth = 1;
                            inflateEnd(&strm);
                            goto cleanup;
                        }
                        crc = crc32(crc, in + intrker, ai);
                        size_t ftrker = data_len - ai;
                        intrker += ai;
                        //ai = 0;
                        strm.avail_in = intrker;
                        strm.next_in = in;
                        do {
                            do {
                                strm.avail_out = CHUNK - offset;
                                strm.next_out = out + offset;
                                ret = inflate(&strm, Z_NO_FLUSH);
                                size_t have = CHUNK - strm.avail_out;
                                if (ret != Z_OK && ret != Z_STREAM_END) {
                                    slp_png_stream->bit_depth = 3;
                                    inflateEnd(&strm);
                                    goto cleanup;
                                }
                                size_t row_produced = have / (bpr + 1);

                                row_count += row_produced;
                                if (row_count > slp_png_stream->height) {
                                    slp_png_stream->bit_depth = 2;
                                    inflateEnd(&strm);
                                    goto cleanup;
                                }

                                offset = have % (bpr + 1);
                                for (size_t i = 0; i < row_produced; i++) {

                                    // defilter to scanline[1] from buffer as raw and scanline[0] as up
                                    if (slp_png_defilter(out + i * (bpr + 1), scanline, bpp, bpr, imtrker) != 0) {
                                        slp_png_stream->bit_depth = 2;
                                        inflateEnd(&strm);
                                        goto cleanup;
                                    }

                                    if (is_color_type3) {
                                        slp_png_colortype3_unpack(scanline[1], slp_png_stream, bpr, imtrker);

                                        // swap scanline for the next process
                                        uint8_t* temp = scanline[0];
                                        scanline[0] = scanline[1];
                                        scanline[1] = temp;
                                    }
                                    else {
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
                                    slp_png_stream->bit_depth = 1;
                                    inflateEnd(&strm);
                                    goto cleanup;
                                }
                                crc = crc32(crc, in + intrker, ftrker);
                                intrker += ftrker;
                                ai -= ftrker;
                                ftrker = 0;
                            }
                            else {
                                if (fread(in + intrker, 1, ai, file) != ai) {
                                    slp_png_stream->bit_depth = 1;
                                    inflateEnd(&strm);
                                    goto cleanup;
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
                        slp_png_stream->bit_depth = 1;
                        inflateEnd(&strm);
                        goto cleanup;
                    }

                    if (big_edian_u32(worker + 8) != crc) {
                        slp_png_stream->bit_depth = 2;
                        inflateEnd(&strm);
                        goto cleanup;
                    }

                    if (fread(worker, 1, 8, file) != 8) {
                        slp_png_stream->bit_depth = 1;
                        inflateEnd(&strm);
                        goto cleanup;
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
                        slp_png_stream->bit_depth = 3;
                        inflateEnd(&strm);
                        goto cleanup;
                    }
                    size_t row_produced = have / (bpr + 1);

                    row_count += row_produced;
                    if (row_count > slp_png_stream->height) {
                        slp_png_stream->bit_depth = 2;
                        inflateEnd(&strm);
                        goto cleanup;
                    }

                    offset = have % (bpr + 1);
                    for (size_t i = 0; i < row_produced; i++) {

                        // defilter to scanline[1] from buffer and scanline[0]
                        if (slp_png_defilter(out + i * (bpr + 1), scanline, bpp, bpr, imtrker) != 0) {
                            slp_png_stream->bit_depth = 2;
                            inflateEnd(&strm);
                            goto cleanup;
                        }

                        if (is_color_type3) {
                            slp_png_colortype3_unpack(scanline[1], slp_png_stream, bpr, imtrker);

                            // swap scanline for the next process
                            uint8_t* temp = scanline[0];
                            scanline[0] = scanline[1];
                            scanline[1] = temp;
                        }
                        else {
                            // move scanline for the next process
                            scanline[0] = scanline[1];
                            scanline[1] += bpr;
                        }

                        imtrker++;
                    }
                    SLP_MEMMOVE(out, out + have - offset, offset);
                } while (ret != Z_STREAM_END);
                inflateEnd(&strm);

                if (offset != 0) {
                    slp_png_stream->bit_depth = 2;
                    goto cleanup;
                }

                if (fseek(file, -8, SEEK_CUR) != 0) {
                    slp_png_stream->bit_depth = 1;
                    goto cleanup;
                }

                SLP_FREE(out); out = NULL;
                SLP_FREE(in); in = NULL;

                if (is_color_type3) {
                    SLP_FREE(scanline[0]);
                    SLP_FREE(scanline[1]);
                }
                scanline[0] = NULL;
                scanline[1] = NULL;

                break;
            }

            // PLTE
            case PLTE: {
                plte_check++;
                if (plte_check > 1) {
                    slp_png_stream->bit_depth = 2;
                    goto cleanup;
                }

                uint8_t* plte = (uint8_t*)SLP_MALLOC(data_len);
                if (plte == NULL) {
                    slp_png_stream->bit_depth = -1;
                    goto cleanup;
                }

                if (fread(plte, 1, data_len, file) != data_len) {
                    SLP_FREE(plte);
                    slp_png_stream->bit_depth = 1;
                    goto cleanup;
                }

                uint32_t crc_ = crc32(0, worker + 4, 4);
                crc_ = crc32(crc_, plte, data_len);


                if (fread(worker + 8, 1, 4, file) != 4) {
                    SLP_FREE(plte);
                    slp_png_stream->bit_depth = 1;
                    goto cleanup;
                }

                if (big_edian_u32(worker + 8) != crc_) {
                    SLP_FREE(plte);
                    slp_png_stream->bit_depth = 2;
                    goto cleanup;
                }

                if (is_color_type3) {
                    if (data_len % 3 != 0 || data_len / 3 > 256) {
                        SLP_FREE(plte);
                        slp_png_stream->bit_depth = 2;
                        goto cleanup;
                    }

                    palette = (uint8_t*)SLP_CALLOC(256 * 4);// default to always use 256 entries
                    if (palette == NULL) {
                        SLP_FREE(plte);
                        slp_png_stream->bit_depth = 255;
                        goto cleanup;
                    }

                    for (size_t i = 0, k = 0; k + 3 <= data_len; i+=4, k+=3) {
                        palette[i + 0] = plte[k + 0];
                        palette[i + 1] = plte[k + 1];
                        palette[i + 2] = plte[k + 2];
                        palette[i + 3] = 255;
                    }
                }

                SLP_FREE(plte);

                break;
            }

            // tRNS
            case tRNS: {
                tRNS_check++;
                if (tRNS_check > 1) {
                    slp_png_stream->bit_depth = 2;
                    goto cleanup;
                }

                uint8_t* trns = (uint8_t*)SLP_MALLOC(data_len);
                if (trns == NULL) {
                    slp_png_stream->bit_depth = -1;
                    goto cleanup;
                }

                if (fread(trns, 1, data_len, file) != data_len) {
                    SLP_FREE(trns);
                    slp_png_stream->bit_depth = 1;
                    goto cleanup;
                }

                uint32_t crc_ = crc32(0, worker + 4, 4);
                crc_ = crc32(crc_, trns, data_len);

                if (fread(worker + 8, 1, 4, file) != 4) {
                    SLP_FREE(trns);
                    slp_png_stream->bit_depth = 1;
                    goto cleanup;
                }

                if (big_edian_u32(worker + 8) != crc_) {
                    SLP_FREE(trns);
                    slp_png_stream->bit_depth = 2;
                    goto cleanup;
                }

                if (is_color_type3) {
                    if (plte_check == 0 || data_len > 256) {
                        slp_png_stream->bit_depth = 2;
                        goto cleanup;
                    }
                    for (size_t i = 0; i < data_len; i++) palette[i * 4 + 3] = trns[i];
                }

                SLP_FREE(trns);

                break;
            }

            // IEND
            case IEND: {
                if (idat_check == 0 || (is_color_type3 && plte_check == 0)) {
                    slp_png_stream->bit_depth = 2;
                    goto cleanup;
                }
                iend_check = 1;
                break;
            }

            // else = skip
            default: {
                fseek(file, data_len + 4, SEEK_CUR);
                break;
            }
        }

    } while ((size_t)ftell(file) <= (file_size - 12) && iend_check == 0);

    if (iend_check == 0) {
        slp_png_stream->bit_depth = 2;
        goto cleanup;
    }

    if ((size_t)(ftell(file) + 4) != file_size) { // IEND != EOF
        slp_png_stream->bit_depth = 2;
        goto cleanup;
    }

    if (is_color_type3) slp_png_index_u32_to_RGBA(slp_png_stream, palette);

cleanup:
    if (is_color_type3) {
        SLP_FREE(scanline[0]);
        SLP_FREE(scanline[1]);
    }
    SLP_FREE(palette);
    SLP_FREE(out);
    SLP_FREE(in);
    return;
}


static inline int slp_png_defilter(uint8_t* restrict buffer, uint8_t* restrict* restrict scanline, const size_t bpp, const size_t bpr, const size_t imtrker) {
    uint8_t filter = *buffer++;
    switch (filter) {
        case 0: {
            SLP_MEMCPY(scanline[1], buffer, bpr);
            break;
        }
        case 1: {
            SLP_MEMCPY(scanline[1], buffer, bpp);
            for (size_t i = bpp; i < bpr; i++) scanline[1][i] = buffer[i] + scanline[1][i - bpp];
            break;
        }
        case 2: {
            if (imtrker == 0) SLP_MEMCPY(scanline[1], buffer, bpr);
            else {
                size_t i = 0;
                #ifdef __AVX2__
                for (; i + 32 <= bpr; i += 32) {
                    const __m256i raw = _mm256_loadu_si256((const __m256i *)(buffer + i));
                    const __m256i up  = _mm256_loadu_si256((const __m256i *)(scanline[0] + i));
                    _mm256_storeu_si256((__m256i*)(scanline[1] + i), _mm256_add_epi8(raw, up));
                }
                #endif
                #ifdef __SSE2__
                for (; i + 16 <= bpr; i += 16) {
                    const __m128i raw = _mm_loadu_si128((const __m128i*)(buffer + i));
                    const __m128i up  = _mm_loadu_si128((const __m128i*)(scanline[0] + i));
                    _mm_storeu_si128((__m128i *)(scanline[1] + i), _mm_add_epi8(raw, up));
                }
                #endif
                for (; i < bpr; i++) scanline[1][i] = buffer[i] + scanline[0][i];
            }
            break;
        }
        case 3: {
            if (imtrker == 0) {
                SLP_MEMCPY(scanline[1], buffer, bpp);
                for (size_t i = bpp; i < bpr; i++) scanline[1][i] = buffer[i] + (scanline[1][i - bpp] >> 1);
            }
            else {
                size_t i = 0;
                for (; i < bpp; i++) scanline[1][i] = buffer[i] + ((scanline[0][i]) >> 1);
                for (; i < bpr; i++) scanline[1][i] = buffer[i] + ((scanline[0][i] + scanline[1][i - bpp]) >> 1);
            }
            break;
        }
        case 4: {
            if (imtrker == 0) {
                SLP_MEMCPY(scanline[1], buffer, bpp);
                for (size_t i = bpp; i < bpr; i++) scanline[1][i] = buffer[i] + scanline[1][i - bpp];
            }
            else {
                size_t i = 0;
                for (; i < bpp; i++) scanline[1][i] = buffer[i] + scanline[0][i];
                for (; i < bpr; i++) {
                    const int p = scanline[1][i - bpp] + scanline[0][i] - scanline[0][i - bpp];
                    const int pa = abs(p - scanline[1][i - bpp]);
                    const int pb = abs(p - scanline[0][i]);
                    const int pc = abs(p - scanline[0][i - bpp]);

                    uint8_t d = (pb <= pc) ? (scanline[0][i]) : (scanline[0][i - bpp]);
                    d = (pa <= pb && pa <= pc) ? (scanline[1][i - bpp]) : (d);

                    scanline[1][i] = buffer[i] + d;
                }
            }
            break;
        }
        default: return 1;
    }

    return 0;
}


static inline void slp_png_colortype3_unpack(uint8_t* restrict buffer, slp_image_t* restrict slp_png_stream, const size_t bpr, const size_t imtrker) {
    
    uint8_t *src = buffer;
    uint8_t *dest = slp_png_stream->buffer + imtrker * (size_t)slp_png_stream->width * slp_png_stream->channels;

    size_t i = 0;
    switch (slp_png_stream->bit_depth) {
        case 1: {
            #ifdef __SSE2__
            const __m128i zeroes = _mm_setzero_si128();

            for (; i + 16 <= bpr; i += 16) {
                const __m128i in = _mm_loadu_si128((const __m128i*)(src + i));

                const __m128i in0 = _mm_and_si128(_mm_srli_epi64(in, 7), _mm_set1_epi8(1));
                const __m128i in1 = _mm_and_si128(_mm_srli_epi64(in, 6), _mm_set1_epi8(1));
                const __m128i in2 = _mm_and_si128(_mm_srli_epi64(in, 5), _mm_set1_epi8(1));
                const __m128i in3 = _mm_and_si128(_mm_srli_epi64(in, 4), _mm_set1_epi8(1));
                const __m128i in4 = _mm_and_si128(_mm_srli_epi64(in, 3), _mm_set1_epi8(1));
                const __m128i in5 = _mm_and_si128(_mm_srli_epi64(in, 2), _mm_set1_epi8(1));
                const __m128i in6 = _mm_and_si128(_mm_srli_epi64(in, 1), _mm_set1_epi8(1));
                const __m128i in7 = _mm_and_si128(_mm_srli_epi64(in, 0), _mm_set1_epi8(1));


                const __m128i a01_lo = _mm_unpacklo_epi8(in0, in1);
                const __m128i a01_hi = _mm_unpackhi_epi8(in0, in1);
                const __m128i a23_lo = _mm_unpacklo_epi8(in2, in3);
                const __m128i a23_hi = _mm_unpackhi_epi8(in2, in3);
                const __m128i a45_lo = _mm_unpacklo_epi8(in4, in5);
                const __m128i a45_hi = _mm_unpackhi_epi8(in4, in5);
                const __m128i a67_lo = _mm_unpacklo_epi8(in6, in7);
                const __m128i a67_hi = _mm_unpackhi_epi8(in6, in7);


                const __m128i a0123lo_lo = _mm_unpacklo_epi16(a01_lo, a23_lo);
                const __m128i a0123lo_hi = _mm_unpackhi_epi16(a01_lo, a23_lo);
                const __m128i a0123hi_lo = _mm_unpacklo_epi16(a01_hi, a23_hi);
                const __m128i a0123hi_hi = _mm_unpackhi_epi16(a01_hi, a23_hi);
                const __m128i a4567lo_lo = _mm_unpacklo_epi16(a45_lo, a67_lo);
                const __m128i a4567lo_hi = _mm_unpackhi_epi16(a45_lo, a67_lo);
                const __m128i a4567hi_lo = _mm_unpacklo_epi16(a45_hi, a67_hi);
                const __m128i a4567hi_hi = _mm_unpackhi_epi16(a45_hi, a67_hi);


                const __m128i a01234567lo_lo_lo = _mm_unpacklo_epi32(a0123lo_lo, a4567lo_lo);
                const __m128i a01234567lo_lo_hi = _mm_unpackhi_epi32(a0123lo_lo, a4567lo_lo);
                const __m128i a01234567lo_hi_lo = _mm_unpacklo_epi32(a0123lo_hi, a4567lo_hi);
                const __m128i a01234567lo_hi_hi = _mm_unpackhi_epi32(a0123lo_hi, a4567lo_hi);
                const __m128i a01234567hi_lo_lo = _mm_unpacklo_epi32(a0123hi_lo, a4567hi_lo);
                const __m128i a01234567hi_lo_hi = _mm_unpackhi_epi32(a0123hi_lo, a4567hi_lo);
                const __m128i a01234567hi_hi_lo = _mm_unpacklo_epi32(a0123hi_hi, a4567hi_hi);
                const __m128i a01234567hi_hi_hi = _mm_unpackhi_epi32(a0123hi_hi, a4567hi_hi);


                //_mm_storeu_si128((__m128i *)(dest + 0 * 16), a01234567lo_lo_lo);
                {
                    const __m128i x0 = _mm_unpacklo_epi8(a01234567lo_lo_lo, zeroes);
                    const __m128i x1 = _mm_unpackhi_epi8(a01234567lo_lo_lo, zeroes);

                    const __m128i p0 = _mm_unpacklo_epi16(x0, zeroes);
                    const __m128i p1 = _mm_unpackhi_epi16(x0, zeroes);
                    const __m128i p2 = _mm_unpacklo_epi16(x1, zeroes);
                    const __m128i p3 = _mm_unpackhi_epi16(x1, zeroes);

                    _mm_storeu_si128((__m128i*)(dest + i*32 + 0 * 16), p0);
                    _mm_storeu_si128((__m128i*)(dest + i*32 + 1 * 16), p1);
                    _mm_storeu_si128((__m128i*)(dest + i*32 + 2 * 16), p2);
                    _mm_storeu_si128((__m128i*)(dest + i*32 + 3 * 16), p3);
                }


                //_mm_storeu_si128((__m128i *)(dest + 1 * 16), a01234567lo_lo_hi);
                {
                    const __m128i x0 = _mm_unpacklo_epi8(a01234567lo_lo_hi, zeroes);
                    const __m128i x1 = _mm_unpackhi_epi8(a01234567lo_lo_hi, zeroes);

                    const __m128i p0 = _mm_unpacklo_epi16(x0, zeroes);
                    const __m128i p1 = _mm_unpackhi_epi16(x0, zeroes);
                    const __m128i p2 = _mm_unpacklo_epi16(x1, zeroes);
                    const __m128i p3 = _mm_unpackhi_epi16(x1, zeroes);

                    _mm_storeu_si128((__m128i*)(dest + i*32 + 4 * 16), p0);
                    _mm_storeu_si128((__m128i*)(dest + i*32 + 5 * 16), p1);
                    _mm_storeu_si128((__m128i*)(dest + i*32 + 6 * 16), p2);
                    _mm_storeu_si128((__m128i*)(dest + i*32 + 7 * 16), p3);
                }    


                //_mm_storeu_si128((__m128i *)(dest + 2 * 16), a01234567lo_hi_lo);
                {
                    const __m128i x0 = _mm_unpacklo_epi8(a01234567lo_hi_lo, zeroes);
                    const __m128i x1 = _mm_unpackhi_epi8(a01234567lo_hi_lo, zeroes);

                    const __m128i p0 = _mm_unpacklo_epi16(x0, zeroes);
                    const __m128i p1 = _mm_unpackhi_epi16(x0, zeroes);
                    const __m128i p2 = _mm_unpacklo_epi16(x1, zeroes);
                    const __m128i p3 = _mm_unpackhi_epi16(x1, zeroes);

                    _mm_storeu_si128((__m128i*)(dest + i*32 + 8  * 16), p0);
                    _mm_storeu_si128((__m128i*)(dest + i*32 + 9  * 16), p1);
                    _mm_storeu_si128((__m128i*)(dest + i*32 + 10 * 16), p2);
                    _mm_storeu_si128((__m128i*)(dest + i*32 + 11 * 16), p3);
                }


                //_mm_storeu_si128((__m128i *)(dest + 3 * 16), a01234567lo_hi_hi);
                {
                    const __m128i x0 = _mm_unpacklo_epi8(a01234567lo_hi_hi, zeroes);
                    const __m128i x1 = _mm_unpackhi_epi8(a01234567lo_hi_hi, zeroes);

                    const __m128i p0 = _mm_unpacklo_epi16(x0, zeroes);
                    const __m128i p1 = _mm_unpackhi_epi16(x0, zeroes);
                    const __m128i p2 = _mm_unpacklo_epi16(x1, zeroes);
                    const __m128i p3 = _mm_unpackhi_epi16(x1, zeroes);

                    _mm_storeu_si128((__m128i*)(dest + i*32 + 12 * 16), p0);
                    _mm_storeu_si128((__m128i*)(dest + i*32 + 13 * 16), p1);
                    _mm_storeu_si128((__m128i*)(dest + i*32 + 14 * 16), p2);
                    _mm_storeu_si128((__m128i*)(dest + i*32 + 15 * 16), p3);
                }


                //_mm_storeu_si128((__m128i *)(dest + 4 * 16), a01234567hi_lo_lo);
                {
                    const __m128i x0 = _mm_unpacklo_epi8(a01234567hi_lo_lo, zeroes);
                    const __m128i x1 = _mm_unpackhi_epi8(a01234567hi_lo_lo, zeroes);

                    const __m128i p0 = _mm_unpacklo_epi16(x0, zeroes);
                    const __m128i p1 = _mm_unpackhi_epi16(x0, zeroes);
                    const __m128i p2 = _mm_unpacklo_epi16(x1, zeroes);
                    const __m128i p3 = _mm_unpackhi_epi16(x1, zeroes);

                    _mm_storeu_si128((__m128i*)(dest + i*32 + 16 * 16), p0);
                    _mm_storeu_si128((__m128i*)(dest + i*32 + 17 * 16), p1);
                    _mm_storeu_si128((__m128i*)(dest + i*32 + 18 * 16), p2);
                    _mm_storeu_si128((__m128i*)(dest + i*32 + 19 * 16), p3);
                }


                //_mm_storeu_si128((__m128i *)(dest + 5 * 16), a01234567hi_lo_hi);
                {
                    const __m128i x0 = _mm_unpacklo_epi8(a01234567hi_lo_hi, zeroes);
                    const __m128i x1 = _mm_unpackhi_epi8(a01234567hi_lo_hi, zeroes);

                    const __m128i p0 = _mm_unpacklo_epi16(x0, zeroes);
                    const __m128i p1 = _mm_unpackhi_epi16(x0, zeroes);
                    const __m128i p2 = _mm_unpacklo_epi16(x1, zeroes);
                    const __m128i p3 = _mm_unpackhi_epi16(x1, zeroes);

                    _mm_storeu_si128((__m128i*)(dest + i*32 + 20 * 16), p0);
                    _mm_storeu_si128((__m128i*)(dest + i*32 + 21 * 16), p1);
                    _mm_storeu_si128((__m128i*)(dest + i*32 + 22 * 16), p2);
                    _mm_storeu_si128((__m128i*)(dest + i*32 + 23 * 16), p3);
                }


                //_mm_storeu_si128((__m128i *)(dest + 6 * 16), a01234567hi_hi_lo);
                {
                    const __m128i x0 = _mm_unpacklo_epi8(a01234567hi_hi_lo, zeroes);
                    const __m128i x1 = _mm_unpackhi_epi8(a01234567hi_hi_lo, zeroes);

                    const __m128i p0 = _mm_unpacklo_epi16(x0, zeroes);
                    const __m128i p1 = _mm_unpackhi_epi16(x0, zeroes);
                    const __m128i p2 = _mm_unpacklo_epi16(x1, zeroes);
                    const __m128i p3 = _mm_unpackhi_epi16(x1, zeroes);

                    _mm_storeu_si128((__m128i*)(dest + i*32 + 24 * 16), p0);
                    _mm_storeu_si128((__m128i*)(dest + i*32 + 25 * 16), p1);
                    _mm_storeu_si128((__m128i*)(dest + i*32 + 26 * 16), p2);
                    _mm_storeu_si128((__m128i*)(dest + i*32 + 27 * 16), p3);
                }


                //_mm_storeu_si128((__m128i *)(dest + 7 * 16), a01234567hi_hi_hi);
                {
                    const __m128i x0 = _mm_unpacklo_epi8(a01234567hi_hi_hi, zeroes);
                    const __m128i x1 = _mm_unpackhi_epi8(a01234567hi_hi_hi, zeroes);

                    const __m128i p0 = _mm_unpacklo_epi16(x0, zeroes);
                    const __m128i p1 = _mm_unpackhi_epi16(x0, zeroes);
                    const __m128i p2 = _mm_unpacklo_epi16(x1, zeroes);
                    const __m128i p3 = _mm_unpackhi_epi16(x1, zeroes);

                    _mm_storeu_si128((__m128i*)(dest + i*32 + 28 * 16), p0);
                    _mm_storeu_si128((__m128i*)(dest + i*32 + 29 * 16), p1);
                    _mm_storeu_si128((__m128i*)(dest + i*32 + 30 * 16), p2);
                    _mm_storeu_si128((__m128i*)(dest + i*32 + 31 * 16), p3);
                }
            }
            #endif
            for (; i < bpr; i++) {
                dest[i*32 + 0 * 4] = (src[i] >> 7) & 1;
                dest[i*32 + 1 * 4] = (src[i] >> 6) & 1;
                dest[i*32 + 2 * 4] = (src[i] >> 5) & 1;
                dest[i*32 + 3 * 4] = (src[i] >> 4) & 1;
                dest[i*32 + 4 * 4] = (src[i] >> 3) & 1;
                dest[i*32 + 5 * 4] = (src[i] >> 2) & 1;
                dest[i*32 + 6 * 4] = (src[i] >> 1) & 1;
                dest[i*32 + 7 * 4] = (src[i] >> 0) & 1;
            }
            break;
        }
        case 2: {
            #ifdef __SSE2__
            const __m128i zeroes = _mm_setzero_si128();

            for (; i + 16 <= bpr; i += 16) {
                const __m128i in = _mm_loadu_si128((const __m128i*)(src + i));

                const __m128i in0 = _mm_and_si128(_mm_srli_epi64(in, 6), _mm_set1_epi8(3));
                const __m128i in1 = _mm_and_si128(_mm_srli_epi64(in, 4), _mm_set1_epi8(3));
                const __m128i in2 = _mm_and_si128(_mm_srli_epi64(in, 2), _mm_set1_epi8(3));
                const __m128i in3 = _mm_and_si128(_mm_srli_epi64(in, 0), _mm_set1_epi8(3));

                const __m128i in01_lo = _mm_unpacklo_epi8(in0, in1);
                const __m128i in01_hi = _mm_unpackhi_epi8(in0, in1);
                const __m128i in23_lo = _mm_unpacklo_epi8(in2, in3);
                const __m128i in23_hi = _mm_unpackhi_epi8(in2, in3);

                const __m128i in0123lo_lo = _mm_unpacklo_epi16(in01_lo, in23_lo);
                const __m128i in0123lo_hi = _mm_unpackhi_epi16(in01_lo, in23_lo);
                const __m128i in0123hi_lo = _mm_unpacklo_epi16(in01_hi, in23_hi);
                const __m128i in0123hi_hi = _mm_unpackhi_epi16(in01_hi, in23_hi);


                //_mm_storeu_si128((__m128i *)(dest + 0 * 16), in0123lo_lo);
                {
                    const __m128i a0 = _mm_unpacklo_epi8(in0123lo_lo, zeroes);
                    __m128i a1 = _mm_unpackhi_epi8(in0123lo_lo, zeroes);

                    const __m128i p0 = _mm_unpacklo_epi16(a0, zeroes);
                    const __m128i p1 = _mm_unpackhi_epi16(a0, zeroes);
                    const __m128i p2 = _mm_unpacklo_epi16(a1, zeroes);
                    const __m128i p3 = _mm_unpackhi_epi16(a1, zeroes);

                    _mm_storeu_si128((__m128i*)(dest + i*16 + 0 * 16), p0);
                    _mm_storeu_si128((__m128i*)(dest + i*16 + 1 * 16), p1);
                    _mm_storeu_si128((__m128i*)(dest + i*16 + 2 * 16), p2);
                    _mm_storeu_si128((__m128i*)(dest + i*16 + 3 * 16), p3);
                }

                //_mm_storeu_si128((__m128i *)(dest + 1 * 16), in0123lo_hi);
                {
                    const __m128i a0 = _mm_unpacklo_epi8(in0123lo_hi, zeroes);
                    __m128i a1 = _mm_unpackhi_epi8(in0123lo_hi, zeroes);

                    const __m128i p0 = _mm_unpacklo_epi16(a0, zeroes);
                    const __m128i p1 = _mm_unpackhi_epi16(a0, zeroes);
                    const __m128i p2 = _mm_unpacklo_epi16(a1, zeroes);
                    const __m128i p3 = _mm_unpackhi_epi16(a1, zeroes);

                    _mm_storeu_si128((__m128i*)(dest + i*16 + 4 * 16), p0);
                    _mm_storeu_si128((__m128i*)(dest + i*16 + 5 * 16), p1);
                    _mm_storeu_si128((__m128i*)(dest + i*16 + 6 * 16), p2);
                    _mm_storeu_si128((__m128i*)(dest + i*16 + 7 * 16), p3);
                }


                //_mm_storeu_si128((__m128i *)(dest + 2 * 16), in0123hi_lo);
                {
                    const __m128i a0 = _mm_unpacklo_epi8(in0123hi_lo, zeroes);
                    __m128i a1 = _mm_unpackhi_epi8(in0123hi_lo, zeroes);

                    const __m128i p0 = _mm_unpacklo_epi16(a0, zeroes);
                    const __m128i p1 = _mm_unpackhi_epi16(a0, zeroes);
                    const __m128i p2 = _mm_unpacklo_epi16(a1, zeroes);
                    const __m128i p3 = _mm_unpackhi_epi16(a1, zeroes);

                    _mm_storeu_si128((__m128i*)(dest + i*16 + 8  * 16), p0);
                    _mm_storeu_si128((__m128i*)(dest + i*16 + 9  * 16), p1);
                    _mm_storeu_si128((__m128i*)(dest + i*16 + 10 * 16), p2);
                    _mm_storeu_si128((__m128i*)(dest + i*16 + 11 * 16), p3);
                }


                //_mm_storeu_si128((__m128i *)(dest + 3 * 16), in0123hi_hi);
                {
                    const __m128i a0 = _mm_unpacklo_epi8(in0123hi_hi, zeroes);
                    __m128i a1 = _mm_unpackhi_epi8(in0123hi_hi, zeroes);

                    const __m128i p0 = _mm_unpacklo_epi16(a0, zeroes);
                    const __m128i p1 = _mm_unpackhi_epi16(a0, zeroes);
                    const __m128i p2 = _mm_unpacklo_epi16(a1, zeroes);
                    const __m128i p3 = _mm_unpackhi_epi16(a1, zeroes);

                    _mm_storeu_si128((__m128i*)(dest + i*16 + 12 * 16), p0);
                    _mm_storeu_si128((__m128i*)(dest + i*16 + 13 * 16), p1);
                    _mm_storeu_si128((__m128i*)(dest + i*16 + 14 * 16), p2);
                    _mm_storeu_si128((__m128i*)(dest + i*16 + 15 * 16), p3);
                }
            }
            #endif
            for (; i < bpr; i++) {
                dest[i*16 + 0*4] = (src[i] >> 6) & 3;
                dest[i*16 + 1*4] = (src[i] >> 4) & 3;
                dest[i*16 + 2*4] = (src[i] >> 2) & 3;
                dest[i*16 + 3*4] = (src[i] >> 0) & 3;
            }
            break;
        }
        case 4: {
            #ifdef __SSE2__
            const __m128i zeroes = _mm_setzero_si128();

            for (; i + 16 <= bpr; i += 16) {
                const __m128i in = _mm_loadu_si128((const __m128i*)(src + i));

                const __m128i in0 = _mm_and_si128(_mm_srli_epi64(in, 4), _mm_set1_epi8(0x0F));
                const __m128i in1 = _mm_and_si128(_mm_srli_epi64(in, 0), _mm_set1_epi8(0x0F));

                const __m128i out_lo = _mm_unpacklo_epi8(in0, in1);
                const __m128i out_hi = _mm_unpackhi_epi8(in0, in1);

                //_mm_storeu_si128((__m128i *)(dest + 0 * 16), out_lo);
                {
                    const __m128i a0 = _mm_unpacklo_epi8(out_lo, zeroes);
                    const __m128i a1 = _mm_unpackhi_epi8(out_lo, zeroes);

                    const __m128i p0 = _mm_unpacklo_epi16(a0, zeroes);
                    const __m128i p1 = _mm_unpackhi_epi16(a0, zeroes);
                    const __m128i p2 = _mm_unpacklo_epi16(a1, zeroes);
                    const __m128i p3 = _mm_unpackhi_epi16(a1, zeroes);

                    _mm_storeu_si128((__m128i*)(dest + i*8 + 0 * 16), p0);
                    _mm_storeu_si128((__m128i*)(dest + i*8 + 1 * 16), p1);
                    _mm_storeu_si128((__m128i*)(dest + i*8 + 2 * 16), p2);
                    _mm_storeu_si128((__m128i*)(dest + i*8 + 3 * 16), p3);
                }

                //_mm_storeu_si128((__m128i *)(dest + 1 * 16), out_hi);
                {
                    const __m128i a0 = _mm_unpacklo_epi8(out_hi, zeroes);
                    const __m128i a1 = _mm_unpackhi_epi8(out_hi, zeroes);

                    const __m128i p0 = _mm_unpacklo_epi16(a0, zeroes);
                    const __m128i p1 = _mm_unpackhi_epi16(a0, zeroes);
                    const __m128i p2 = _mm_unpacklo_epi16(a1, zeroes);
                    const __m128i p3 = _mm_unpackhi_epi16(a1, zeroes);

                    _mm_storeu_si128((__m128i*)(dest + i*8 + 4 * 16), p0);
                    _mm_storeu_si128((__m128i*)(dest + i*8 + 5 * 16), p1);
                    _mm_storeu_si128((__m128i*)(dest + i*8 + 6 * 16), p2);
                    _mm_storeu_si128((__m128i*)(dest + i*8 + 7 * 16), p3);
                }
            }
            #endif
            for (; i < bpr; i++) {
                dest[i*8 + 0 * 4] = (src[i] >> 4) & 0x0F;
                dest[i*8 + 1 * 4] = (src[i] >> 0) & 0x0F;
            }
            break;
        }
        case 8: {
            #ifdef __SSE2__
            const __m128i zeroes = _mm_setzero_si128();

            for (; i + 16 <= bpr; i += 16) {

                const __m128i in = _mm_loadu_si128((const __m128i*)(src + i));

                const __m128i in_lo = _mm_unpacklo_epi8(in, zeroes);
                const __m128i in_hi = _mm_unpackhi_epi8(in, zeroes);

                const __m128i p0 = _mm_unpacklo_epi16(in_lo, zeroes);
                const __m128i p1 = _mm_unpackhi_epi16(in_lo, zeroes);
                const __m128i p2 = _mm_unpacklo_epi16(in_hi, zeroes);
                const __m128i p3 = _mm_unpackhi_epi16(in_hi, zeroes);

                _mm_storeu_si128((__m128i*)(dest + i*4 + 0 * 16), p0);
                _mm_storeu_si128((__m128i*)(dest + i*4 + 1 * 16), p1);
                _mm_storeu_si128((__m128i*)(dest + i*4 + 2 * 16), p2);
                _mm_storeu_si128((__m128i*)(dest + i*4 + 3 * 16), p3);
            }
            #endif
            for (; i < bpr; i++) dest[i * 4] = src[i];
            break;
        }
    }
}


static inline void slp_png_index_u32_to_RGBA(slp_image_t* restrict slp_png_stream, const uint8_t* restrict palette) {
    for (size_t i = 0; i + slp_png_stream->channels <= slp_png_stream->image_size; i += slp_png_stream->channels) {
        int index = slp_png_stream->buffer[i] * slp_png_stream->channels;
        for (size_t k = 0; k < slp_png_stream->channels; k++) slp_png_stream->buffer[i + k] = palette[index + k];
    }
}


void slp_image_delete(slp_image_t* image) {
    SLP_FREE(image->buffer);
    SLP_MEMSET(image, 0, sizeof(*image));
}
