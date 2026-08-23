#!/bin/bash
# script to build both windows and linux versions of wi
# supports two flags being -r/--release for release and -d/--debug for debug

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

xmake f -c -m $mode -p windows --toolchain=mingw > /dev/null
xmake

xmake f -c -m $mode -p linux > /dev/null
xmake

echo "done"
