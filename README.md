# Simple PNG codec library written in C++

## Support platforms
- OS: Linux, MacOS, Windows
- Architecture: x86-64, arm64

Note: for now, we have only optimized for x86-64 architecture

## Dependencies
- zlib

## Install
```bash
git clone https://github.com/miyabi-no1-fan/slp_png.git
cd slp_png
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

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

    return 0;
}
```

## Support

- Chunk type: `IHDR`, `IDAT`, `IEND`, `PLTE` (color type 3 only), `tRNS` (color type 3 only)
- Color type: 0/2/3/4/6
- Bit depth: 1/2/4/8/16
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
cmake --build --preset release -j$(nproc)
python tests/test_images/generate_some_images.py
mv *.png -t tests/test_images/benchmark
echo "---------------------------------------"
for file in tests/test_images/benchmark/*.png; do
    # See `tests/benchmark.cpp`
    build/release/benchmark $file
    echo ""
done
```

Test image: `tests/test_images/benchmark/10.4-MB.png`

| Library | Read time | Write time | Output file size |
| --- | --- | --- | --- |
| `libspng` | 0.095s | 0.977s | 10.658 MiB |
| `slp_png` | 0.089s | 0.686s | 10.645 MiB |