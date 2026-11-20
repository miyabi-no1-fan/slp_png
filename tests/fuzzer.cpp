/*
```bash
mkdir -p tests/corpus && mkdir -p tests/corpus/crashes
cmake --preset debug -DSLP_PNG_BUILD_FUZZER=ON
cmake --build build/debug
build/debug/fuzzer -max_total_time=120 -artifact_prefix=tests/corpus/crashes/ tests/corpus/
```
*/

#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <span>
#include <spanstream>
#include <sstream>

#include "slp_png.hpp"

constexpr std::uint32_t WIDTH_LIMIT = 1024;
constexpr std::uint32_t HEIGHT_LIMIT = 1024;

extern "C" auto LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) -> int try {
    std::ispanstream in(std::span<const char>(reinterpret_cast<const char*>(data), size));

    auto img = slp::Image::read_png(in, WIDTH_LIMIT, HEIGHT_LIMIT);
    if (!img) return 0;

    std::ostringstream out(std::ios::binary);
    img->write_png(out).value();

    return 0;
} catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
}
