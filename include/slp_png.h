#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef SLP_PNG_H
#define SLP_PNG_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct slp_image_t {
    uint8_t* pixels;
    uint32_t height;
    uint32_t width;
    uint32_t channels;
    uint8_t bit_depth;
    size_t image_size;
    size_t allocated_size;
} slp_image_t;

enum SLP_ERROR {
    ALLOC_ERR = -1,
    FILE_ERR = 1,
    INVALID_FILE = 2,   // for slp_png_read
    INVALID_INPUT = 2,  // for slp_png_write
    ZLIB_ERR = 3,
};

slp_image_t slp_png_read(const char* path);
int slp_png_write(slp_image_t image, const char* path);
void slp_image_destroy(slp_image_t* image);

#ifndef SLP_MALLOC
    #define SLP_MALLOC(size) malloc(size)
#endif

#ifndef SLP_CALLOC
    #define SLP_CALLOC(size) calloc(size, 1)
#endif

#ifndef SLP_FREE
    #define SLP_FREE(ptr) free(ptr)
#endif

#ifndef SLP_MEMCPY
    #define SLP_MEMCPY(dest, source, size) memcpy(dest, source, size)
#endif

#ifndef SLP_MEMMOVE
    #define SLP_MEMMOVE(dest, source, size) memmove(dest, source, size)
#endif

#ifndef SLP_MEMSET
    #define SLP_MEMSET(s, c, n) memset(s, c, n)
#endif

#ifndef SLP_USE_ALIGNED_ALLOC
    #if defined(__unix__) || defined(__APPLE__) || defined(_WIN32)
        #define SLP_USE_ALIGNED_ALLOC 1
    #else
        #define SLP_USE_ALIGNED_ALLOC 0
    #endif
#endif

#if SLP_USE_ALIGNED_ALLOC
    #define SLP_ALIGNMENT 64
    #define SLP_ALIGN_SIZE(size) (((size) + SLP_ALIGNMENT - 1) & ~(SLP_ALIGNMENT - 1))
    #ifdef _WIN32
        #define SLP_ALIGNED_ALLOC(size) _aligned_malloc(SLP_ALIGN_SIZE(size), SLP_ALIGNMENT)
        #define SLP_ALIGNED_FREE(ptr) _aligned_free(ptr)
    #else
        #define SLP_ALIGNED_ALLOC(size) aligned_alloc(SLP_ALIGNMENT, SLP_ALIGN_SIZE(size))
        #define SLP_ALIGNED_FREE(ptr) free(ptr)
    #endif
#else
    #define SLP_ALIGN_SIZE(size) (size)
    #define SLP_ALIGNED_ALLOC(size) malloc(size)
    #define SLP_ALIGNED_FREE(ptr) free(ptr)
#endif

#ifdef SLP_PNG_MACROS
    #define bswap_u32(x) ((((x) & 0xFF000000u) >> 24) | (((x) & 0x00FF0000u) >> 8) | \
                          (((x) & 0x0000FF00u) << 8) | (((x) & 0x000000FFu) << 24))
    #define bswap_u64(x) ((((x) & 0xFF00000000000000ull) >> 56) | (((x) & 0x00FF000000000000ull) >> 40) | \
                          (((x) & 0x0000FF0000000000ull) >> 24) | (((x) & 0x000000FF00000000ull) >> 8) |  \
                          (((x) & 0x00000000FF000000ull) << 8) | (((x) & 0x0000000000FF0000ull) << 24) |  \
                          (((x) & 0x000000000000FF00ull) << 40) | (((x) & 0x00000000000000FFull) << 56))

    // return big edian in memory order
    #define big_edian_u32_in_mem(x, is_little_edian) ((is_little_edian) ? (bswap_u32(x)) : (x))
    #define big_edian_u64_in_mem(x, is_little_edian) ((is_little_edian) ? (bswap_u64(x)) : (x))

    // read x as big edian
    #define big_edian_u32(x) (((uint32_t)((x)[0]) << 24) | ((uint32_t)((x)[1]) << 16) | ((uint32_t)((x)[2]) << 8) | ((uint32_t)((x)[3]) << 0))
    #define big_edian_u64(x) (((uint64_t)((x)[0]) << 56) | ((uint64_t)((x)[1]) << 48) | ((uint64_t)((x)[2]) << 40) | ((uint64_t)((x)[3]) << 32) | \
                              ((uint64_t)((x)[4]) << 24) | ((uint64_t)((x)[5]) << 16) | ((uint64_t)((x)[6]) << 8) | ((uint64_t)((x)[7]) << 0))

    // a and b are integer
    #define div_ceil(a, b) ((a) / (b) + ((a) % (b) != 0))
#endif

#ifdef __cplusplus
}
#endif

#endif