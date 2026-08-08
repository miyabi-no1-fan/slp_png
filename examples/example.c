#include <slp_png.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char* argv[]) {
    // default path
    char path[64] = "tests/test_images/10.4-MB.png";
    char path_out[64] = "tests/test_images/new.png";

    // parse input/output path
    for (int i = 1; i < argc; i++) {
        switch (argv[i][0]) {
            case '-': {
                switch (argv[i][1]) {
                    case 'o': {
                        strncpy(path_out, argv[i + 1], 63);
                        i++;
                        break;
                    }
                }
                break;
            }
            default: {
                strncpy(path, argv[i], 63);
                break;
            }
        }
    }

    int ret = 0;

    slp_image_t image;
    {
        FILE* file = fopen(path, "rb");
        int ret = slp_png_read(&image, &(slp_png_io) {.buf = file});
        if (ret != 0)
            return ret;
        fclose(file);
    }

    {
        FILE* file = fopen(path_out, "wb");
        int ret = slp_png_write(&image, &(slp_png_io) {.buf = file});
        if (ret != 0)
            return ret;
        fclose(file);
    }

    slp_image_destroy(&image);
    return 0;
}
