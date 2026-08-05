#include <slp_png.h>
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

    slp_image_t a = slp_png_read(path);
    if (!a.pixels)
        return 1;

    int ret = slp_png_write(a, path_out);
    if (ret != 0)
        return 1;

    slp_image_destroy(&a);
    return 0;
}
