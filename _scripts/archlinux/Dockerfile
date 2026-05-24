FROM archlinux:latest


RUN pacman -Syu --noconfirm --ask=4 --needed base base-devel zlib-ng-compat libspng gcc clang valgrind llvm git debuginfod ca-certificates cmake make
ENV DEBUGINFOD_URLS="https://debuginfod.archlinux.org"
WORKDIR /workspace
RUN git clone https://github.com/slp-c/slp_png.git
WORKDIR /workspace/slp_png
RUN cmake -B build -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -S .
RUN cmake --build build --config Release
RUN clang-tidy -p build --checks='-*,clang-analyzer-*, -clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling' --warnings-as-errors='*' src/slp_image_transform/slp_image_transform.c src/slp_png_read/slp_png_read_from_file.c src/slp_png_write/slp_png_write_to_file.c
RUN rm build
RUN _scripts/archlinux/build_lib.sh
RUN _scripts/archlinux/build_exe.sh dynamic tests/CI_TEST.c build/test -Ofast
RUN build/test
RUN _scripts/archlinux/build_exe.sh dynamic tests/CI_TEST.c build/test -Ofast -mno-avx512f
RUN valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --track-origins=yes --error-exitcode=1 build/test --valgrind
RUN _scripts/archlinux/build_exe.sh dynamic tests/perf/slp_png_perf_test.c build/slp -Ofast
RUN _scripts/archlinux/build_exe.sh dynamic tests/perf/spng_perf_test.c build/spng -lspng -Ofast
RUN build/slp
RUN build/spng


# sudo docker build -t slp_png-linux _scripts/archlinux
