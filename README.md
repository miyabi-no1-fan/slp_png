# Simple PNG codec library written in C

## Support platforms
- OS: Linux, MacOS, Windows.
- Architecture: x86_64, ARM64.


## Dependencies
- zlib


## Install
```bash
git clone https://github.com/slp-c/slp_png.git
cd slp_png
cmake -B build
cmake --build build
```
- If you want to run test, see `CMakeLists.txt` option


## Basic usage
```C
#include <slp_png.h>
#include <stdio.h>

int main(void) {
    slp_image_t your_image;
    {
        FILE* file = fopen("/path/to/your/image", "rb");
        int ret = slp_png_read(&your_image, &(slp_png_io) {.buf = file});
        if (ret != 0)
            return ret;
        fclose(file);
    }

    {
        FILE* file = fopen("/path/to/where/to/write", "wb");
        int ret = slp_png_write(&your_image, &(slp_png_io) {.buf = file});
        if (ret != 0)
            return ret;
        fclose(file);
    }

    slp_image_destroy(&your_image);
    return 0;
}
```


## Features
For `slp_png_read`:
- Chunk type support: `IHDR`, `IDAT`, `IEND`, `PLTE` (color type 3 only), `tRNS` (color type 3 only)
- Color type support: 0/2/3/4/6
- Bit depth support: 1/2/4/8/16
- Compression method: 0
- Filter method: 0
- Interlace method: 0

For `slp_png_write`:
- Chunk type support: `IHDR`, `IDAT`, `IEND`
- Color type: 0/2/4/6
- Bit depth: 1/2/4/8/16
- Compression method: 0
- Filter method: 0
- Interlace method: 0

Format:
- `slp_png_read` output format is `raw format` except for `color type 3` which will be converted to `RGBA32`.

    If you're familiar with `libspng`, our output format is equivalent to this:
    ```C
    (color_type == SPNG_COLOR_TYPE_INDEXED) ? SPNG_FMT_RGBA8 : SPNG_FMT_RAW
    ```
    We have our test to ensure `slp_png_read` output match every single byte of `libspng` output, using the same format.

- `slp_png_write` expect the same format as `slp_png_read` output format.


## Security
- In general security of this project is still low and not recommended to use.
- You should read `.github/workflows/cmake-multi-platform.yml` and `tests/test.c` for details if needed.


## Performance
- Platform: Linux
- CPU: intel i5 12450H
- RAM: 16GB DDR5
- Compare with https://github.com/randy408/libspng

```bash
git clone https://github.com/slp-c/slp_png.git
cd slp_png
cmake -S . -B build -DBUILD_TEST=ON
cmake --build build

# For slp_png
build/slp_benchmark

# For spng
build/test --spng-benchmark

# You could also try with `hyperfine` and `perf` for more precise comparison
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

Somehow our encoder is a bit faster than `libspng`

---

I have some clues but I'll keep it myself.

You can just think "maybe the benchmark is unfair", that's ok.

If you wanna know why, clone this repo and test it yourself - or ask me in private, maybe I'll response.

---
By the way, you might wonder why we use the Apache License.
It's simple, Apache License was the first option in Github's `Add License`, so I just pick it idc
