#!/bin/bash
# Coverage check tool

# target="../generators/PlnX86_64Generator"
target="../PlnDataAllocator"
# target="../models/expressions/PlnDivOperation"
targetnm=${target##*/}
gcovs="${targetnm}.cpp"

g++ -coverage -std=c++11 -c -g ${target}.cpp -o ../objs/${targetnm}.o
make LDFLAGS=-coverage -lgcov
./tester
gcov ../objs/${targetnm}.gcda | grep -A 1 -E ${gcovs}

rm ../objs/${targetnm}.*
rm `ls ./*.gcov | grep -E -v ${gcovs}`

