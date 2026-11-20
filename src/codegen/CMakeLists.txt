cmake_minimum_required(VERSION 3.16)

add_executable(palan-codegen
	main.cpp
	PlnCodegenMessage.cpp
	PlnDeserialize.cpp
	PlnVCodeGen.cpp
	PlnVCodeGenExpr.cpp
	PlnVCodeGenCall.cpp
	PlnRegAlloc.cpp
	PlnX86CodeGen.cpp
	PlnX86Arith.cpp
	PlnX86Call.cpp
	PlnX86Mem.cpp
)
palan_tool(palan-codegen)

option(OUTPUT_COVERAGE "Output coverage files.")

if (CMAKE_COMPILER_IS_GNUCXX)
	if (OUTPUT_COVERAGE)
		message("Coverage: ON")
		target_compile_options(palan-codegen PRIVATE --coverage)
		set_target_properties(palan-codegen PROPERTIES LINK_FLAGS "--coverage")
	endif()
endif()
