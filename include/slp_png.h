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
#pragma once
#include <stdbool.h>
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
} slp_image_t;

enum SLP_ERROR {
    // allocation failed
    ALLOC_ERR = -1,
    // input/output error from file operations
    IO_ERR = 1,
    // PNG is invalid **or not supported**, used for both `read` and `write`
    INVALID_PNG = 2,
    // internal decode/encode error
    ZLIB_ERR = 3,
    // function arguments are NULL
    NULL_ARGS = 4,
};

/* This would set a hard limit for `slp_png_read` to reject any PNG that has width and height exceed this limit.
- Thread-safe: The limit is applied per thread and complete independent each thread (via _Thread_local) */
void slp_png_set_limit(uint32_t width, uint32_t height);


typedef struct {
    /* read `n` bytes from `src` to `dst`.
    Return true on success and false on error. */
    bool (*read)(void* dst, void* src, size_t n);

    /* write `n` bytes from `src` to `dst`.
    Return true on success and false on error. */
    bool (*write)(void* src, void* dst, size_t n);

    /* why `write` is (src, dst) but `read` is (dst, src) ?
    Think of it like this:
    "a read from b" is equivalent to "b write to a".
    So plug in b = `src` and a = `dst` we have:
    `dst` `read` `src` and `src` `write` `dst` */

    /* move the read/write `buf` by `n` bytes.

    `n` can be negative. */
    bool (*seek)(void* buf, uint32_t n);

    /* used as `src` for `read`.
    used as `dst` for `write`. */
    void* buf;
} slp_png_io;

/* Read a PNG image,
return status either 0 on success or SLP_ERROR on failure.

`slp_png_read` only use `read`, `seek`, `buf` from `png`. The rest is ignored.

By default, `read` is `fread` and `seek` is `fseek`.

`slp_png_read` have width/height limit:
```C
_Thread_local uint32_t with_limit = 12288;
_Thread_local uint32_t height_limit = 6480;
```

return `INVALID_PNG` error if any PNG exceed this limit.

you can change the limit via `slp_png_set_limit`. */
int slp_png_read(slp_image_t* image, const slp_png_io* png);

/* Write a PNG image.
return status either 0 on success or SLP_ERROR on failure.

`slp_png_read` only use `write`, `buf` from `png`. The rest is ignored. */
int slp_png_write(const slp_image_t* image, const slp_png_io* png);

/* destroy the image, free up resources.
if image == NULL or image->pixels == NULL, this does nothing */
void slp_image_destroy(slp_image_t* image);

// define some macro for easier replacement
#ifdef SLP_PNG_MACROS
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