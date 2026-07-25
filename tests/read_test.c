// THIS IS A COPY FROM https://github.com/randy408/libspng/blob/master/examples/example.c
#include <assert.h>
#include <inttypes.h>
#include <slp_png.h>
#include <spng.h>
#include <stdio.h>
#include <time.h>

const char path[] = "tests/test_images/10.4-MB.png";

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

    assert(ihdr.color_type != SPNG_COLOR_TYPE_INDEXED);

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

int main()
{
    size_t size = 0;
    uint8_t* image = read_png(path, &size);

    slp_image_t a = slp_png_read(path);
    if (a.pixels == NULL) {printf("\nread failed: %d\n", a.bit_depth);return 1;}

    assert(a.image_size == size);

    for (size_t i = 0; i < a.image_size; i++) {
        if (a.pixels[i] != image[i]) {
            fprintf(stderr, "slp_png_read pixels mismatch");
            fflush(stderr);
            assert(false);
        }
    }

    slp_image_destroy(&a);
    free(image);
    return 0;
}
