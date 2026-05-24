FROM archlinux:latest


RUN brew install zlib-ng libspng clang clang-tidy llvm
WORKDIR /workspace/
RUN git clone https://github.com/slp-c/slp_png.git
WORKDIR /workspace/slp_png
RUN cmake -B build -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -S .
RUN cmake --build build --config Release
RUN clang-tidy -p build --checks='-*,clang-analyzer-*, -clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling' --warnings-as-errors='*' src/slp_image_transform/slp_image_transform.c src/slp_png_read/slp_png_read_from_file.c src/slp_png_write/slp_png_write_to_file.c
RUN ./build/test
RUN bash -c "shopt -s nullglob; for file in CI_TEST-*.png; do cmp $file CI_TEST.png; if [ \$? -ne 0 ]; then echo 'fail at: cmp $file CI_TEST.png'; exit 1; fi; done; shopt -u nullglob;"

# sudo docker build --no-cache -t slp_png-macOS scripts/macOS
# sudo docker run --rm -it slp_png-macOS
