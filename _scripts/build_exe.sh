#! /bin/bash
set -e

if [[ "$1" == "--help" ]]; then

    echo "./build_exe.sh [linking type] [.c file] [output file] [extra compile flags]
    
    linking type:
        static
        dynamic
    
    extra compile flags: all passes directly to gcc ( default flags are -O3 -march=native -mtune=native -lz -pthread -flto )

    example: ./build_exe.sh static test.c test -lz -lm"

    exit 0;

fi



if [[ "$1" == "static" ]]; then

    FILE="$2"
    OUT="$3"
    shift 3

    echo "=> use static linking for slp_png
compile: $FILE
out: $OUT
flags: $@"

    gcc "$FILE" \
        -I include \
        -L build \
        -o "$OUT" \
        -O3 -march=native -mtune=native -flto \
        -lz -pthread \
        -Wl,-Bstatic -lslp_png -Wl,-Bdynamic \
        "$@"
    

elif [[ "$1" == "dynamic" ]]; then

    FILE="$2"
    OUT="$3"
    shift 3

    echo "=> use dynamic linking for slp_png
compile: $FILE
out: $OUT
flags: $@"

    gcc "$FILE" \
        -I include \
        -L build \
        -o "$OUT" \
        -O3 -march=native -mtune=native -flto \
        -lz -pthread -lslp_png \
        -Wl,-rpath,build \
        "$@"

else
    echo "unknown linking type"
    exit -1
fi

