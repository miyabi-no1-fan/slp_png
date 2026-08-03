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
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#if defined(__i386__) || defined(__x86_64__)
    #include <immintrin.h>
#endif

#define SLP_IMAGE_HELPER_MACROS
#include <slp_image.h>
#include <slp_png.h>

#ifndef SLP_DEBUG
#define SLP_DEBUG 0
#endif

#if SLP_DEBUG
    #undef SLP_DEBUG
    #define SLP_DEBUG(err) image.bit_depth = err
#else
    #undef SLP_DEBUG
    #define SLP_DEBUG(err) do { int e = err; } while(0)
#endif

// constants
#define PNG_SIGNATURE 0x89504E470D0A1A0Aull
#define PREFERED_IO_BUF_SIZE 65536
#define __CHUNK_TYPE(x0, x1, x2, x3) ((((uint32_t)x0) << 24) | (((uint32_t)x1) << 16) | (((uint32_t)x2) << 8) | (((uint32_t)x3) << 0))
#define IHDR __CHUNK_TYPE('I', 'H', 'D', 'R')
#define IDAT __CHUNK_TYPE('I', 'D', 'A', 'T')
#define IEND __CHUNK_TYPE('I', 'E', 'N', 'D')
#define PLTE __CHUNK_TYPE('P', 'L', 'T', 'E')
#define tRNS __CHUNK_TYPE('t', 'R', 'N', 'S')

// helpers
static int slp_png_get_channels(const int color_type, const int bit_depth);
static int slp_png_defilter(uint8_t* restrict buffer, uint8_t* restrict* restrict scanline, const size_t bpp, const size_t bpr, const size_t imtrker);
static int slp_png_decode(slp_image_t* restrict image, FILE* restrict file, const size_t file_size, const int color_type);
static void slp_png_colortype3_unpack(uint8_t* restrict buffer, slp_image_t* restrict image, const size_t bpr, const size_t imtrker);
static void slp_png_index_u32_to_RGBA(slp_image_t* restrict image, const uint8_t* restrict palette);

// read png from a file
slp_image_t slp_png_read(const char* path) {
    slp_image_t image = {};

    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        SLP_DEBUG(FILE_ERR);
        image.pixels = NULL;
        return image;
    }

    int ret = fseek(file, 0, SEEK_END);
    if (ret != 0) {
        fclose(file);
        SLP_DEBUG(FILE_ERR);
        image.pixels = NULL;
        return image;
    }

    size_t file_size = ftell(file);
    if (file_size < 57) {  // minimal size required for PNGSIG + IHDR + IDAT(with data len = 0) + IEND
        fclose(file);
        SLP_DEBUG(INVALID_FILE);
        image.pixels = NULL;
        return image;
    }

    ret = fseek(file, 0, SEEK_SET);
    if (ret != 0) {
        fclose(file);
        SLP_DEBUG(FILE_ERR);
        image.pixels = NULL;
        return image;
    }

    uint8_t worker[33];

    if (fread(worker, 1, 33, file) < 33) {
        fclose(file);
        SLP_DEBUG(FILE_ERR);
        image.pixels = NULL;
        return image;
    }

    uint32_t crc_ = crc32(0, worker + 12, 4);
    crc_ = crc32(crc_, worker + 16, 13);

    if (big_edian_u64(worker) != PNG_SIGNATURE ||
        big_edian_u32(worker + 8) != 13 ||
        big_edian_u32(worker + 12) != IHDR ||
        big_edian_u32(worker + 29) != crc_)
    {
        fclose(file);
        SLP_DEBUG(INVALID_FILE);
        image.pixels = NULL;
        return image;
    }

    const uint32_t width = image.width = big_edian_u32(worker + 16);
    const uint32_t height = image.height = big_edian_u32(worker + 20);
    const int bit_depth = image.bit_depth = worker[24];
    const int color_type = worker[25];
    const int channels = image.channels = slp_png_get_channels(color_type, bit_depth);
    const int compression_method = worker[26];
    const int filter_method = worker[27];
    const int interlace_method = worker[28];

    if (compression_method != 0 || filter_method != 0 || interlace_method != 0 || channels == 0) {
        fclose(file);
        SLP_DEBUG(INVALID_FILE);
        image.pixels = NULL;
        return image;
    }

    if (color_type == 3) {
        assert(channels == 4);
    }

    const size_t image_size = image.image_size = height * div_ceil((size_t)width * channels * ((color_type == 3) ? 8 : bit_depth), 8);
    const size_t allocated_size = image.allocated_size = SLP_ALIGN_SIZE(image_size);

    image.pixels = (uint8_t*)SLP_ALIGNED_ALLOC(allocated_size);
    if (image.pixels == NULL) {
        fclose(file);
        SLP_DEBUG(ALLOC_ERR);
        image.pixels = NULL;
        return image;
    }

    ret = slp_png_decode(&image, file, file_size, color_type);
    if (ret != 0) {
        fclose(file);
        SLP_ALIGNED_FREE(image.pixels);
        SLP_DEBUG(ret);
        image.pixels = NULL;
        return image;
    }
    image.bit_depth = (color_type == 3) ? 8 : image.bit_depth;

    fclose(file);
    return image;
}

