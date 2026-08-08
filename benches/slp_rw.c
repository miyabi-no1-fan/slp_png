#include <slp_png.h>
#include <stdio.h>
#include <time.h>

// default path
char path[] = "tests/test_images/10.4-MB.png";
char path_out[] = "tests/test_images/new.png";

int main(void) {
    clock_t start = 0, end = 0;
    double read_time = 0;
    double write_time = 0;

    int ret = 0;

    start = clock();
    slp_image_t image;
    {
        FILE* file = fopen(path, "rb");
        int ret = slp_png_read(&image, &(slp_png_io) {.buf = file});
        if (ret != 0)
            return ret;
        fclose(file);
    }
    end = clock();

    read_time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("read time: %.3fs\n", read_time);

    start = clock();
    {
        FILE* file = fopen(path_out, "wb");
        int ret = slp_png_write(&image, &(slp_png_io) {.buf = file});
        if (ret != 0)
            return ret;
        fclose(file);
    }
    end = clock();

    write_time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("write time: %.3fs\n", write_time);

    slp_image_destroy(&image);
    return 0;
}
