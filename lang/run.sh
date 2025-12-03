#!/bin/bash
ret="$(stdbuf -oL ./langself < "$1")"
status=$?
outfile="tmp"

if [ $status -eq 0 ]; then
    printf "%s\n" "$ret" | gcc -xassembler -o $outfile -
    ./$outfile
else
    echo "Failed with message:"
    printf "%s\n" "$ret"
fi
