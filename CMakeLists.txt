cmake_minimum_required(VERSION 3.19)

project("slp" C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(ENABLE_OPT "Enable architecture-specific optimizations" ON)
option(SLP_PNG_SHARED "Build shared lib" ON)
option(SLP_PNG_STATIC "Build static lib" ON)
option(SLP_IMAGE_TRANSFORM "Build with expierimental module image transform (it will not work)" OFF)
option(BUILD_TEST "Build test" OFF)
option(BUILD_EXAMPLE "Build example" OFF)

if (SLP_PNG_SHARED)
    if (SLP_IMAGE_TRANSFORM)
        add_library("slp_png" SHARED
            src/slp_png_read/slp_png_read_from_file.c
            src/slp_png_write/slp_png_write_to_file.c
            src/slp_image_transform/slp_image_transform.c
        )
        find_package(Threads REQUIRED)
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

    find_package(ZLIB REQUIRED)
    target_link_libraries("slp_png" PRIVATE ZLIB::ZLIB)

    if (ENABLE_OPT)
        target_compile_options("slp_png" PRIVATE -O3 -ffast-math -march=native -mtune=native -DNDEBUG -flto=auto)
    endif()

    target_include_directories("slp_png" PUBLIC include)

    if (BUILD_EXAMPLE)
        add_executable("example" tests/example.c)
        target_link_libraries("example" PRIVATE "slp_png")
        if (WIN32)
            set_target_properties("example" PROPERTIES POSITION_INDEPENDENT_CODE ON)
        endif()
    endif()

    if (BUILD_TEST)
        add_executable("test" tests/test.c)
        find_package(Threads REQUIRED)
        target_link_libraries("test" PRIVATE "slp_png" Threads::Threads)
        if (WIN32)
            set_target_properties("test" PROPERTIES POSITION_INDEPENDENT_CODE ON)
        endif()
    endif()
endif()

if (SLP_PNG_STATIC)
    if (SLP_IMAGE_TRANSFORM)
        add_library("slp_png-static" STATIC
            src/slp_png_read/slp_png_read_from_file.c
            src/slp_png_write/slp_png_write_to_file.c
            src/slp_image_transform/slp_image_transform.c
        )
        find_package(Threads REQUIRED)
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

    find_package(ZLIB REQUIRED)
    target_link_libraries("slp_png-static" PRIVATE ZLIB::ZLIB)

    if (ENABLE_OPT)
        target_compile_options("slp_png-static" PRIVATE -O3 -ffast-math -march=native -mtune=native -DNDEBUG)
    endif()

    target_include_directories("slp_png-static" PUBLIC include)

    if (BUILD_EXAMPLE)
        add_executable("example-static" tests/example.c)
        target_link_libraries("example-static" PRIVATE "slp_png-static")
        if (WIN32)
            set_target_properties("example-static" PROPERTIES POSITION_INDEPENDENT_CODE ON)
        endif()
    endif()

    if (BUILD_TEST)
        add_executable("test-static" tests/test.c)
        find_package(Threads REQUIRED)
        target_link_libraries("test-static" PRIVATE "slp_png-static" Threads::Threads)
        if (WIN32)
            set_target_properties("test-static" PROPERTIES POSITION_INDEPENDENT_CODE ON)
        endif()
    endif()
endif()
