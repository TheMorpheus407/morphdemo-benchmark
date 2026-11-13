#!/bin/sh
# usage: build.sh name   -> ../../build/name (flat dynamic ELF)
set -e
N=$1; B=../../build
CF="-Os -std=gnu11 -fno-pic -no-pie -ffreestanding -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -fomit-frame-pointer -ffast-math -fno-ident -fno-tree-loop-distribute-patterns -msse4.1 -Wall -Wno-unused-function"
gcc $CF -c $N.c -o $B/$N.o
gcc -c ../common/hdr.S -o $B/hdr.o
ld -T ../common/link.ld -o $B/$N.elf $B/hdr.o $B/$N.o
objcopy -O binary $B/$N.elf $B/$N
chmod +x $B/$N
ls -l $B/$N
