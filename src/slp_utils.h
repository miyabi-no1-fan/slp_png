#pragma once
#include <stddef.h>
#include <stdint.h>

static inline uint32_t bswap_u32(uint32_t x) {
    return (x & 0xFF000000u) >> 24 |
           (x & 0x00FF0000u) >> 8 |
           (x & 0x0000FF00u) << 8 |
           (x & 0x000000FFu) << 24;
}

static inline uint64_t bswap_u64(uint64_t x) {
    return (x & 0xFF00000000000000ull) >> 56 |
           (x & 0x00FF000000000000ull) >> 40 |
           (x & 0x0000FF0000000000ull) >> 24 |
           (x & 0x000000FF00000000ull) >> 8 |
           (x & 0x00000000FF000000ull) << 8 |
           (x & 0x0000000000FF0000ull) << 24 |
           (x & 0x000000000000FF00ull) << 40 |
           (x & 0x00000000000000FFull) << 56;
}

static inline uint32_t to_be_u32(uint32_t x) {
    uint16_t v = 1;
    if (*(uint8_t*)&v)
        return bswap_u32(x);
    else
        return x;
}

static inline uint64_t to_be_u64(uint64_t x) {
    uint16_t v = 1;
    if (*(uint8_t*)&v)
        return bswap_u64(x);
    else
        return x;
}

static inline uint32_t from_be_u32(uint8_t* x) {
    return ((uint32_t)x[0]) << 24 |
           ((uint32_t)x[1]) << 16 |
           ((uint32_t)x[2]) << 8 |
           ((uint32_t)x[3]) << 0;
}

static inline uint64_t from_be_u64(uint8_t* x) {
    return ((uint64_t)x[0]) << 56 |
           ((uint64_t)x[1]) << 48 |
           ((uint64_t)x[2]) << 40 |
           ((uint64_t)x[3]) << 32 |
           ((uint64_t)x[4]) << 24 |
           ((uint64_t)x[5]) << 16 |
           ((uint64_t)x[6]) << 8 |
           ((uint64_t)x[7]) << 0;
}

static inline size_t div_ceil(size_t a, size_t b) {
    return (a / b) + (a % b != 0);
}
