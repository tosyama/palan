#!/bin/bash
# Coverage check tool

# target="../generators/PlnX86_64Generator"
# target="../generators/PlnX86_64DataAllocator"
# target="../PlnDataAllocator"
# target="../models/PlnObjectLiteral"
# target="../models/PlnBlock"
target="../PlnModelTreeBuilder"
# target="../PlnGenerator"
# target="../models/expressions/PlnArrayValue"
# target="../models/expressions/PlnFunctionCall"
# target="../models/types/PlnArrayValueType"
# target="../models/types/PlnFixedArrayType"
#target="../models/expressions/assignitem/PlnAssignItem"
targetnm=${target##*/}
gcovs="${targetnm}.cpp"
#gcovs="PlnAssignArrayValue.h"
#gcovs="PlnArrayValueType.h"
#gcovs="PlnDstCopyObjectItem.h"

g++ -coverage -std=c++11 -c -g ${target}.cpp -o ../objs/${targetnm}.o
make LDFLAGS=-coverage -lgcov
./tester
gcov ../objs/${targetnm}.gcda | grep -A 1 -E ${gcovs}

rm ../objs/${targetnm}.*
rm $(ls ./*.gcov | grep -E -v ${gcovs})

