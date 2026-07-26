#include <pthread.h>
#include <slp_png.h>
#include <spng.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

uint8_t* read_png(const char* filepath, size_t* out_size);
int rw_test(const char* path, const char* path_out);
int thread_safety_test(const char* path);


int main(int argc, char* argv[]) {
    char path[64] = "tests/test_images/10.4-MB.png";
    char path_out[64] = "TEST.png";

    bool thread_test = false;
    for (int i = 1; i < argc; i++) {
        if (argv[i] == NULL)
            continue;
        switch (argv[i][0]) {
            case '-': {
                if (strlen(argv[i]) < 2)
                    continue;
                switch (argv[i][1]) {
                    case '-': {
                        thread_test = (strcmp(argv[i] + 2, "thread_test") == 0) ? true : thread_test;
                        break;
                    }
                }
                break;
            }
        }
    }

    if (thread_test)
        thread_safety_test(path);
    else
        rw_test(path, path_out);

    return 0;
}






int rw_test(const char* path, const char* path_out) {
    size_t spng_size = 0;
    uint8_t* spng_image = read_png(path, &spng_size);
    if (spng_image == NULL)
        panic("spng read failed");

    slp_image_t a = slp_png_read(path);
    if (a.pixels == NULL)
        panic("slp_png_read failed: %u", a.bit_depth);

    if (a.image_size != spng_size)
        panic("slp_png_read image size mismatch");

    for (size_t i = 0; i < a.image_size; i++)
        if (a.pixels[i] != spng_image[i])
            panic("slp_png_read image pixels mismatch");
    free(spng_image);

    int ret = slp_png_write(a, path_out);
    if (ret != 0)
        panic("slp_png_write failed: %d", ret);

    // validate new saved image
    slp_image_t b = slp_png_read(path_out);
    if (b.pixels == NULL)
        panic("slp_png_read failed: %u", b.bit_depth);

    if (b.image_size != a.image_size)
        panic("slp_png_read image size mismatch");

    for (size_t i = 0; i < a.image_size; i++)
        if (b.pixels[i] != a.pixels[i])
            panic("slp_png_read image pixels mismatch");

    slp_image_destroy(&b);
    slp_image_destroy(&a);
    return 0;
}


struct thread_safety_test_arg {
    const char* in_path;
    const char* out_path;
    bool* status;
};


void* thread_safety_test_task(void* arg) {

    enum {spam = 1};

    for (uint16_t i = 0; i < spam; i++) {
        struct thread_safety_test_arg data = *(struct thread_safety_test_arg*)arg;
        slp_image_t a = slp_png_read(data.in_path);
        if (a.pixels == NULL) {
            abort();
        }

        // put more function here for thread safety check if needed

        int ret = slp_png_write(a, data.out_path);
        if (ret != 0) {
            abort();
        }

        // validate new saved image
        slp_image_t b = slp_png_read(data.out_path);
        if (b.pixels == NULL) {
            abort();
        }
        const size_t size = (size_t)a.width * a.height * a.channels * (1 + (a.bit_depth == 16));
        for (size_t i = 0; i < size; i++) {
            if (a.pixels[i] != b.pixels[i]) {
                abort();
            }
        }

        free(a.pixels);
        free(b.pixels);
    }

    return NULL;
}


int thread_safety_test(const char *path) {
    const char out_paths_prefix[] = "TEST-%02u.png";

    enum {thread_count = 50};
    pthread_t threads[thread_count] = {0};
    struct thread_safety_test_arg thread_arg[thread_count] = {0};
    char out_paths_ptr[thread_count][32] = {0};
    bool thread_status[thread_count] = {0};

    for (uint16_t i = 0; i < thread_count; i++) thread_status[i] = true;

    for (uint16_t i = 0; i < thread_count; i++) {
        snprintf(out_paths_ptr[i], 32, out_paths_prefix, i);

        thread_arg[i].in_path = path;
        thread_arg[i].out_path = out_paths_ptr[i];
        thread_arg[i].status = thread_status + i;

        if (pthread_create(threads + i, NULL, thread_safety_test_task, thread_arg + i) != 0) {
            for (int j = 0; j <= i; j++)
                if (pthread_join(threads[j], NULL) != 0) abort();
            return -1;
        }
    }
    for (uint16_t i = 0; i < thread_count; i++)
        if (pthread_join(threads[i], NULL) != 0) abort();
    for (uint16_t i = 0; i < thread_count; i++)
        if (!thread_arg[i].status) return 1;

    return 0;
}

uint8_t* read_png(const char *filepath, size_t *out_size) {
    FILE* file = fopen(filepath, "rb");
    if (!file) return NULL;

    spng_ctx* ctx = spng_ctx_new(0);
    if (!ctx) {
        fclose(file);
        return NULL;
    }

    if (spng_set_png_file(ctx, file) != 0) {
        spng_ctx_free(ctx);
        fclose(file);
        return NULL;
    }

    struct spng_ihdr ihdr;
    if (spng_get_ihdr(ctx, &ihdr) != 0) {
        spng_ctx_free(ctx);
        fclose(file);
        return NULL;
    }

    if (ihdr.color_type == SPNG_COLOR_TYPE_INDEXED)
        panic("Test read_png function can't handle color type 3");

    size_t image_size;
    if (spng_decoded_image_size(ctx, SPNG_FMT_PNG, &image_size) != 0) {
        spng_ctx_free(ctx);
        fclose(file);
        return NULL;
    }

    uint8_t* image_buf = (uint8_t*)malloc(image_size);
    if (!image_buf) {
        spng_ctx_free(ctx);
        fclose(file);
        return NULL;
    }

    if (spng_decode_image(ctx, image_buf, image_size, SPNG_FMT_PNG, 0) != 0) {
        free(image_buf);
        image_buf = NULL;
    } else if (out_size) {
        *out_size = image_size;
    }

    spng_ctx_free(ctx);
    fclose(file);

    return image_buf;
}
