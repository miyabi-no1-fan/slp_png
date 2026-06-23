# Simple PNG codec library written in C

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
- Or you could run this following line:
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
    - Low memory spikes
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
- For spng, I use https://github.com/randy408/libspng/blob/master/examples/example.c for the test

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
