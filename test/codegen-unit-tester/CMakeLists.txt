cmake_minimum_required(VERSION 3.16)

add_executable(codegen-unit-tester
    basicTests.cpp
    deserializeLowerTests.cpp
    ../../src/codegen/PlnRegAlloc.cpp
    ../../src/codegen/PlnDeserialize.cpp
    ../../src/codegen/PlnVCodeGen.cpp
    ../../src/codegen/PlnVCodeGenExpr.cpp
    ../../src/codegen/PlnVCodeGenCall.cpp
    ../../src/codegen/PlnCodegenMessage.cpp
)
target_include_directories(codegen-unit-tester PRIVATE ../../src/codegen ../../lib/json/single_include)
target_link_libraries(codegen-unit-tester PRIVATE gtest_main)

include(GoogleTest)
gtest_discover_tests(codegen-unit-tester)
