cmake_minimum_required(VERSION 3.20)

project("slp_png" C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(APPLE)
    include_directories("/opt/homebrew/include")
    link_directories("/opt/homebrew/lib")
endif()


add_library("slp_png"
    src/slp_png_read/slp_png_read_from_file.c
    src/slp_png_write/slp_png_write_to_file.c
    src/slp_image_transform/slp_image_transform.c
)

target_include_directories("slp_png" PUBLIC include)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        target_compile_options("slp_png" PUBLIC -mavx2 -msse2 -flto -Wall -Wextra -Werror -pedantic -Wformat -Wno-c98-compat)
    elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
        target_compile_options("slp_png" PUBLIC -mavx2 -msse2 -flto)
    endif()
endif()

find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)
target_link_libraries("slp_png"
    PUBLIC z
    PUBLIC pthread
)



# tests executable
find_package(PkgConfig REQUIRED)
pkg_check_modules(SPNG REQUIRED IMPORTED_TARGET spng)

add_executable("slp_png_perf_test" tests/perf/slp_png_perf_test.c)
add_executable("spng_perf_test"    tests/perf/spng_perf_test.c)

target_link_libraries("slp_png_perf_test" PRIVATE slp_png)
target_link_libraries("spng_perf_test"    PRIVATE spng)

target_compile_options("slp_png_perf_test" PUBLIC -march=native -mtune=native -O3)
target_compile_options("spng_perf_test"    PUBLIC -march=native -mtune=native -O3)


add_executable("test" tests/CI_TEST.c)
target_link_libraries("test"
    PRIVATE slp_png
    PRIVATE pthread
)
