#!/bin/bash
# script for packing wi releases into .zip files, output is "bin/wi-(name).zip"
set -e

cd bin
mkdir -p lib

# pack lib folder, executable, and shared library into one .zip
# $1 - release name
# $2 - build directory (inside bin)
# $3 - executable
# $4 - shared library
pack() {
    rm -f "wi-$1.zip"
    zip -qj "wi-$1.zip" "$2/$3" "$2/$4"
    zip -qr "wi-$1.zip" lib
}

pack "win64" "." "wi.exe" "wi.dll"
pack "win32" "x86" "wi.exe" "wi.dll"
pack "linux64" "." "wi" "libwi.so"
pack "linux-arm64" "arm64" "wi" "libwi.so"

echo "done"
