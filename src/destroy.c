#include "slp_png.h"

void slp_image_destroy(slp_image_t* image) {
    if (image != NULL) {
        if (image->pixels != NULL)
            SLP_ALIGNED_FREE(image->pixels);
        image->pixels = NULL;
    }
}
