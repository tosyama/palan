cmake_minimum_required(VERSION 3.16)

enable_testing()

add_executable(std-tester utf8Tests.cpp rtTests.cpp)
target_link_libraries(std-tester PRIVATE gtest_main testbase)
add_dependencies(std-tester palan-std)

include(GoogleTest)
