#!/bin/bash
# script to build wi, for windows + linux by default, or wasm with -w/--wasm
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

if $wasm; then
    xmake f -c -m $mode -p wasm > /dev/null
    xmake
else
    xmake f -c -m $mode -p windows --toolchain=mingw > /dev/null
    xmake

    xmake f -c -m $mode -p linux > /dev/null
    xmake
fi

echo "done"
