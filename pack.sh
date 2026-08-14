#!/bin/bash
# script for packing wi releases into .zip files

# pack foreign folder, executable, and shared library into their respective .zip files
# output is "bin/(name).zip"
# $1 - architecture
# $2 - executable
# $3 - shared library
pack() {
    mkdir -p bin
    pushd bin

    rm -rf "wi-$1.zip"
    mkdir -p "foreign"
    zip -r "wi-$1.zip" $2 $3 "foreign"

    popd
}

pack "win64" "wi.exe" "wi.dll" > /dev/null
pack "linux64" "wi" "libwi.so" > /dev/null

echo "done"
