#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
#endif

#if defined(__unix__) || defined(__APPLE__)
    #include <unistd.h>
#endif

#if defined(__i386__) || defined(__x86_64__)
    #include <immintrin.h>
#endif

#include <slp_image_transform.h>
#include <slp_png.h>
#include <spng.h>

static int get_nproc(void) {
    #ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
    #endif
    #if defined(__unix__) || defined(__APPLE__)
    return sysconf(_SC_NPROCESSORS_ONLN);
    #endif
    return 2;
}

// test images name:
// rover
// gray16
// grayscale_4bit
// 4bit3
// palette_4bit
// 10.4-MB

#define panic(...) \
    do {                                                               \
        fprintf(stderr, "panic at:\n File: %s\n  Line: %d\n", __FILE__, __LINE__); \
        fprintf(stderr, __VA_ARGS__);                                              \
        fprintf(stderr, "\n");                                                     \
        fflush(stderr);                                                            \
        exit(-1);                                                                  \
    } while (0)

slp_image_t spng_read_png(const char* filepath);
void spng_bench(const char* path, const char* path_out);
void rw_test(const char* path, const char* path_out);
void thread_safety_test(const char* path);


int main(int argc, char* argv[]) {
    char* path = "tests/test_images/10.4-MB.png";
    char* path_out = "TEST.png";

    bool thread_test = false;
    bool spng_benchmark = false;

    for (int i = 1; i < argc; i++) {
        if (argv[i] == NULL)
            continue;
        switch (argv[i][0]) {
            case '-': {
                if (strlen(argv[i]) < 2)
                    continue;
                switch (argv[i][1]) {
                    case '-': {
                        thread_test = (strcmp(argv[i] + 2, "thread-test") == 0) ? true : thread_test;
                        spng_benchmark = (strcmp(argv[i] + 2, "spng-benchmark") == 0) ? true : spng_benchmark;
                        break;
                    }
                }
                break;
            }
            default: {
                path = argv[i];
            }
        }
    }

    if (thread_test)
        thread_safety_test(path);
    else if (spng_benchmark)
        spng_bench(path, path_out);
    else
        rw_test(path, path_out);

    return 0;
}


void rw_test(const char* path, const char* path_out) {
    slp_image_t spng_image = spng_read_png(path);
    if (spng_image.pixels == NULL)
        panic("spng read failed");

    slp_image_t a = slp_png_read(path, NULL);
    if (a.pixels == NULL)
        panic("slp_png_read failed: %u", a.bit_depth);

    if (a.image_size != spng_image.image_size)
        panic("slp_png_read image size mismatch");

    for (size_t i = 0; i < a.image_size; i++)
        if (a.pixels[i] != spng_image.pixels[i])
            panic("slp_png_read image pixels mismatch at %zu / %zu, value: %u vs %u", i, a.image_size, a.pixels[i], spng_image.pixels[i]);
    free(spng_image.pixels);

    int ret = slp_png_write(a, path_out);
    if (ret != 0)
        panic("slp_png_write failed: %d", ret);

    // validate new saved image
    slp_image_t b = slp_png_read(path_out, NULL);
    if (b.pixels == NULL)
        panic("slp_png_read failed: %u", b.bit_depth);

    if (b.image_size != a.image_size)
        panic("slp_png_read image size mismatch");

    for (size_t i = 0; i < a.image_size; i++)
        if (b.pixels[i] != a.pixels[i])
            panic("slp_png_read image pixels mismatch");

    slp_image_destroy(&b);
    slp_image_destroy(&a);
}


struct thread_safety_test_arg {
    const char* in_path;
    const char* out_path;
};

void* thread_safety_test_task(void* arg) {
    const int spam = 10;

    for (uint16_t i = 0; i < spam; i++) {
        struct thread_safety_test_arg data = *(struct thread_safety_test_arg*)arg;
        rw_test(data.in_path, data.out_path);
    }

    return NULL;
}

void thread_safety_test(const char *path) {
    const char out_paths_prefix[] = "TEST-%02u.png";

    const int thread_count = get_nproc();
    printf("Number of threads: %d\n", thread_count);

    pthread_t threads[thread_count] = {};
    struct thread_safety_test_arg thread_arg[thread_count] = {};
    char out_paths_ptr[thread_count][32] = {};

    for (uint16_t i = 0; i < thread_count; i++) {
        snprintf(out_paths_ptr[i], 32, out_paths_prefix, i);

        thread_arg[i].in_path = path;
        thread_arg[i].out_path = out_paths_ptr[i];

        if (pthread_create(threads + i, NULL, thread_safety_test_task, thread_arg + i) != 0)
            for (int j = 0; j <= i; j++)
                if (pthread_join(threads[j], NULL) != 0)
                    panic("thread fail to join");
    }

    for (uint16_t i = 0; i < thread_count; i++)
        if (pthread_join(threads[i], NULL) != 0)
            panic("thread fail to join");
}

