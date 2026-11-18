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

todo...
