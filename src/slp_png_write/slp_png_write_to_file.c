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
#include <stdalign.h>
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

// constants
#define COMPRESSION_LEVEL 6
#define CHUNK 65536  // sizeof 1 IDAT chunk

// helpers
static uint8_t slp_get_color_type(const uint8_t channels);
static int slp_png_encode(slp_image_t* restrict image, FILE* restrict file);
static void slp_png_filter(uint8_t* restrict image_buffer, int8_t* restrict* restrict filter_buffers, uint64_t* restrict filter_scores, const size_t i, const size_t bpr, const size_t bpp);

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

    for (size_t i = 0; i < height; i++)
    {
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

#ifdef __SSE2__
static inline __m128i _mm_abs_epi16_compat(__m128i v) {
    #ifdef __SSSE3__
    return _mm_abs_epi16(v);
    #else
    __m128i mask = _mm_srai_epi16(v, 15);  // srai will shift in 1 or 0 depends on the msbit
    v = _mm_xor_si128(v, mask);            // do bit flips if signed
    return _mm_sub_epi16(v, mask);         // this is add 1 if signed, for signed number all1 = -1 so --1 = +1
    #endif
}

static inline __m128i _mm_abs_epi8_compat(__m128i v) {
    #ifdef __SSSE3__
    return _mm_abs_epi8(v);
    #else
    __m128i mask = _mm_cmpgt_epi8(_mm_setzero_si128(), v);
    v = _mm_xor_si128(v, mask);
    return _mm_sub_epi8(v, mask);
    #endif
}

// assume mask is -1 for true and 0 for false
static inline __m128i _mm_blendv_epi8_compat(__m128i V1, __m128i V2, __m128i M) {
    #ifdef __SSE4_1__
    return _mm_blendv_epi8(V1, V2, M);
    #else
    return _mm_or_si128(_mm_andnot_si128(M, V1), _mm_and_si128(M, V2));
    #endif
}

static inline __m128i _mm_srli_epi8(__m128i v, const int count) {
    const __m128i mask = _mm_xor_si128(_mm_set1_epi8((1ul << count) - 1), _mm_set1_epi8(-1));
    v = _mm_and_si128(v, mask);
    return _mm_srli_epi64(v, count);
}

static inline __m256i _mm256_srli_epi8(__m256i v, const int count) {
    const __m256i mask = _mm256_xor_si256(_mm256_set1_epi8((1ul << count) - 1), _mm256_set1_epi8(-1));
    v = _mm256_and_si256(v, mask);
    return _mm256_srli_epi64(v, count);
}

static inline __m256i _mm256_avg(const __m256i a, const __m256i b) {
    // average of two integers without converting to a wider bit-width
    return _mm256_add_epi8(_mm256_and_si256(a, b), _mm256_srli_epi8(_mm256_xor_si256(a, b), 1));
}

static inline __m128i _mm_avg(const __m128i a, const __m128i b) {
    // average of two integers without converting to a wider bit-width
    return _mm_add_epi8(_mm_and_si128(a, b), _mm_srli_epi8(_mm_xor_si128(a, b), 1));
}

#define _mm_cmple_epu8(a, b) _mm_cmpeq_epi8(_mm_min_epu8(a, b), a)
#define _mm256_cmple_epu8(a, b) _mm256_cmpeq_epi8(_mm256_min_epu8(a, b), a)

static inline __m256i _mm256_paeth(const __m256i a, const __m256i b, const __m256i c) {
    // fpnge
    // https://www.lucaversari.it/FJXL_and_FPNGE.pdf
    const __m256i max_bc = _mm256_max_epu8(b, c);
    const __m256i min_bc = _mm256_min_epu8(b, c);

    const __m256i max_ac = _mm256_max_epu8(a, c);
    const __m256i min_ac = _mm256_min_epu8(a, c);

    const __m256i pa = _mm256_sub_epi8(max_bc, min_bc);
    const __m256i pb = _mm256_sub_epi8(max_ac, min_ac);

    const __m256i c_le_a = _mm256_cmpeq_epi8(min_ac, c);
    const __m256i b_le_c = _mm256_cmpeq_epi8(min_bc, b);

    const __m256i a_lt_c = _mm256_xor_si256(c_le_a, _mm256_set1_epi8(-1));  // !(c <= a)
    const __m256i c_lt_b = _mm256_xor_si256(b_le_c, _mm256_set1_epi8(-1));  // !(b <= c)

    const __m256i pc = _mm256_blendv_epi8(
        _mm256_set1_epi8(-1),
        _mm256_sub_epi8(_mm256_max_epu8(pa, pb), _mm256_min_epu8(pa, pb)),  //
        _mm256_cmpeq_epi8(a_lt_c, c_lt_b)                                   //
    );

    const __m256i pa_le_pb = _mm256_cmple_epu8(pa, pb);
    const __m256i pa_le_pc = _mm256_cmple_epu8(pa, pc);

    const __m256i cond1 = _mm256_and_si256(pa_le_pb, pa_le_pc);
    const __m256i cond2 = _mm256_cmple_epu8(pb, pc);

    __m256i d = _mm256_blendv_epi8(c, b, cond2);
    return _mm256_blendv_epi8(d, a, cond1);
}

static inline __m128i _mm_paeth(const __m128i a, const __m128i b, const __m128i c) {
    // fpnge
    // https://www.lucaversari.it/FJXL_and_FPNGE.pdf
    const __m128i pa = _mm_sub_epi8(_mm_max_epu8(b, c), _mm_min_epu8(b, c));
    const __m128i pb = _mm_sub_epi8(_mm_max_epu8(a, c), _mm_min_epu8(a, c));

    const __m128i a_lt_c = _mm_xor_si128(_mm_cmple_epu8(c, a), _mm_set1_epi8(-1));  // !(c <= a)
    const __m128i c_lt_b = _mm_xor_si128(_mm_cmple_epu8(b, c), _mm_set1_epi8(-1));  // !(b <= c)

    const __m128i pc = _mm_blendv_epi8_compat(
        _mm_set1_epi8(-1),
        _mm_sub_epi8(_mm_max_epu8(pa, pb), _mm_min_epu8(pa, pb)),  //
        _mm_cmpeq_epi8(a_lt_c, c_lt_b)                             //
    );

    const __m128i pa_le_pb = _mm_cmple_epu8(pa, pb);
    const __m128i pa_le_pc = _mm_cmple_epu8(pa, pc);

    const __m128i cond1 = _mm_and_si128(pa_le_pb, pa_le_pc);
    const __m128i cond2 = _mm_cmple_epu8(pb, pc);

    __m128i d = _mm_blendv_epi8_compat(c, b, cond2);
    return _mm_blendv_epi8(d, a, cond1);
}
#endif

static inline void slp_png_filter(uint8_t* restrict image_buffer, int8_t* restrict* restrict filter_buffers, uint64_t* restrict filter_scores, const size_t i, const size_t bpr, const size_t bpp) {
    if (i == 0)
    {
        uint8_t* src = image_buffer;
        for (size_t j = 0; j < bpp; j++) filter_buffers[1][j + 1] = src[j];
        for (size_t j = bpp; j < bpr; j++) filter_buffers[1][j + 1] = src[j] - src[j - bpp];
        for (int j = 0; j < 5; j++) filter_scores[j] = 1;
        filter_scores[1] = 0;
    } else
    {
        uint8_t* src = image_buffer + i * bpr;

        size_t j = 0;
        for (; j < bpp; j++) {
            filter_buffers[0][j + 1] = src[j];
            filter_buffers[1][j + 1] = src[j];
            filter_buffers[2][j + 1] = src[j] - src[j - bpr];
            filter_buffers[3][j + 1] = src[j] - (src[j - bpr] >> 1);
            filter_buffers[4][j + 1] = src[j] - src[j - bpr];

            filter_scores[0] += abs(filter_buffers[0][j + 1]);
            filter_scores[1] += abs(filter_buffers[1][j + 1]);
            filter_scores[2] += abs(filter_buffers[2][j + 1]);
            filter_scores[3] += abs(filter_buffers[3][j + 1]);
            filter_scores[4] += abs(filter_buffers[4][j + 1]);
        }

        #ifdef __AVX2__
        {
            __m256i noneSum = _mm256_setzero_si256();
            __m256i subSum = _mm256_setzero_si256();
            __m256i upSum = _mm256_setzero_si256();
            __m256i avgSum = _mm256_setzero_si256();
            __m256i paethSum = _mm256_setzero_si256();
            __m256i zero = _mm256_setzero_si256();

            for (; j + 32 <= bpr; j += 32)
            {
                const __m256i raw = _mm256_loadu_si256((const __m256i*)(src + j));
                const __m256i a = _mm256_loadu_si256((const __m256i*)(src + j - bpp));
                const __m256i b = _mm256_loadu_si256((const __m256i*)(src + j - bpr));
                const __m256i c = _mm256_loadu_si256((const __m256i*)(src + j - bpr - bpp));

                const __m256i sub = _mm256_sub_epi8(raw, a);
                const __m256i up = _mm256_sub_epi8(raw, b);
                const __m256i avg = _mm256_sub_epi8(raw, _mm256_avg(a, b));
                const __m256i paeth = _mm256_sub_epi8(raw, _mm256_paeth(a, b, c));

                noneSum = _mm256_add_epi64(noneSum, _mm256_sad_epu8(_mm256_abs_epi8(raw), zero));
                subSum = _mm256_add_epi64(subSum, _mm256_sad_epu8(_mm256_abs_epi8(sub), zero));
                upSum = _mm256_add_epi64(upSum, _mm256_sad_epu8(_mm256_abs_epi8(up), zero));
                avgSum = _mm256_add_epi64(avgSum, _mm256_sad_epu8(_mm256_abs_epi8(avg), zero));
                paethSum = _mm256_add_epi64(paethSum, _mm256_sad_epu8(_mm256_abs_epi8(paeth), zero));

                _mm256_storeu_si256((__m256i*)(filter_buffers[0] + j + 1), raw);
                _mm256_storeu_si256((__m256i*)(filter_buffers[1] + j + 1), sub);
                _mm256_storeu_si256((__m256i*)(filter_buffers[2] + j + 1), up);
                _mm256_storeu_si256((__m256i*)(filter_buffers[3] + j + 1), avg);
                _mm256_storeu_si256((__m256i*)(filter_buffers[4] + j + 1), paeth);
            }

            alignas(32) uint64_t tmp0[4];
            alignas(32) uint64_t tmp1[4];
            alignas(32) uint64_t tmp2[4];
            alignas(32) uint64_t tmp3[4];
            alignas(32) uint64_t tmp4[4];

            _mm256_store_si256((__m256i*)tmp0, noneSum);
            _mm256_store_si256((__m256i*)tmp1, subSum);
            _mm256_store_si256((__m256i*)tmp2, upSum);
            _mm256_store_si256((__m256i*)tmp3, avgSum);
            _mm256_store_si256((__m256i*)tmp4, paethSum);

            for (unsigned int u = 0; u < 4; u++) {
                filter_scores[0] += tmp0[u];
                filter_scores[1] += tmp1[u];
                filter_scores[2] += tmp2[u];
                filter_scores[3] += tmp3[u];
                filter_scores[4] += tmp4[u];
            }
        }
        #endif
        #ifdef __SSE2__
        {
            __m128i noneSum = _mm_setzero_si128();
            __m128i subSum = _mm_setzero_si128();
            __m128i upSum = _mm_setzero_si128();
            __m128i avgSum = _mm_setzero_si128();
            __m128i paethSum = _mm_setzero_si128();
            __m128i zero = _mm_setzero_si128();

            for (; j + 16 <= bpr; j += 16)
            {
                const __m128i raw = _mm_loadu_si128((const __m128i*)(src + j));
                const __m128i a = _mm_loadu_si128((const __m128i*)(src + j - bpp));
                const __m128i b = _mm_loadu_si128((const __m128i*)(src + j - bpr));
                const __m128i c = _mm_loadu_si128((const __m128i*)(src + j - bpr - bpp));

                const __m128i sub = _mm_sub_epi8(raw, a);
                const __m128i up = _mm_sub_epi8(raw, b);
                const __m128i avg = _mm_sub_epi8(raw, _mm_avg(a, b));
                const __m128i paeth = _mm_sub_epi8(raw, _mm_paeth(a, b, c));

                noneSum = _mm_add_epi64(noneSum, _mm_sad_epu8(_mm_abs_epi8_compat(raw), zero));
                subSum = _mm_add_epi64(subSum, _mm_sad_epu8(_mm_abs_epi8_compat(sub), zero));
                upSum = _mm_add_epi64(upSum, _mm_sad_epu8(_mm_abs_epi8_compat(up), zero));
                avgSum = _mm_add_epi64(avgSum, _mm_sad_epu8(_mm_abs_epi8_compat(avg), zero));
                paethSum = _mm_add_epi64(paethSum, _mm_sad_epu8(_mm_abs_epi8_compat(paeth), zero));

                _mm_storeu_si128((__m128i*)(filter_buffers[0] + j + 1), raw);
                _mm_storeu_si128((__m128i*)(filter_buffers[1] + j + 1), sub);
                _mm_storeu_si128((__m128i*)(filter_buffers[2] + j + 1), up);
                _mm_storeu_si128((__m128i*)(filter_buffers[3] + j + 1), avg);
                _mm_storeu_si128((__m128i*)(filter_buffers[4] + j + 1), paeth);
            }

            alignas(16) uint64_t tmp0[2];
            alignas(16) uint64_t tmp1[2];
            alignas(16) uint64_t tmp2[2];
            alignas(16) uint64_t tmp3[2];
            alignas(16) uint64_t tmp4[2];

            _mm_store_si128((__m128i*)tmp0, noneSum);
            _mm_store_si128((__m128i*)tmp1, subSum);
            _mm_store_si128((__m128i*)tmp2, upSum);
            _mm_store_si128((__m128i*)tmp3, avgSum);
            _mm_store_si128((__m128i*)tmp4, paethSum);

            for (unsigned int u = 0; u < 2; u++) {
                filter_scores[0] += tmp0[u];
                filter_scores[1] += tmp1[u];
                filter_scores[2] += tmp2[u];
                filter_scores[3] += tmp3[u];
                filter_scores[4] += tmp4[u];
            }
        }
        #endif

        for (; j < bpr; j++)
        {
            const int p = src[j - bpp] + src[j - bpr] - src[j - bpr - bpp];
            const int pa = abs(p - src[j - bpp]);
            const int pb = abs(p - src[j - bpr]);
            const int pc = abs(p - src[j - bpr - bpp]);

            uint8_t d = (pb <= pc) ? src[j - bpr] : src[j - bpr - bpp];
            d = (pa <= pb && pa <= pc) ? src[j - bpp] : d;

            filter_buffers[0][j + 1] = src[j];
            filter_buffers[1][j + 1] = src[j] - src[j - bpp];
            filter_buffers[2][j + 1] = src[j] - src[j - bpr];
            filter_buffers[3][j + 1] = src[j] - ((src[j - bpp] + src[j - bpr]) / 2);
            filter_buffers[4][j + 1] = src[j] - d;

            filter_scores[0] += abs(filter_buffers[0][j + 1]);
            filter_scores[1] += abs(filter_buffers[1][j + 1]);
            filter_scores[2] += abs(filter_buffers[2][j + 1]);
            filter_scores[3] += abs(filter_buffers[3][j + 1]);
            filter_scores[4] += abs(filter_buffers[4][j + 1]);
        }
    }
}