static inline int get_channels(const int color_type, const int bit_depth) {
    switch (color_type) {
        case 0: {
            switch (bit_depth) {
                case 1: break;
                case 2: break;
                case 4: break;
                case 8: break;
                case 16: break;
                default: return 0;
            }
            return 1;
        }
        case 2: {
            switch (bit_depth) {
                case 8: break;
                case 16: break;
                default: return 0;
            }
            return 3;
        }
        case 3: {
            switch (bit_depth) {
                case 1: break;
                case 2: break;
                case 4: break;
                case 8: break;
                default: return 0;
            }
            return 4;
        }
        case 4: {
            switch (bit_depth) {
                case 8: break;
                case 16: break;
                default: return 0;
            }
            return 2;
        }
        case 6: {
            switch (bit_depth) {
                case 8: break;
                case 16: break;
                default: return 0;
            }
            return 4;
        }
        default: return 0;
    }
}

static inline uint8_t get_color_type(const uint8_t channels) {
    switch (channels) {
        case 1: return 0;
        case 2: return 4;
        case 3: return 2;
        case 4: return 6;
        default: return 0xFF;
    }
}

slp_image_t spng_read_png(const char *filepath) {
    slp_image_t null = {};

    FILE* file = fopen(filepath, "rb");
    if (!file) return null;

    spng_ctx* ctx = spng_ctx_new(0);
    if (!ctx) {
        fclose(file);
        return null;
    }

    if (spng_set_png_file(ctx, file) != 0) {
        spng_ctx_free(ctx);
        fclose(file);
        return null;
    }

    struct spng_ihdr ihdr;
    if (spng_get_ihdr(ctx, &ihdr) != 0) {
        spng_ctx_free(ctx);
        fclose(file);
        return null;
    }

    const int target_fmt = (ihdr.color_type == SPNG_COLOR_TYPE_INDEXED) ? SPNG_FMT_RGBA8 : SPNG_FMT_RAW;

    size_t image_size;
    if (spng_decoded_image_size(ctx, target_fmt, &image_size) != 0) {
        spng_ctx_free(ctx);
        fclose(file);
        return null;
    }

    uint8_t* image_buf = (uint8_t*)malloc(image_size);
    if (!image_buf) {
        spng_ctx_free(ctx);
        fclose(file);
        return null;
    }

    if (spng_decode_image(ctx, image_buf, image_size, target_fmt, 0) != 0) {
        free(image_buf);
        spng_ctx_free(ctx);
        fclose(file);
        return null;
    }

    spng_ctx_free(ctx);
    fclose(file);

    slp_image_t image = {
        .pixels = image_buf,
        .height = ihdr.height,
        .width = ihdr.width,
        .channels = get_channels(ihdr.color_type, ihdr.bit_depth),
        .bit_depth = ihdr.bit_depth,
        .image_size = image_size,
    };

    return image;
}

int spng_write_png(const char *filepath, const slp_image_t image) {
    if (image.pixels == NULL) {
        panic("image.pixels == NULL");
    }

    FILE* file = fopen(filepath, "wb");
    if (!file) return -1;

    spng_ctx* ctx = spng_ctx_new(SPNG_CTX_ENCODER);
    if (!ctx) {
        fclose(file);
        return -1;
    }

    if (spng_set_png_file(ctx, file) != 0) {
        spng_ctx_free(ctx);
        fclose(file);
        return -1;
    }

    struct spng_ihdr ihdr = {
        .width = image.width,
        .height = image.height,
        .bit_depth = image.bit_depth,
        .color_type = get_color_type(image.channels),
        .compression_method = 0,
        .filter_method = 0,
        .interlace_method = 0,
    };

    if (spng_set_ihdr(ctx, &ihdr) != 0) {
        spng_ctx_free(ctx);
        fclose(file);
        return -1;
    }

    if (spng_encode_image(ctx, image.pixels, image.image_size, SPNG_FMT_RAW, SPNG_ENCODE_FINALIZE) != 0) {
        spng_ctx_free(ctx);
        fclose(file);
        return -1;
    }

    spng_ctx_free(ctx);
    fclose(file);
    return 0;
}

void spng_bench(const char* path, const char* path_out) {
    clock_t start, end;
    double read_time = 0;
    double write_time = 0;

    start = clock();
    slp_image_t spng_image = spng_read_png(path);
    end = clock();
    if (spng_image.pixels == NULL) panic("spng read failed");

    read_time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("read time: %.3fs\n", read_time);

    start = clock();
    int ret = spng_write_png(path_out, spng_image);
    end = clock();
    if (ret != 0) panic("spng save failed");

    write_time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("write time: %.3fs\n", write_time);

    free(spng_image.pixels);
}
