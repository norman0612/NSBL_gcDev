#!/bin/bash
for a in *.nsbl
do
    ../bin/nsbl $a
    mv a.out $a.exe
    echo "test on $a" 2>&1  >> testall.log
    ./$a.exe 2>&1 >> testall.log
done
