#! /bin/bash
set -e
# Assume you have zlib which normally should be exist

mkdir -p build

echo "=> compiling src files.."
gcc -c src/slp_image_transform/slp_image_transform.c -O3 -march=native -mtune=native -I include -o build/slp_image_transform.o
gcc -c src/slp_png_read/slp_png_read_from_file.c -O3 -march=native -mtune=native -I include -o build/slp_png_read_from_file.o
gcc -c src/slp_png_write/slp_png_write_to_file.c -O3 -march=native -mtune=native -I include -o build/slp_png_write_to_file.o

echo "=> genrating build/libslp_png.so..."
# DYNAMIC LINKING:
gcc -shared build/slp_image_transform.o build/slp_png_read_from_file.o build/slp_png_write_to_file.o -lz -flto -o build/libslp_png.so

echo "=> genrating build/libslp_png.a..."
# STATIC LINKING:
ar rcs build/libslp_png.a build/slp_image_transform.o build/slp_png_read_from_file.o build/slp_png_write_to_file.o

echo "cleaning up .o"
# CLEANUP:
rm build/slp_image_transform.o build/slp_png_read_from_file.o build/slp_png_write_to_file.o
