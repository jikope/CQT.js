#!/bin/sh
set -xe

clang -O3 -Wall -Wextra -Wpedantic -Wswitch-enum -fno-vectorize -msimd128 --target=wasm32 -c cqt.c
wasm-ld --no-entry --export-dynamic --allow-undefined  --gc-sections -O3 --lto-O3 -o cqt.wasm cqt.o

cp cqt.wasm tests
