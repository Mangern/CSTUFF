#!/bin/bash
infile="./langc-impl/langc.lang"
compiler="./langc-impl/v2/langc"
ret="$(stdbuf -oL $compiler < $infile)"
status=$?
outfile="langself"

if [ $status -eq 0 ]; then
    printf "%s\n" "$ret" | gcc -xassembler -o $outfile -
    echo "Built $outfile from $infile using $compiler"
else
    echo "Failed with message:"
    printf "%s\n" "$ret"
fi
