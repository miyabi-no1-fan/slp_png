#! /bin/bash
set -e

if [[ "$1" == "--help" ]]; then

    echo "build_exe.sh [linking type] [.c file] [output file] [extra compile flags]
    
    linking type:
        static
        dynamic
    
    extra compile flags: all passes directly to gcc ( default flags are -O3 -march=native -mtune=native -DNDEBUG -flto -lz -pthread )

    example: build_exe.sh static test.c test -mno-avx512f -lm"

    exit 0;

fi

LINK="$1"
FILE="$2"
OUT="$3"
shift 3

if [[ "$OUT" == "_" ]]; then
    OUT="build/"$(basename "$FILE" .c)
fi

if [[ "$LINK" == "static" ]]; then

    echo "link: static (libslp_png only)
compile: $FILE
out: $OUT"

    gcc "$FILE" \
        -I include \
        -L build \
        -o "$OUT" \
        -O3 -march=native -mtune=native -flto -DNDEBUG \
        -lz -pthread \
        -Wl,-Bstatic -lslp_png -Wl,-Bdynamic \
        "$@"
    

elif [[ "$LINK" == "dynamic" ]]; then

    echo "link: dynamic (libslp_png only)
compile: $FILE
out: $OUT"

    gcc "$FILE" \
        -I include \
        -L build \
        -o "$OUT" \
        -O3 -march=native -mtune=native -flto -DNDEBUG \
        -lz -pthread -lslp_png \
        -Wl,-rpath,build \
        "$@"

else
    echo "compile: $FILE
out: $OUT"

    gcc "$FILE" \
        -L build \
        -o "$OUT" \
        -O3 -march=native -mtune=native -flto -DNDEBUG \
        "$@"
fi
