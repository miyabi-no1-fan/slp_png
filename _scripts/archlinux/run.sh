#! /bin/bash
set -e

_scripts/archlinux/build_lib.sh
_scripts/archlinux/build_exe.sh "$@"
echo "Running $3...
"
$3
