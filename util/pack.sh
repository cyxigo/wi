#!/bin/bash
# script for packing wi releases into .zip files

# pack lib folder, executable, and shared library into their respective .zip files
# output is "bin/(name).zip"
# $1 - release name
# $2 - build directory (inside bin)
# $3 - executable
# $4 - shared library
set -e

pack() {
    mkdir -p bin
    pushd bin

    rm -rf "wi-$1.zip"
    mkdir -p "lib"
    zip -j "wi-$1.zip" "$2/$3" "$2/$4"
    zip -r "wi-$1.zip" "lib"

    popd
}

pack "win64" "." "wi.exe" "wi.dll" > /dev/null
pack "win32" "x86" "wi.exe" "wi.dll" > /dev/null
pack "linux64" "." "wi" "libwi.so" > /dev/null
pack "linux-arm64" "arm64" "wi" "libwi.so" > /dev/null

echo "done"
