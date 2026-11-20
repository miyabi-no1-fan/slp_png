# Simple PNG codec library written in C++

## Support platforms
- OS: Linux, MacOS, Windows
- Architecture: x86_64, ARM64

## Dependencies
- zlib

## Install
```bash
git clone https://github.com/miyabi-no1-fan/slp_png.git
cd slp_png
cmake --preset release
cmake --build --preset release
```
- If you want to run test, see `CMakeLists.txt` option

## Basic usage
```CXX
#include <slp_png.hpp>
#include <iostream>

int main() {
    auto img = slp::Image::read_png("in.png");
    if (!img) {
        std::cerr << "read error: " << img.error() << "\n";
        return 1;
    }

    if (auto res = img->write_png("out.png"); !res) {
        std::cerr << "write error: " << res.error() << "\n";
        return 1;
    }
}
```

## Features

- Chunk type support: `IHDR`, `IDAT`, `IEND`, `PLTE` (color type 3 only), `tRNS` (color type 3 only)
- Color type support: 0/2/3/4/6
- Bit depth support: 1/2/4/8/16
- Compression method: 0
- Filter method: 0
- Interlace method: 0

## Performance

- Platform: Linux
- CPU: intel i5 12450H
- RAM: 16GB DDR5
- Compare with https://github.com/randy408/libspng

```bash
git clone https://github.com/slp-c/slp_png.git
cd slp_png
cmake --preset release -DSLP_PNG_BUILD_BENCH=ON
cmake --build --preset release
build/release/benchmark
# See `tests/benchmark.cpp`
```

Test image: `tests/test_images/benchmark/10.4-MB.png`

| Library | Read time | Write time |
| --- | --- | --- |
| `libspng` | 0.096s | 0.981s |
| `slp_png` | 0.106s | 0.709s |
