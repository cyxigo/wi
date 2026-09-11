#!/bin/bash
# script for packing wi releases into .zip files

# pack lib folder, executable, and shared library into their respective .zip files
# output is "bin/(name).zip"
# $1 - architecture
# $2 - executable
# $3 - shared library
set -e

pack() {
    mkdir -p bin
    pushd bin

    rm -rf "wi-$1.zip"
    mkdir -p "lib"
    zip -r "wi-$1.zip" $2 $3 "lib"

    popd
}

pack "win64" "wi.exe" "wi.dll" > /dev/null
pack "linux64" "wi" "libwi.so" > /dev/null

echo "done"
