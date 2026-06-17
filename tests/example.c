#include <slp_png.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// test images name:
// rover
// gray16
// grayscale_4bit
// 4bit3
// palette_4bit
// 10.4-MB

int main(int argc, char* argv[]) {
    // default path
    char path[64] = "tests/test_images/10.4-MB.png";
    char path_out[64] = "tests/test_images/new.png";

    // parse input path
    for (int i = 1; i < argc; i++) {
        switch (argv[i][0]) {
            case '-': {
                switch (argv[i][1]) {
                    case 'o': {
                        strncpy(path_out, argv[i + 1], 64);
                        i++;
                        break;
                    }
                }
                break;
            }
            default: {
                strncpy(path, argv[i], 64);
                break;
            }
        }
    }

    clock_t start = 0, end = 0;
    double read_time = 0;
    double write_time = 0;

    start = clock();
    slp_image_t a = slp_png_read(path);
    end = clock();
    if (a.buffer == NULL) {printf("read failed: %d\n", a.bit_depth);return 1;}

    read_time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("read time: %.6fs\n", read_time);

    start = clock();
    int ret = slp_png_write(a, path_out);
    end = clock();
    if (ret != 0) {printf("save failed: %d\n", ret);return 1;}

    write_time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("write time: %.6fs\n", write_time);

    printf("total time: %.6fs\n", read_time + write_time);

    slp_image_destroy(&a);
    return 0;
}
