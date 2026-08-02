#include <slp_png.h>

// default path
char path[] = "tests/test_images/10.4-MB.png";
char path_out[] = "tests/test_images/new.png";

int main(void) {
    slp_image_t image = slp_png_read(path);
    if (image.pixels == NULL) return 1;

    int ret = slp_png_write(image, path_out);
    if (ret != 0) return 1;

    slp_image_destroy(&image);
    return 0;
}
