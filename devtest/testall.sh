#!/bin/bash
for a in *.nsbl
do
    ../bin/nsbl $a
    mv a.out $a.exe
    echo "test on $a" >> testall.log
    ./$a.exe >> testall.log
done
