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

#ifdef __cplusplus
extern "C" {
#endif

typedef struct slp_image_t {
    uint8_t* pixels;
    uint32_t height;
    uint32_t width;
    uint8_t channels;
    uint8_t bit_depth;
    size_t size;
} slp_image_t;

enum SLP_RESULT {
    SLP_SUCCESS,
    SLP_ERROR_ALLOC,
    // error from read/write operations
    SLP_ERROR_IO,
    // PNG is invalid **or not supported**
    SLP_ERROR_INVALID_PNG,
    SLP_ERROR_INVALID_ARGUMENT,
};
typedef enum SLP_RESULT slp_result_t;

/* This would set a hard limit for `slp_png_read` to reject any PNG that has width or height exceed this limit.
The limit is applied to all threads. */
void slp_png_set_limit(uint32_t width, uint32_t height);

typedef struct {
    /* read `n` bytes from `src` to `dst`.
    Return true on success and false on error. */
    bool (*read)(void* dst, void* src, size_t n);

    /* write `n` bytes from `src` to `dst`.
    Return true on success and false on error. */
    bool (*write)(void* src, void* dst, size_t n);

    /* move the read/write `buf` by `n` bytes.
    Return true on success and false on error. */
    bool (*seek)(void* buf, uint32_t n);

    /* used as `src` for `read`.
    used as `dst` for `write`. */
    void* buf;
} slp_png_io;

/* Read a PNG image,

`slp_png_read` only use `read`, `seek`, `buf` from `png`. The rest is ignored.

By default, `read` is `fread` and `seek` is `fseek`.

`slp_png_read` have width/height limit.
By default, the limit is 12288x6480.

return `SLP_ERROR_INVALID_PNG` error if any PNG exceed this limit.

You can change the limit via `slp_png_set_limit`. */
slp_result_t slp_png_read(slp_image_t* image, const slp_png_io* png);

/* Write a PNG image.

`slp_png_read` only use `write`, `buf` from `png`. The rest is ignored. */
slp_result_t slp_png_write(const slp_image_t* image, const slp_png_io* png);

/* destroy the image's pixels.
if image == NULL or image->pixels == NULL, this does nothing */
void slp_image_destroy(slp_image_t* image);

// define some macro for easier replacement
#ifdef SLP_PNG_MACROS
    #include <stdlib.h>
    #include <string.h>

    #ifndef SLP_MALLOC
    #define SLP_MALLOC(size) malloc(size)
    #endif

    #ifndef SLP_CALLOC
    #define SLP_CALLOC(size) calloc(size, 1)
    #endif

    #ifndef SLP_FREE
    #define SLP_FREE(ptr, size) \
    do {                        \
        (void)(size);           \
        free(ptr);              \
    } while (0)
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
#endif /* SLP_PNG_MACROS */

#ifdef __cplusplus
}
#endif
