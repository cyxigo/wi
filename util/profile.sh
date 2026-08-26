#!/bin/bash
# script for profiling wi via test/benchmark tests
# to run perf you need kernel.perf_event_paranoid and i always forget command for it so:
# sudo sysctl -w kernel.perf_event_paranoid=-1

count=0
total=$(ls test/benchmark/*.wi | wc -l)

for file in test/benchmark/*.wi; do
    ((count++))

    base=$(basename "$file" ".${file##*.}")
    data="$base.data"
    txt="$base.txt"

    perf record -g -o $data wi $file > /dev/null 2>&1
    perf report -i $data > $txt
    cat $file >> $txt
    
    echo "[$count/$total] profiled $file > $txt"

    rm -f $data
done

echo "done"
