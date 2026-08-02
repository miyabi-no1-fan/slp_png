cmake_minimum_required(VERSION 3.19)

project("slp_png" C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    message(FATAL_ERROR "slp_png requires GCC or Clang. Current compiler: ${CMAKE_C_COMPILER_ID}")
endif()

option(ENABLE_OPTS "Enable architecture-specific optimizations" ON)
option(SLP_PNG_SHARED "Build shared lib" ON)
option(SLP_PNG_STATIC "Build static lib" ON)
option(SLP_IMAGE_TRANSFORM "Build with expierimental module image transform (it will not work)" OFF)
option(BUILD_TEST "Build test" OFF)
option(BUILD_EXAMPLE "Build example" OFF)

set(SRC 
    src/slp_png_read/slp_png_read_from_file.c
    src/slp_png_write/slp_png_write_to_file.c
    src/slp_png_write/slp_png_filter.c
)
set(OPTIMIZE_FLAGS -O3 -ffast-math -march=native -mtune=native -DNDEBUG)

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
            set(SPNG_TARGET spng::spng_static)
        else()
            find_package(PkgConfig REQUIRED)
            set(PKG_CONFIG_USE_STATIC_LIBS ON)
            pkg_check_modules(SPNG IMPORTED_TARGET spng)
            if (SPNG_FOUND)
                set(SPNG_TARGET PkgConfig::SPNG)
            else()
                set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
                include(FetchContent)
                FetchContent_Declare(
                    libspng
                    GIT_REPOSITORY https://github.com/randy408/libspng.git
                    GIT_TAG v0.7.4
                )
                FetchContent_MakeAvailable(libspng)
                set(SPNG_TARGET spng_static)

                if (ENABLE_OPTS)
                    target_compile_options(${SPNG_TARGET} PRIVATE ${OPTIMIZE_FLAGS})
                    if (NOT WIN32)
                        set_target_properties(${SPNG_TARGET} PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
                    endif()
                endif()

                unset(BUILD_EXAMPLES)
            endif()
            unset(PKG_CONFIG_USE_STATIC_LIBS)
        endif()
    endif()
endif()

if (SLP_PNG_SHARED)
    if (SLP_IMAGE_TRANSFORM)
        add_library("slp_png" SHARED ${SRC}
            src/slp_image_transform/slp_image_transform.c
        )
        target_link_libraries("slp_png"
            PRIVATE Threads::Threads
            PRIVATE m
        )
    else()
        add_library("slp_png" SHARED ${SRC})
    endif()
endif()

if (SLP_PNG_STATIC)
    if (SLP_IMAGE_TRANSFORM)
        add_library("slp_png-static" STATIC
            ${SRC}
            src/slp_image_transform/slp_image_transform.c
        )
        target_link_libraries("slp_png-static"
            PRIVATE Threads::Threads
            PRIVATE m
        )
    else()
        add_library("slp_png-static" STATIC ${SRC})
    endif()
endif()

if (TARGET "slp_png")
    target_link_libraries("slp_png" PRIVATE ZLIB::ZLIB)
    target_include_directories("slp_png" PUBLIC include)
    if (ENABLE_OPTS)
        target_compile_options("slp_png" PRIVATE ${OPTIMIZE_FLAGS})
    endif()
endif()

if (TARGET "slp_png-static")
    target_link_libraries("slp_png-static" PRIVATE ZLIB::ZLIB)
    target_include_directories("slp_png-static" PUBLIC include)
    if (ENABLE_OPTS)
        target_compile_options("slp_png-static" PRIVATE ${OPTIMIZE_FLAGS})
    endif()
endif()

if (ENABLE_OPTS AND NOT WIN32)
    if (TARGET "slp_png")
        set_target_properties("slp_png" PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
    if (TARGET "slp_png-static")
        set_target_properties("slp_png-static" PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endif()

if (BUILD_EXAMPLE)
    if (TARGET "slp_png")
        add_executable("example" tests/example.c)
        target_link_libraries("example" PRIVATE "slp_png")
    endif()
    if (TARGET "slp_png-static")
        add_executable("example-static" tests/example.c)
        target_link_libraries("example-static" PRIVATE "slp_png-static")
    endif()
endif()

if (BUILD_TEST)
    if (TARGET "slp_png")
        add_executable("test" tests/test.c)
        target_link_libraries("test" PRIVATE "slp_png" Threads::Threads ${SPNG_TARGET})

        add_executable("slp_benchmark" benches/slp_rw.c)
        target_link_libraries("slp_benchmark" PRIVATE "slp_png")
    endif()
    if (TARGET "slp_png-static")
        add_executable("test-static" tests/test.c)
        target_link_libraries("test-static" PRIVATE "slp_png-static" Threads::Threads ${SPNG_TARGET})
    endif()
endif()
