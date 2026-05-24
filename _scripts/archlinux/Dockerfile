FROM archlinux:latest


RUN pacman -Syu --noconfirm --ask=4 --needed base base-devel zlib-ng-compat libspng cmake gcc clang ninja valgrind llvm git
WORKDIR /workspace
RUN git clone https://github.com/slp-c/slp_png.git
WORKDIR /workspace/slp_png
RUN _scripts/run.sh dynamic tests/CI_TEST.c build/test -Ofast -mno-avx512f
RUN valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --track-origins=yes --error-exitcode=1 build/test
RUN _scripts/run.sh dynamic tests/perf/slp_png_perf_test.c build/slp -Ofast
RUN _scripts/run.sh dynamic tests/perf/spng_perf_test.c build/spng -Ofast
RUN build/slp
RUN build/spng


# sudo docker build -t slp_png-linux _scripts/archlinux
# sudo docker run --rm -it slp_png-linux
