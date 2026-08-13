#!/bin/bash
# script for packing wi releases into .zip files

# pack foreign folder, executable, and shared library into their respective .zip files
# output is "bin/(name).zip"
# $1 - architecture
# $2 - executable
# $3 - shared library
pack() {
    local dir="bin/wi-$1"
    mkdir -p "$dir/foreign"

    [ -f "bin/$2" ] && cp "bin/$2" "$dir/"
    [ -f "bin/$3" ] && cp "bin/$3" "$dir/"
    [ -d "foreign" ] && [ "$(ls -A foreign)" ] && cp -r foreign/* "$dir/foreign/"

    zip -r "$dir.zip" "$dir"
    rm -rf "$dir"
}

pack "win64" "wi.exe" "libwi.dll" > /dev/null
pack "linux64" "wi" "libwi.so" > /dev/null

echo "done"
