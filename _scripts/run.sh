#! /bin/bash
set -e

_scripts/build_lib.sh
_scripts/build_exe.sh "$@"
echo "Running $3...
"
$3
