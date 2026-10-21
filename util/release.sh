#!/bin/bash
# script to build wi (into ./bin) and pack release zips (into ./dist)
# for windows + linux by default, or wasm with -w/--wasm
# supports -r/--release, -d/--debug, -w/--wasm
set -e

mode="release"
wasm=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -r|--release)
            mode="release"
            shift
            ;;
        -d|--debug)
            mode="debug"
            shift
            ;;
        -w|--wasm)
            wasm=true
            shift
            ;;
        *)
            shift
            ;;
    esac
done

# configure from scratch, build, and (unless wasm) pack into ./dist
# $@ - extra xmake config flags
build() {
    xmake f -c -m "$mode" "$@" > /dev/null
    xmake

    if ! $wasm; then
        xmake pack -o dist
    fi
}

if $wasm; then
    build -p wasm
else
    build -p windows --toolchain=mingw
    build -p linux
    build -p windows -a x86 --toolchain=mingw
    build -p linux -a arm64 --cross=aarch64-linux-gnu-
fi

echo "done"
