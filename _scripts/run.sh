#! /bin/bash
set -e

./scripts/build_lib.sh
./scripts/build_exe.sh "$@"
echo "Running $3...
"
$3
