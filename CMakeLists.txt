cmake_minimum_required(VERSION 3.19)

project("slp_png" C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(ENABLE_OPT "Enable architecture-specific optimizations" ON)
option(SLP_PNG_SHARED "Build shared lib" ON)
option(SLP_PNG_STATIC "Build static lib" ON)
option(SLP_IMAGE_TRANSFORM "Build with expierimental module image transform (it will not work)" OFF)
option(BUILD_TEST "Build test" OFF)
option(BUILD_EXAMPLE "Build example" OFF)

if (SLP_PNG_SHARED OR SLP_PNG_STATIC)
    find_package(ZLIB REQUIRED)

    if (SLP_IMAGE_TRANSFORM)
        find_package(Threads REQUIRED)
    endif()

    if (BUILD_TEST)
        find_package(Threads REQUIRED)
        include(CMakeFindDependencyMacro)
        find_package(SPNG CONFIG)
        if (SPNG_FOUND)
            set(SPNG_TARGET spng::spng)
        else()
            find_package(PkgConfig REQUIRED)
            pkg_check_modules(SPNG REQUIRED IMPORTED_TARGET spng)
            set(SPNG_TARGET PkgConfig::SPNG)
        endif()
    endif()
endif()

if (SLP_PNG_SHARED)
    if (SLP_IMAGE_TRANSFORM)
        add_library("slp_png" SHARED
            src/slp_png_read/slp_png_read_from_file.c
            src/slp_png_write/slp_png_write_to_file.c
            src/slp_image_transform/slp_image_transform.c
        )
        target_link_libraries("slp_png"
            PRIVATE Threads::Threads
            PRIVATE m
        )
    else()
        add_library("slp_png" SHARED
            src/slp_png_read/slp_png_read_from_file.c
            src/slp_png_write/slp_png_write_to_file.c
        )
    endif()

    target_link_libraries("slp_png" PRIVATE ZLIB::ZLIB)

    if (ENABLE_OPT)
        target_compile_options("slp_png" PRIVATE -O3 -ffast-math -march=native -mtune=native -DNDEBUG)
        set_target_properties("slp_png" PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()

    target_include_directories("slp_png" PUBLIC include)

    if (BUILD_EXAMPLE)
        add_executable("example" tests/example.c)
        target_link_libraries("example" PRIVATE "slp_png")
    endif()

    if (BUILD_TEST)
        add_executable("test" tests/test.c)
        target_link_libraries("test" PRIVATE "slp_png" Threads::Threads ${SPNG_TARGET})
    endif()
endif()

if (SLP_PNG_STATIC)
    if (SLP_IMAGE_TRANSFORM)
        add_library("slp_png-static" STATIC
            src/slp_png_read/slp_png_read_from_file.c
            src/slp_png_write/slp_png_write_to_file.c
            src/slp_image_transform/slp_image_transform.c
        )
        target_link_libraries("slp_png-static"
            PRIVATE Threads::Threads
            PRIVATE m
        )
    else()
        add_library("slp_png-static" STATIC
            src/slp_png_read/slp_png_read_from_file.c
            src/slp_png_write/slp_png_write_to_file.c
        )
    endif()

    target_link_libraries("slp_png-static" PRIVATE ZLIB::ZLIB)

    if (ENABLE_OPT)
        target_compile_options("slp_png-static" PRIVATE -O3 -ffast-math -march=native -mtune=native -DNDEBUG)
        set_target_properties("slp_png-static" PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()

    target_include_directories("slp_png-static" PUBLIC include)

    if (BUILD_EXAMPLE)
        add_executable("example-static" tests/example.c)
        target_link_libraries("example-static" PRIVATE "slp_png-static")
    endif()

    if (BUILD_TEST)
        add_executable("test-static" tests/test.c)
        target_link_libraries("test-static" PRIVATE "slp_png-static" Threads::Threads ${SPNG_TARGET})
    endif()
endif()
