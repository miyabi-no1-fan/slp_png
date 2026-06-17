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
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdalign.h>
#include <string.h>
#include <zlib.h>

#if defined(__i386__) || defined(__x86_64__)
#include <immintrin.h>
#endif

// use to write IHDR
typedef struct __attribute__((packed)) {
    uint32_t width;
    uint32_t height;
    uint8_t bit_depth;
    uint8_t color_type;
    uint8_t compression_method;
    uint8_t filter_method;
    uint8_t interlace_method;
}ihdr_t;

// helper
// functions
static uint8_t slp_get_color_type(const uint8_t channels);
static int slp_png_encode(slp_image_t* restrict image, FILE* restrict file);
static void slp_png_filter(uint8_t* restrict image_buffer, int8_t* restrict* restrict filter_buffers, uint64_t* restrict filter_scores, const size_t i, const size_t bpr, const size_t bpp);

#ifdef __SSE2__
__m128i _mm_abs_epi16_sse2(__m128i v) {
    __m128i mask = _mm_srai_epi16(v, 15); // srai will shift in 1 or 0 depends on the msbit
    v = _mm_xor_si128(v, mask); // do bit flips if signed
    return _mm_sub_epi16(v, mask); // this is add 1 if signed, for signed number all1 = -1 so --1 = +1
}
#endif

// constants
enum {
    level = 6, // level of compression
    CHUNK = 65536 // sizeof 1 IDAT chunk
};



int slp_png_write(slp_image_t image, const char* path) {
    if (image.height == 0 || image.width == 0 || image.channels == 0) return 2;
    switch (image.bit_depth) {
        case 1: break;
        case 2: break;
        case 4: break;
        case 8: break;
        case 16: break;
        default: return 2;
    }

    const uint16_t random_value_for_edian_test = 1;
    const bool is_little_edian = *(uint8_t*)(&random_value_for_edian_test);
    const uint64_t PNG_SIGNATURE = big_edian_u64_in_mem(0x89504E470D0A1A0Aull, is_little_edian);

    FILE *file = fopen(path, "wb");
    if (file == NULL) return 1;

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
        return 2;
    }

    uint32_t crc = crc32(0xA8A1AE0A, (unsigned char*)(&header), 13);
    crc = big_edian_u32_in_mem(crc, is_little_edian);
    const uint32_t data_len = big_edian_u32_in_mem(13, is_little_edian);

    if (fwrite(&PNG_SIGNATURE, 1,  8, file) != 8  ||
        fwrite(&data_len     , 1,  4, file) != 4  ||
        fwrite("IHDR"        , 1,  4, file) != 4  ||
        fwrite(&header       , 1, 13, file) != 13 ||
        fwrite(&crc          , 1,  4, file) != 4)
    {
        fclose(file);
        return 1;
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

    const uint16_t random_value_for_edian_test = 1;
    const bool is_little_edian = *(uint8_t*)(&random_value_for_edian_test);

    const size_t width = image->width;
    const size_t height = image->height;
    const size_t channels = image->channels;
    const size_t bit_depth = image->bit_depth;

    const size_t bpp = channels * (1 + (bit_depth == 16));
    const size_t bpr = div_round_up(width * channels * bit_depth, 8);// bytes per row

    size_t have = 0;
    size_t data_len = 0;

    uint8_t* mem_ptr = NULL;

    mem_ptr = (uint8_t*)SLP_ALIGNED_ALLOC(SLP_ALIGN_SIZE(bpr + 1)*5 + CHUNK+12);
    if (mem_ptr == NULL) {
        return_code = -1;
        goto cleanup;
    }
    int8_t* filter_buffers[5];
    filter_buffers[0] = (int8_t*)mem_ptr + SLP_ALIGN_SIZE(bpr + 1)*0;
    filter_buffers[1] = (int8_t*)mem_ptr + SLP_ALIGN_SIZE(bpr + 1)*1;
    filter_buffers[2] = (int8_t*)mem_ptr + SLP_ALIGN_SIZE(bpr + 1)*2;
    filter_buffers[3] = (int8_t*)mem_ptr + SLP_ALIGN_SIZE(bpr + 1)*3;
    filter_buffers[4] = (int8_t*)mem_ptr + SLP_ALIGN_SIZE(bpr + 1)*4;
    uint8_t* out = mem_ptr + SLP_ALIGN_SIZE(bpr + 1)*5;

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
    int ret = deflateInit2(&strm, level, Z_DEFLATED, 15, 8, Z_FILTERED);
    if (ret != Z_OK) {
        return_code = 3;
        goto cleanup;
    }
    strm.avail_out = CHUNK;
    for (size_t i = 0; i < height; i++)
    {
        uint64_t filter_scores[5] = {0};
        slp_png_filter(image->buffer, filter_buffers, filter_scores, i, bpr, bpp);
        unsigned int filter_type = 0;
        for (unsigned int i = 0; i < 5; i++) filter_type = (filter_scores[i] < filter_scores[filter_type]) ? (i) : (filter_type);

        strm.next_in = (uint8_t*)filter_buffers[filter_type];
        strm.avail_in = bpr + 1;
        do {
            strm.next_out = out + 8 + have;
            ret = deflate(&strm, Z_NO_FLUSH);
            if (ret != Z_OK) {
                return_code = 3;
                deflateEnd(&strm);
                goto cleanup;
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
                    return_code = 1;
                    deflateEnd(&strm);
                    goto cleanup;
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
            return_code = 3;
            deflateEnd(&strm);
            goto cleanup;
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
                return_code = 1;
                deflateEnd(&strm);
                goto cleanup;
            }
            strm.avail_out = CHUNK;
            have = 0;
        }
    } while (ret != Z_STREAM_END);

    data_len = (uint32_t)(have);
    data_len = big_edian_u32_in_mem(data_len, is_little_edian);
    SLP_MEMCPY(out, &data_len, 4);
    uint32_t crc_ = crc32(0, out + 4, 4 + have);
    crc_ = big_edian_u32_in_mem(crc_, is_little_edian);
    SLP_MEMCPY(out + 8 + have, &crc_, 4);
    if (fwrite(out, 1, 8 + have + 4, file) != 8 + have + 4) {
        return_code = 1;
        deflateEnd(&strm);
        goto cleanup;
    }
    deflateEnd(&strm);
    // finish writting IDAT


    // CHUNK AFTER IDAT STAY HERE


    // writting IEND
    const uint8_t IENDsig[12] = {0, 0, 0, 0, 'I', 'E', 'N', 'D', 0xAE, 0x42, 0x60, 0x82};
    if (fwrite(IENDsig, 1, 12, file) != 12) {
        return_code = 1;
        goto cleanup;
    }
    // finish IEND

cleanup:
    SLP_ALIGNED_FREE(mem_ptr);
    return return_code;
}

