char __comment[] =
    "To run fuzzer test: "
    "mkdir -p tests/corpus && mkdir -p tests/corpus/crashes"
    "cmake -S . -B build -DBUILD_FUZZER=ON -DENABLE_OPTS=OFF -DCMAKE_C_COMPILER=clang"  // must use clang and disable opts
    "cmake --build build"
    "build/fuzz_slp_png -max_total_time=120 -artifact_prefix=tests/corpus/crashes/ tests/corpus/";  // max total time is in sec

#include <slp_png.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    const uint8_t* data;
    const size_t len;
    size_t pos;
} io_buf;

bool read(void* dst, void* src, size_t n) {
    io_buf* buf = (io_buf*)src;

    if (buf->pos + n > buf->len)
        return false;

    memcpy(dst, buf->data + buf->pos, n);
    buf->pos += n;
    return true;
}

bool seek(void* src, uint32_t n) {
    io_buf* buf = (io_buf*)src;

    if (buf->pos + n > buf->len)
        return false;

    buf->pos += n;
    return true;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, const size_t size) {
    io_buf buf = {
        .data = data,
        .len = size,
        .pos = 0,
    };

    slp_png_io io = {
        .read = &read,
        .seek = &seek,
        .buf = (void*)&buf,
        .write = NULL,
    };

    slp_image_t image = {0};
    int ret = slp_png_read(&image, &io);
    if (ret == 0) {
        slp_image_destroy(&image);
    }

    // malformed input should return an error code, not crash. libFuzzer + ASan/UBSan catch crashes, overflows, and UB.
    return 0;
}
