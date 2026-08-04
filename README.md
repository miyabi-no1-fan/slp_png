# Simple PNG codec library written in C

## Support platforms
- Linux
- macOS
- Windows


## Dependencies
- zlib


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


## Basic usage
```C
#include <slp_png.h>

int main(void) {
    slp_image_t your_image = slp_png_read("/path/to/your/image");
    if (!your_image.buffer) return 1;

    int ret = slp_png_write(your_image, "/path/to/where/to/write");
    if (ret != 0) return 1;

    slp_image_destroy(&your_image);
    return 0;
}
```
The output image format is raw format except for color type 3 which will be converted to RGBA32.
If you're familiar with libspng, our output format is equivalent to this:
```C
(color_type == SPNG_COLOR_TYPE_INDEXED) ? SPNG_FMT_RGBA8 : SPNG_FMT_RAW
```
We have our test to ensure slp_png_read output match every single byte of libspng output using the same format.


## Features
For slp_png_read:
- CHUNKS:
    - For color type 0/2/4/6: IHDR, IDAT, IEND
    - For color type 3: IHDR, IDAT, IEND, PLTE, tRNS
- Color type: 0/2/3/4/6
    - NOTICE: color type 3 will be converted into RGBA32 ( color type 6, bit depth 8 )
- Bit depth: 1/2/4/8/16 (big-edian)
- Compression method: 0
- Filter method: 0
- Interlace method: 0

For slp_png_write:
- CHUNKS: IHDR, IDAT, IEND
- Color type: 0/2/4/6
- Bit depth: 1/2/4/8/16
    - NOTICE: for bit depth 16, input must be big-edian
- Compression method: 0
- Filter method: 0
- Interlace method: 0

For both slp_png_read and slp_png_write:
- Thread-safety: this function can call by any thread, but it does not automatically handle fileIO conflicts
- The buffer in slp_image_t is allocated via aligned_alloc by default with 64 bytes alignment


## Performance
- Platform: Linux
- CPU: intel i5 12450H
- RAM: 16GB DDR5
- Compare with https://github.com/randy408/libspng

For slp_png:
```bash
git clone https://github.com/slp-c/slp_png.git
cd slp_png
cmake -S . -B build -DBUILD_EXAMPLE=ON
cmake --build build
build/example
```

For spng:
```bash
git clone https://github.com/slp-c/slp_png.git
cd slp_png
cmake -S . -B build -DBUILD_TEST=ON
cmake --build build
build/test --spng-benchmark
```

Test image: tests/test_images/10.4-MB.png

Results:
- Read time:
    - slp_png: 0.105s
    - libspng: 0.105s

- Write time:
    - slp_png: 0.693s
    - libspng: 1.056s

- Output file size:
    - slp_png: 10.63MB
    - libspng: 10.66MB

Somehow my encoder is a bit faster, correct me if I'm wrong cause I have no idea how is mine faster