static inline void slp_png_filter(uint8_t* restrict image_buffer, int8_t* restrict* restrict filter_buffers, uint64_t* restrict filter_scores, const size_t i, const size_t bpr, const size_t bpp) {
    if (i == 0)
    {
        uint8_t *raw = image_buffer;
        for (size_t j =   0; j < bpp; j++) filter_buffers[1][j+1] = raw[j];
        for (size_t j = bpp; j < bpr; j++) filter_buffers[1][j+1] = raw[j] - raw[j-bpp];
        for (int j = 0; j < 5; j++) filter_scores[j] = 1000;
        filter_scores[1] = 0;
    }
    else
    {
        uint8_t *raw = image_buffer + i * bpr;

        size_t j = 0;
        for (; j < bpp; j++) {
            filter_buffers[0][j+1] = raw[j];
            filter_buffers[1][j+1] = raw[j];
            filter_buffers[2][j+1] = raw[j] - raw[j - bpr];
            filter_buffers[3][j+1] = raw[j] - (raw[j - bpr]>>1);
            filter_buffers[4][j+1] = raw[j] - raw[j - bpr];

            filter_scores[0] += abs(filter_buffers[0][j+1]);
            filter_scores[1] += abs(filter_buffers[1][j+1]);
            filter_scores[2] += abs(filter_buffers[2][j+1]);
            filter_scores[3] += abs(filter_buffers[3][j+1]);
            filter_scores[4] += abs(filter_buffers[4][j+1]);
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
                const __m256i r  = _mm256_loadu_si256((const __m256i*)(raw + j));
                const __m256i va = _mm256_loadu_si256((const __m256i*)(raw + j - bpp));
                const __m256i vb = _mm256_loadu_si256((const __m256i*)(raw + j - bpr));
                const __m256i vc = _mm256_loadu_si256((const __m256i*)(raw + j - bpr - bpp));


                const __m256i va_lo = _mm256_unpacklo_epi8(va, zero);
                const __m256i va_hi = _mm256_unpackhi_epi8(va, zero);

                const __m256i vb_lo = _mm256_unpacklo_epi8(vb, zero);
                const __m256i vb_hi = _mm256_unpackhi_epi8(vb, zero);

                const __m256i vc_lo = _mm256_unpacklo_epi8(vc, zero);
                const __m256i vc_hi = _mm256_unpackhi_epi8(vc, zero);


                const __m256i p_lo = _mm256_add_epi16(va_lo, _mm256_sub_epi16(vb_lo, vc_lo));
                const __m256i p_hi = _mm256_add_epi16(va_hi, _mm256_sub_epi16(vb_hi, vc_hi));

                const __m256i pa_lo = _mm256_abs_epi16(_mm256_sub_epi16(p_lo, va_lo));
                const __m256i pa_hi = _mm256_abs_epi16(_mm256_sub_epi16(p_hi, va_hi));

                const __m256i pb_lo = _mm256_abs_epi16(_mm256_sub_epi16(p_lo, vb_lo));
                const __m256i pb_hi = _mm256_abs_epi16(_mm256_sub_epi16(p_hi, vb_hi));

                const __m256i pc_lo = _mm256_abs_epi16(_mm256_sub_epi16(p_lo, vc_lo));
                const __m256i pc_hi = _mm256_abs_epi16(_mm256_sub_epi16(p_hi, vc_hi));


                const __m256i not_pa_le_pb_lo = _mm256_cmpgt_epi16(pa_lo, pb_lo);
                const __m256i not_pa_le_pb_hi = _mm256_cmpgt_epi16(pa_hi, pb_hi);

                const __m256i not_pa_le_pc_lo = _mm256_cmpgt_epi16(pa_lo, pc_lo);
                const __m256i not_pa_le_pc_hi = _mm256_cmpgt_epi16(pa_hi, pc_hi);

                const __m256i not_cond1_lo = _mm256_or_si256(not_pa_le_pb_lo, not_pa_le_pc_lo);
                const __m256i not_cond1_hi = _mm256_or_si256(not_pa_le_pb_hi, not_pa_le_pc_hi);

                const __m256i not_cond2_lo = _mm256_cmpgt_epi16(pb_lo, pc_lo);
                const __m256i not_cond2_hi = _mm256_cmpgt_epi16(pb_hi, pc_hi);

                __m256i d_lo = _mm256_blendv_epi8(vb_lo, vc_lo, not_cond2_lo);
                __m256i d_hi = _mm256_blendv_epi8(vb_hi, vc_hi, not_cond2_hi);

                d_lo = _mm256_blendv_epi8(va_lo, d_lo, not_cond1_lo);
                d_hi = _mm256_blendv_epi8(va_hi, d_hi, not_cond1_hi);

                const __m256i d = _mm256_packus_epi16(d_lo, d_hi);


                const __m256i tavg_lo = _mm256_srli_epi16(_mm256_add_epi16(va_lo, vb_lo), 1);
                const __m256i tavg_hi = _mm256_srli_epi16(_mm256_add_epi16(va_hi, vb_hi), 1);
                const __m256i tavg = _mm256_packus_epi16(tavg_lo, tavg_hi);


                const __m256i vsub = _mm256_sub_epi8(r, va);
                const __m256i vup = _mm256_sub_epi8(r, vb);
                const __m256i vavg = _mm256_sub_epi8(r, tavg);
                const __m256i vpaeth = _mm256_sub_epi8(r, d);


                noneSum = _mm256_add_epi64(noneSum, _mm256_sad_epu8(r, zero));
                subSum = _mm256_add_epi64(subSum, _mm256_sad_epu8(r, va));
                upSum = _mm256_add_epi64(upSum, _mm256_sad_epu8(r, vb));
                avgSum = _mm256_add_epi64(avgSum, _mm256_sad_epu8(r, tavg));
                paethSum = _mm256_add_epi64(paethSum, _mm256_sad_epu8(r, d));


                _mm256_storeu_si256((__m256i*)(filter_buffers[0] + j + 1), r);
                _mm256_storeu_si256((__m256i*)(filter_buffers[1] + j + 1), vsub);
                _mm256_storeu_si256((__m256i*)(filter_buffers[2] + j + 1), vup);
                _mm256_storeu_si256((__m256i*)(filter_buffers[3] + j + 1), vavg);
                _mm256_storeu_si256((__m256i*)(filter_buffers[4] + j + 1), vpaeth);
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
                const __m128i r  = _mm_loadu_si128((const __m128i*)(raw + j));
                const __m128i va = _mm_loadu_si128((const __m128i*)(raw + j - bpp));
                const __m128i vb = _mm_loadu_si128((const __m128i*)(raw + j - bpr));
                const __m128i vc = _mm_loadu_si128((const __m128i*)(raw + j - bpr - bpp));

                const __m128i va_lo = _mm_unpacklo_epi8(va, zero);
                const __m128i va_hi = _mm_unpackhi_epi8(va, zero);
                const __m128i vb_lo = _mm_unpacklo_epi8(vb, zero);
                const __m128i vb_hi = _mm_unpackhi_epi8(vb, zero);
                const __m128i vc_lo = _mm_unpacklo_epi8(vc, zero);
                const __m128i vc_hi = _mm_unpackhi_epi8(vc, zero);

                const __m128i p_lo = _mm_add_epi16(va_lo, _mm_sub_epi16(vb_lo, vc_lo));
                const __m128i p_hi = _mm_add_epi16(va_hi, _mm_sub_epi16(vb_hi, vc_hi));
                #ifdef __SSSE3__
                const __m128i pa_lo = _mm_abs_epi16(_mm_sub_epi16(p_lo, va_lo));
                const __m128i pa_hi = _mm_abs_epi16(_mm_sub_epi16(p_hi, va_hi));
                const __m128i pb_lo = _mm_abs_epi16(_mm_sub_epi16(p_lo, vb_lo));
                const __m128i pb_hi = _mm_abs_epi16(_mm_sub_epi16(p_hi, vb_hi));
                const __m128i pc_lo = _mm_abs_epi16(_mm_sub_epi16(p_lo, vc_lo));
                const __m128i pc_hi = _mm_abs_epi16(_mm_sub_epi16(p_hi, vc_hi));
                #else
                const __m128i pa_lo = _mm_abs_epi16_sse2(_mm_sub_epi16(p_lo, va_lo));
                const __m128i pa_hi = _mm_abs_epi16_sse2(_mm_sub_epi16(p_hi, va_hi));
                const __m128i pb_lo = _mm_abs_epi16_sse2(_mm_sub_epi16(p_lo, vb_lo));
                const __m128i pb_hi = _mm_abs_epi16_sse2(_mm_sub_epi16(p_hi, vb_hi));
                const __m128i pc_lo = _mm_abs_epi16_sse2(_mm_sub_epi16(p_lo, vc_lo));
                const __m128i pc_hi = _mm_abs_epi16_sse2(_mm_sub_epi16(p_hi, vc_hi));
                #endif

                const __m128i not_pa_le_pb_lo = _mm_cmpgt_epi16(pa_lo, pb_lo);
                const __m128i not_pa_le_pb_hi = _mm_cmpgt_epi16(pa_hi, pb_hi);
                const __m128i not_pa_le_pc_lo = _mm_cmpgt_epi16(pa_lo, pc_lo);
                const __m128i not_pa_le_pc_hi = _mm_cmpgt_epi16(pa_hi, pc_hi);
                const __m128i not_cond1_lo = _mm_or_si128(not_pa_le_pb_lo, not_pa_le_pc_lo);
                const __m128i not_cond1_hi = _mm_or_si128(not_pa_le_pb_hi, not_pa_le_pc_hi);
                const __m128i not_cond2_lo = _mm_cmpgt_epi16(pb_lo, pc_lo);
                const __m128i not_cond2_hi = _mm_cmpgt_epi16(pb_hi, pc_hi);
                #ifdef __SSE4_1__
                __m128i d_lo = _mm_blendv_epi8(vb_lo, vc_lo, not_cond2_lo);
                __m128i d_hi = _mm_blendv_epi8(vb_hi, vc_hi, not_cond2_hi);
                d_lo = _mm_blendv_epi8(va_lo, d_lo, not_cond1_lo);
                d_hi = _mm_blendv_epi8(va_hi, d_hi, not_cond1_hi);
                #else
                __m128i d_lo = _mm_or_si128(_mm_andnot_si128(not_cond2_lo, vb_lo), _mm_and_si128(not_cond2_lo, vc_lo));
                __m128i d_hi = _mm_or_si128(_mm_andnot_si128(not_cond2_hi, vb_hi), _mm_and_si128(not_cond2_hi, vc_hi));
                d_lo = _mm_or_si128(_mm_andnot_si128(not_cond1_lo, va_lo), _mm_and_si128(not_cond1_lo, d_lo));
                d_hi = _mm_or_si128(_mm_andnot_si128(not_cond1_hi, va_hi), _mm_and_si128(not_cond1_hi, d_hi));
                #endif
                const __m128i d = _mm_packus_epi16(d_lo, d_hi);

                const __m128i tavg_lo = _mm_srli_epi16(_mm_add_epi16(va_lo, vb_lo), 1);
                const __m128i tavg_hi = _mm_srli_epi16(_mm_add_epi16(va_hi, vb_hi), 1);
                const __m128i tavg = _mm_packus_epi16(tavg_lo, tavg_hi);

                const __m128i vsub = _mm_sub_epi8(r, va);
                const __m128i vup = _mm_sub_epi8(r, vb);
                const __m128i vavg = _mm_sub_epi8(r, tavg);
                const __m128i vpaeth = _mm_sub_epi8(r, d);

                noneSum = _mm_add_epi64(noneSum, _mm_sad_epu8(r, zero));
                subSum = _mm_add_epi64(subSum, _mm_sad_epu8(r, va));
                upSum = _mm_add_epi64(upSum, _mm_sad_epu8(r, vb));
                avgSum = _mm_add_epi64(avgSum, _mm_sad_epu8(r, tavg));
                paethSum = _mm_add_epi64(paethSum, _mm_sad_epu8(r, d));

                _mm_storeu_si128((__m128i*)(filter_buffers[0] + j + 1), r);
                _mm_storeu_si128((__m128i*)(filter_buffers[1] + j + 1), vsub);
                _mm_storeu_si128((__m128i*)(filter_buffers[2] + j + 1), vup);
                _mm_storeu_si128((__m128i*)(filter_buffers[3] + j + 1), vavg);
                _mm_storeu_si128((__m128i*)(filter_buffers[4] + j + 1), vpaeth);
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
            const int p = raw[j - bpp] + raw[j - bpr] - raw[j - bpr - bpp];
            const int pa = abs(p - raw[j - bpp]);
            const int pb = abs(p - raw[j - bpr]);
            const int pc = abs(p - raw[j - bpr - bpp]);

            uint8_t d = (pb <= pc) ? raw[j - bpr] : raw[j - bpr - bpp];
            d = (pa <= pb && pa <= pc) ? raw[j - bpp] : d;

            filter_buffers[0][j+1] = raw[j];
            filter_buffers[1][j+1] = raw[j] - raw[j - bpp];
            filter_buffers[2][j+1] = raw[j] - raw[j - bpr];
            filter_buffers[3][j+1] = raw[j] - ((raw[j - bpp] + raw[j - bpr]) / 2);
            filter_buffers[4][j+1] = raw[j] - d;

            filter_scores[0] += abs(filter_buffers[0][j+1]);
            filter_scores[1] += abs(filter_buffers[1][j+1]);
            filter_scores[2] += abs(filter_buffers[2][j+1]);
            filter_scores[3] += abs(filter_buffers[3][j+1]);
            filter_scores[4] += abs(filter_buffers[4][j+1]);
        }
    }
}
