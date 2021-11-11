#!/bin/bash
# Coverage check tool

# target="../generators/PlnX86_64Generator"
# target="../generators/PlnX86_64DataAllocator"
# target="../generators/PlnX86_64RegisterMachine"
# target="../generators/PlnX86_64RegisterSave"
# target="../PlnDataAllocator"
# target="../models/PlnObjectLiteral"
# target="../models/PlnBlock"
# target="../models/PlnModule"
# target="../models/PlnType"
# target="../models/PlnFunction"
# target="../PlnMessage"
#target="../PlnModelTreeBuilder"
# target="../PlnGenerator"
# target="../models/PlnExpression"
# target="../models/expressions/PlnMulOperation"
target="../models/expressions/PlnArrayValue"
# target="../models/expressions/PlnFunctionCall"
# target="../models/expressions/PlnBoolExpression"
# target="../models/expressions/PlnBoolOperation"
# target="../models/expressions/PlnCmpOperation"
# target="../models/expressions/PlnReferenceValue"
# target="../models/types/PlnArrayValueType"
# target="../models/types/PlnFixedArrayType"
# target="../models/types/PlnStructType"
# target="../models/expressions/assignitem/PlnAssignItem"
targetnm=${target##*/}
gcovs="${targetnm}.cpp"
#gcovs="PlnAssignArrayValue.h"
#gcovs="PlnArrayValueType.h"
#gcovs="PlnDstCopyObjectItem.h"
#gcovs="PlnAssignWorkValsItem.h"
#gcovs="PlnAssignArrayValue_IndirectVar.h"

g++ -coverage -std=c++11 -c -g ${target}.cpp -o ../objs/${targetnm}.o
make LDFLAGS=-coverage -lgcov
./tester
gcov ../objs/${targetnm}.gcda | grep -A 1 -E ${gcovs}

rm ../objs/${targetnm}.*
rm $(ls ./*.gcov | grep -E -v ${gcovs})

