# PNG codec library written in C

## Support platforms
- Linux
- macOS
- Windows


## Install
```bash
git clone https://github.com/slp-c/slp_png.git
cd slp_png
cmake -B build
cmake --build build
```
- If you want to run test, see CMakeLists.txt option
- Or you can run this following line:
```bash
gcc tests/example.c -Iinclude -Lbuild -lslp_png -Wl,-rpath,build -o build/example
build/example
```
- Replace tests/example.c with the file you wanna test with


## Project structure
- slp_png: PNG codec
    - include:
        - include/slp_image.h
        - include/slp_png.h
    - src:
        - src/slp_png_read/*
        - src/slp_png_write/*
    - dependencies:
        - zlib

- slp_image_transform: (still experimental)
    - include:
        - include/slp_image.h
        - include/slp_image_transform.h
    - src:
        - src/slp_image_transform/*
    - dependencies:
        - pthreads
        - math


## Basic usage
```C
#include <slp_png.h>
#include <stdio.h>

int main(void)
{
    slp_image_t your_image = slp_png_read("/path/to/your/image");
    if (your_image.buffer == NULL) return 1;

    int ret = slp_png_write(your_image, "/path/to/where/to/write");
    if (ret != 0) return 1;

    slp_image_destroy(&your_image);
    return 0;
}
```
- NOTICE: if slp_png_read fail, your_image.bit_depth will be overwitten with a specified error code.
    - See in include/slp_image.h for more details about the error code


## Support
- For slp_png_read:
    - CHUNKS:
        - For color type 0/2/4/6: IHDR, IDAT, IEND
        - For color type 3: IHDR, PLTE, tRNS, IDAT, IEND
    - Color type: 0/2/3/4/6
        - NOTICE that color type 3 will be force convert into RGBA32 ( color type 6, bit depth 8 )
    - Bit depth: 1/2/4/8/16
        - NOTICE that for bit depth 16 format, output is always big-edian
    - Compression method: 0
    - Filter method: 0
    - Interlace method: 0
    - Full CRC32 validation for all supported chunks
    - Use fixed size buffer for IDAT chunks decode
        - No RAM spikes when decode PNG with big IDAT
        - Buffer size = 65536 bytes

- For slp_png_write:
    - CHUNKS: IHDR, IDAT, IEND
    - Color type: 0/2/4/6
    - Bit depth: 1/2/4/8/16
        - NOTICE that for bit depth 16 format, input must be big-edian
    - Compression method: 0
    - Filter method: 0
    - Interlace method: 0
    - Compression level: 6
    - Heuristic filtering with all 5 filter type
        - Heuristic filtering is extremely cheap, if good filter is generated, deflate runtime will reduce significantly

- For both slp_png_read and slp_png_write:
    - SIMD optimizations (no AVX512)
    - Thread-safe: this function can call by any thread, but it does not automatically handle fileIO conflicts
    - The buffer in slp_image_t is allocated via aligned_alloc
    - See include/slp_image.h if there are any #define you wanna change (aligned_alloc, memcpy, malloc,...)


## Performance
- Platform: Linux
- CPU: intel i5 12450H
- RAM: 16GB DDR5
- Compare with https://github.com/randy408/libspng

- For slp_png:
```bash
git clone https://github.com/slp-c/slp_png.git
cd slp_png
cmake -S . -B build -DBUILD_EXAMPLE=ON
cmake --build build
build/example
```
- For spng, I use a modified one of https://github.com/randy408/libspng/blob/master/examples/example.c for performance test:
```C
// in main:
// for read time, the clock start here
    start = clock();
    png = fopen(path, "rb");
// and end here
    do
    {
        ret = spng_get_row_info(ctx, &row_info);
        if(ret) break;

        ret = spng_decode_row(ctx, image + row_info.row_num * image_width, image_width);
    }
    while(!ret);

    end = clock();

// in encode_image:
// the clock is like this
start = clock();ret = spng_encode_image(ctx, image, length, fmt, SPNG_ENCODE_FINALIZE);end = clock();
```

- Results:
- Read time:
    - slp_png: 0.104468s
    - libspng: 0.105065s

- Write time:
    - slp_png: 0.717744s
    - libspng: 1.056884s

- Output file size:
    - slp_png: 10.6 MiB ( 11,161,849 bytes )
    - libspng: 10.7 MiB ( 11,176,211 bytes )

- Peak RAM usage:
    - libspng: 33 MiB
    - slp_png: 33 MiB

- slp_png read time is usually the same as spng, while slp_png write time is a little bit faster
- The differences in performance is small
- Though slp_png got simplier API, just a single slp_png_read/write call that have an equivalent performance is not bad right?
