#include <slp_png.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>

// test images name:
// rover
// gray16
// grayscale_4bit
// 4bit3
// palette_4bit
// 10.4-MB

int rw_test(const char* path, const char* path_out);
int thread_safety_test(const char* path);


int main(int argc, char* argv[]) {
    char path[64] = "tests/test_images/10.4-MB.png";
    char path_out[64] = "TEST.png";

    bool thread_test = true;
    for (int i = 1; i < argc; i++) {
        switch (argv[i][0]) {
            case '-': {
                switch (argv[i][1]) {
                    case '-': {
                        thread_test = (strcmp(argv[i] + 2, "no-thread_test") == 0) ? false : thread_test;
                        break;
                    }
                }
                break;
            }
        }
    }

    if (thread_test) thread_safety_test(path);
    else rw_test(path, path_out);

    return 0;
}






int rw_test(const char* path, const char* path_out) {
    slp_image a = slp_png_read(path);
    if (a.buffer == NULL) {printf("\nread failed: %d\n", a.bit_depth);return 1;}

    int ret = slp_png_write(a, path_out);
    if (ret != 0) {printf("\nwrite failed: %d\n", ret);free(a.buffer);return 1;}


    // validate new saved image
    #define DEBUG
    slp_image b = slp_png_read(path_out);
    if (b.buffer == NULL) {printf("\nread newly saved .png failed: %d\n", b.bit_depth);return 1;}
    const size_t size = (size_t)a.width * a.height * a.channels * (1 + (a.bit_depth == 16));
    for (size_t i = 0; i < size; i++) {
        if (a.buffer[i] != b.buffer[i]) {
            printf("slp_png_write output error\n");
            free(a.buffer);free(b.buffer);
            return 1;
        }
    }

    slp_image_delete(&b);
    slp_image_delete(&a);
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
        slp_image a = slp_png_read(data.in_path);
        if (a.buffer == NULL) {
            abort();
        }


        // put more function here for thread safety check if needed



        int ret = slp_png_write(a, data.out_path);
        if (ret != 0) {
            abort();
        }

        // validate new saved image
        slp_image b = slp_png_read(data.out_path);
        if (b.buffer == NULL) {
            abort();
        }
        const size_t size = (size_t)a.width * a.height * a.channels * (1 + (a.bit_depth == 16));
        for (size_t i = 0; i < size; i++) {
            if (a.buffer[i] != b.buffer[i]) {
                abort();
            }
        }

        free(a.buffer);
        free(b.buffer);
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
            for (int j = 0; j <= i; j++) if (pthread_join(threads[j], NULL) != 0) abort();
            return -1;
        }
    }
    for (uint16_t i = 0; i < thread_count; i++) if (pthread_join(threads[i], NULL) != 0) abort();
    for (uint16_t i = 0; i < thread_count; i++) if (!thread_arg[i].status) return 1;

    return 0;
}
