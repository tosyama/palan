cmake_minimum_required(VERSION 3.16)

enable_testing()

add_executable(sa-tester basicTests.cpp callTests.cpp arrayTests.cpp structTests.cpp pointerTests.cpp cTests.cpp moduleTests.cpp errorTests.cpp errorArrayTests.cpp errorStructTests.cpp errorCTests.cpp saTestUtil.cpp)
target_link_libraries(sa-tester PRIVATE gtest_main testbase)

include(GoogleTest)