static inline int slp_png_get_channels(const int color_type, const int bit_depth) {
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
        default: return 0;
    }
    assert(false);
}

static inline int slp_png_decode(slp_image_t* restrict image, FILE* restrict file, const size_t file_size, const int color_type) {
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
                                    if (slp_png_defilter(out + i * (bpr + 1), scanline, bpp, bpr, imtrker) != 0) {
                                        inflateEnd(&strm);
                                        Err(INVALID_FILE);
                                    }

                                    if (is_color_type3) {
                                        slp_png_colortype3_unpack(scanline[1], image, bpr, imtrker);

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
                        if (slp_png_defilter(out + i * (bpr + 1), scanline, bpp, bpr, imtrker) != 0) {
                            inflateEnd(&strm);
                            Err(INVALID_FILE);
                        }

                        if (is_color_type3) {
                            slp_png_colortype3_unpack(scanline[1], image, bpr, imtrker);

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
        slp_png_index_u32_to_RGBA(image, palette);

#undef Err
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

// scanline[0] is up, scanline[1] is output
static inline int slp_png_defilter(uint8_t* restrict buffer, uint8_t* restrict* restrict scanline, const size_t bpp, const size_t bpr, const size_t imtrker) {
    assert(bpp < bpr);
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
            if (imtrker == 0)
                SLP_MEMCPY(scanline[1], buffer, bpr);
            else {
                size_t i = 0;
                #ifdef __AVX2__
                for (; i + 32 <= bpr; i += 32) {
                    const __m256i raw = _mm256_loadu_si256((const __m256i*)(buffer + i));
                    const __m256i up = _mm256_loadu_si256((const __m256i*)(scanline[0] + i));
                    _mm256_storeu_si256((__m256i*)(scanline[1] + i), _mm256_add_epi8(raw, up));
                }
                #endif
                #ifdef __SSE2__
                for (; i + 16 <= bpr; i += 16) {
                    const __m128i raw = _mm_loadu_si128((const __m128i*)(buffer + i));
                    const __m128i up = _mm_loadu_si128((const __m128i*)(scanline[0] + i));
                    _mm_storeu_si128((__m128i*)(scanline[1] + i), _mm_add_epi8(raw, up));
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
            } else {
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
            } else {
                size_t i = 0;
                for (; i < bpp; i++) scanline[1][i] = buffer[i] + scanline[0][i];
                for (; i < bpr; i++) {
                    const int a = scanline[1][i - bpp];
                    const int b = scanline[0][i];
                    const int c = scanline[0][i - bpp];

                    const int p = a + b - c;
                    const int pa = abs(p - a);
                    const int pb = abs(p - b);
                    const int pc = abs(p - c);

                    uint8_t d = (pb <= pc) ? b : c;
                    d = (pa <= pb && pa <= pc) ? a : d;

                    scanline[1][i] = buffer[i] + d;
                }
            }
            break;
        }
        default: return 1;
    }
    return 0;
}

static inline void slp_png_colortype3_unpack(uint8_t* restrict buffer, slp_image_t* restrict image, const size_t bpr, const size_t imtrker) {
    assert(image->channels == 4);
    uint8_t* src = buffer;
    uint8_t* dest = image->pixels + imtrker * image->width * image->channels;

#ifdef __SSE2__
// convert into u32 and store
#define store(indx, src) do {                                                                       \
        const __m128i zeroes = _mm_setzero_si128();                                                 \
        const __m128i x0 = _mm_unpacklo_epi8(src, zeroes);                                          \
        const __m128i x1 = _mm_unpackhi_epi8(src, zeroes);                                          \
        const __m128i p0 = _mm_unpacklo_epi16(x0, zeroes);                                          \
        const __m128i p1 = _mm_unpackhi_epi16(x0, zeroes);                                          \
        const __m128i p2 = _mm_unpacklo_epi16(x1, zeroes);                                          \
        const __m128i p3 = _mm_unpackhi_epi16(x1, zeroes);                                          \
        _mm_storeu_si128((__m128i*)(dest + i * (32 / image->bit_depth) + (indx * 4 + 0) * 16), p0); \
        _mm_storeu_si128((__m128i*)(dest + i * (32 / image->bit_depth) + (indx * 4 + 1) * 16), p1); \
        _mm_storeu_si128((__m128i*)(dest + i * (32 / image->bit_depth) + (indx * 4 + 2) * 16), p2); \
        _mm_storeu_si128((__m128i*)(dest + i * (32 / image->bit_depth) + (indx * 4 + 3) * 16), p3); \
    } while (0)
#endif

    size_t i = 0;
    switch (image->bit_depth) {
        case 1: {
            #ifdef __SSE2__
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

                store(0, a01234567lo_lo_lo);
                store(1, a01234567lo_lo_hi);
                store(2, a01234567lo_hi_lo);
                store(3, a01234567lo_hi_hi);
                store(4, a01234567hi_lo_lo);
                store(5, a01234567hi_lo_hi);
                store(6, a01234567hi_hi_lo);
                store(7, a01234567hi_hi_hi);
            }
            #endif
            for (; i < bpr; i++) {
                dest[i * 32 + 0 * 4] = (src[i] >> 7) & 1;
                dest[i * 32 + 1 * 4] = (src[i] >> 6) & 1;
                dest[i * 32 + 2 * 4] = (src[i] >> 5) & 1;
                dest[i * 32 + 3 * 4] = (src[i] >> 4) & 1;
                dest[i * 32 + 4 * 4] = (src[i] >> 3) & 1;
                dest[i * 32 + 5 * 4] = (src[i] >> 2) & 1;
                dest[i * 32 + 6 * 4] = (src[i] >> 1) & 1;
                dest[i * 32 + 7 * 4] = (src[i] >> 0) & 1;
            }
            break;
        }
        case 2: {
            #ifdef __SSE2__
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

                store(0, in0123lo_lo);
                store(1, in0123lo_hi);
                store(2, in0123hi_lo);
                store(3, in0123hi_hi);
            }
            #endif
            for (; i < bpr; i++) {
                dest[i * 16 + 0 * 4] = (src[i] >> 6) & 3;
                dest[i * 16 + 1 * 4] = (src[i] >> 4) & 3;
                dest[i * 16 + 2 * 4] = (src[i] >> 2) & 3;
                dest[i * 16 + 3 * 4] = (src[i] >> 0) & 3;
            }
            break;
        }
        case 4: {
            #ifdef __SSE2__
            for (; i + 16 <= bpr; i += 16) {
                const __m128i in = _mm_loadu_si128((const __m128i*)(src + i));

                const __m128i in0 = _mm_and_si128(_mm_srli_epi64(in, 4), _mm_set1_epi8(0x0F));
                const __m128i in1 = _mm_and_si128(_mm_srli_epi64(in, 0), _mm_set1_epi8(0x0F));

                const __m128i out_lo = _mm_unpacklo_epi8(in0, in1);
                const __m128i out_hi = _mm_unpackhi_epi8(in0, in1);

                store(0, out_lo);
                store(1, out_hi);
            }
            #endif
            for (; i < bpr; i++) {
                dest[i * 8 + 0 * 4] = (src[i] >> 4) & 0x0F;
                dest[i * 8 + 1 * 4] = (src[i] >> 0) & 0x0F;
            }
            break;
        }
        case 8: {
            #ifdef __SSE2__
            for (; i + 16 <= bpr; i += 16) store(0, _mm_loadu_si128((const __m128i*)(src + i)));
            #endif
            for (; i < bpr; i++) dest[i * 4] = src[i];
            break;
        }
        default: assert(false);
    }

#undef store
}

static inline void slp_png_index_u32_to_RGBA(slp_image_t* restrict image, const uint8_t* restrict palette) {
    assert(image->channels == 4);
    for (size_t i = 0; i + image->channels <= image->image_size; i += image->channels) {
        int index = image->pixels[i] * image->channels;
        for (size_t k = 0; k < image->channels; k++) image->pixels[i + k] = palette[index + k];
    }
}

void slp_image_destroy(slp_image_t* image) {
    if (image != NULL) {
        if (image->pixels != NULL)
            SLP_ALIGNED_FREE(image->pixels);
        SLP_MEMSET(image, 0, sizeof(*image));
    }
}
