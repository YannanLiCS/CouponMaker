#!/bin/sh
rm *.bc
clang -emit-llvm -c *.c
for $file in *_precomp.bc
do
    /home/yannanli/build2/bin/llvm-link $file NVM.bc -o $file
done
for $file in *_online.bc
do
    /home/yannanli/build2/bin/llvm-link $file NVM.bc -o $file
done

