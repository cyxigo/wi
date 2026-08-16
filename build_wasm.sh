#!/bin/bash
# script mirroring build.sh, but builds wasm (not included in build.sh)
mode="release"

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
        *)
            shift
            ;;
    esac
done

xmake f -c -m $mode -p wasm > /dev/null
xmake

echo "done"
