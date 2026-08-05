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

    start = clock();
    slp_image_t image = slp_png_read(path);
    if (image.pixels == NULL) return 1;
    end = clock();

    read_time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("read time: %.3fs\n", read_time);

    start = clock();
    int ret = slp_png_write(image, path_out);
    if (ret != 0) return 1;
    end = clock();

    write_time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("write time: %.3fs\n", write_time);

    slp_image_destroy(&image);
    return 0;
}
