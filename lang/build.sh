#!/bin/bash
infile="./langc-impl/langc.lang"
ret="$(stdbuf -oL ./langc-impl/v1/langc < $infile)"
status=$?
outfile="langself"

if [ $status -eq 0 ]; then
    printf "%s\n" "$ret" | gcc -xassembler -o $outfile -
    echo "Built $outfile from $infile"
else
    echo "Failed with message:"
    printf "%s\n" "$ret"
fi
