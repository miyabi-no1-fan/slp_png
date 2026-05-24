#! /bin/bash
set -e

if [[ "$1" == "--help" ]]; then

    echo "build_exe.sh [linking type] [.c file] [output file] [extra compile flags]
    
    linking type:
        static
        dynamic
    
    extra compile flags: all passes directly to gcc ( default flags are -O3 -march=native -mtune=native -DNDEBUG -flto -lz -pthread )

    example: build_exe.sh static test.c test -lz -lm"

    exit 0;

fi



if [[ "$1" == "static" ]]; then

    FILE="$2"
    OUT="$3"
    shift 3

    echo "link: static (libslp_png only)
compile: $FILE
out: $OUT
flags: $@"

    gcc "$FILE" \
        -I include \
        -L build \
        -o "$OUT" \
        -O3 -march=native -mtune=native -flto -DNDEBUG \
        -lz -pthread \
        -Wl,-Bstatic -lslp_png -Wl,-Bdynamic \
        "$@"
    

elif [[ "$1" == "dynamic" ]]; then

    FILE="$2"
    OUT="$3"
    shift 3

    echo "link: dynamic (libslp_png only)
compile: $FILE
out: $OUT
flags: $@"

    gcc "$FILE" \
        -I include \
        -L build \
        -o "$OUT" \
        -O3 -march=native -mtune=native -flto -DNDEBUG \
        -lz -pthread -lslp_png \
        -Wl,-rpath,build \
        "$@"

else
    echo "unknown linking type"
    exit -1
fi

