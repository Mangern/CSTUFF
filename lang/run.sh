#!/bin/bash
ret="$(stdbuf -oL ./langself "$1")"
status=$?
outfile="tmp"

if [ $status -eq 0 ]; then
    printf "%s\n" "$ret" | gcc -xassembler -o $outfile -
    gccstat=$?
    if [ $gccstat -eq 0 ]; then
        ./$outfile
        rm $outfile
    else
        echo "Assembler failed"
        printf "%s\n" "$ret"
    fi
else
    echo "Failed with message:"
    printf "%s\n" "$ret"
fi
