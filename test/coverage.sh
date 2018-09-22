#!/bin/bash
# Coverage check tool

target="../models/expressions/PlnAssignment"
targetnm=${target##*/}

g++ -coverage -std=c++11 -c -g ${target}.cpp -o ../objs/${targetnm}.o
make LDFLAGS=-coverage -lgcov
./tester
gcov ../objs/${targetnm}.gcda | sed -n 1,2p
# vi ./${targetnm}.cpp.gcov
rm ../objs/${targetnm}.*
rm `ls ./*.gcov | grep -v ${targetnm}.cpp`

