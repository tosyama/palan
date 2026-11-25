cmake_minimum_required(VERSION 3.16)
project(palanx)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS src/common/PlnDefs.h)
file(STRINGS src/common/PlnDefs.h PALAN_VERSION_LINE REGEX "#define PALAN_VERSION ")
string(REGEX REPLACE ".*\"(.*)\".*" "\\1" PALAN_VERSION "${PALAN_VERSION_LINE}")

file(STRINGS src/common/PlnDefs.h PALAN_TARGET_LINE REGEX "#define PALAN_TARGET ")
string(REGEX REPLACE ".*\"(.*)\".*" "\\1" PALAN_TARGET "${PALAN_TARGET_LINE}")
set(PALAN_BUILD_LIB_DIR ${CMAKE_BINARY_DIR}/lib/palan/dev)
set(PALAN_INSTALL_LIB_DIR lib/palan/${PALAN_VERSION})

function(palan_tool tool)
	set_target_properties(${tool} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PALAN_BUILD_LIB_DIR}/tools)
	install(TARGETS ${tool} RUNTIME DESTINATION ${PALAN_INSTALL_LIB_DIR}/tools COMPONENT tools)
endfunction()

include(FetchContent)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
	googletest
	DOWNLOAD_EXTRACT_TIMESTAMP ON
	URL https://github.com/google/googletest/archive/refs/tags/release-1.12.0.zip
	)
FetchContent_MakeAvailable(googletest)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(test/test-base)

add_subdirectory(src/gen-ast)
add_subdirectory(test/gen-ast-tester)

add_subdirectory(src/c2ast)
add_subdirectory(test/c2ast-tester)

add_subdirectory(src/semantic-anlyzr)
add_subdirectory(test/sa-tester)
add_subdirectory(test/sa-unit-tester)

add_subdirectory(src/codegen)
add_subdirectory(test/codegen-tester)
add_subdirectory(test/codegen-unit-tester)

add_subdirectory(src/build-mgr)
add_subdirectory(test/build-mgr-tester)

add_subdirectory(src/std)
add_subdirectory(test/std-tester)

set(PALAN_INSTALL_DOC_DIR share/doc/palan/${PALAN_VERSION})
install(FILES README.md LICENSE src/std/LICENSE-Unicode.txt DESTINATION ${PALAN_INSTALL_DOC_DIR} COMPONENT doc)
install(DIRECTORY doc DESTINATION ${PALAN_INSTALL_DOC_DIR} COMPONENT doc PATTERN Issues.md EXCLUDE)

