cmake_minimum_required(VERSION 3.20)

project(slp_png CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SLP_PNG_DEBUG_FLAGS "" OFF)
option(SLP_PNG_LTO "Enable link-time optimization" ON)
option(SLP_PNG_BUILD_TEST "Build test" OFF)
option(SLP_PNG_BUILD_BENCH "Build benchmarks" OFF)
option(SLP_PNG_BUILD_FUZZER "Build libFuzzer harness (requires clang)" OFF)

set(DEBUG_FLAGS -Wall -Wextra -Wpedantic -Werror -g -Og -fno-omit-frame-pointer -fsanitize=address,undefined)

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

find_package(ZLIB REQUIRED)

add_library(${PROJECT_NAME} ${SRC})
target_link_libraries(${PROJECT_NAME} PRIVATE ZLIB::ZLIB)
target_include_directories(${PROJECT_NAME} PUBLIC include)
target_include_directories(${PROJECT_NAME} PRIVATE src)
if(SLP_PNG_DEBUG_FLAGS)
    target_compile_options(${PROJECT_NAME} PUBLIC ${DEBUG_FLAGS})
    target_link_options(${PROJECT_NAME} PUBLIC ${DEBUG_FLAGS})
endif()
if(SLP_PNG_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_output)
    if(ipo_supported)
        set_target_properties(${PROJECT_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message("warning: SLP_PNG_LTO is ON but LTO is not supported.")
    endif()
endif()

if(SLP_PNG_BUILD_TEST OR SLP_PNG_BUILD_BENCH)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    include(FetchContent)
    FetchContent_Declare(
        spng
        GIT_REPOSITORY https://github.com/randy408/libspng.git
        GIT_TAG v0.7.4
    )
    FetchContent_MakeAvailable(spng)
endif()

if(SLP_PNG_BUILD_TEST)
    add_executable(test tests/test.cpp)
    target_link_libraries(test PRIVATE ${PROJECT_NAME} spng_static)
endif()

if(SLP_PNG_BUILD_BENCH)
    add_executable(benchmark tests/benchmark.cpp)
    target_link_libraries(benchmark PRIVATE ${PROJECT_NAME} spng_static)
endif()

if(SLP_PNG_BUILD_FUZZER)
    target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=fuzzer-no-link,address,undefined)
    target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=fuzzer-no-link,address,undefined)

    add_executable(fuzzer tests/fuzzer.cpp)
    target_link_libraries(fuzzer PRIVATE ${PROJECT_NAME})
    target_compile_options(fuzzer PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_options(fuzzer PRIVATE -fsanitize=fuzzer,address,undefined)
endif()