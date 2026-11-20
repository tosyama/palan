cmake_minimum_required(VERSION 3.16)

add_executable(palan-sa
	main.cpp
	PlnSemanticAnalyzer.cpp
	PlnSaExpr.cpp
	PlnSaCall.cpp
	PlnSaExpand.cpp
	PlnSaDecl.cpp
	PlnSaStorage.cpp
	PlnSaStmt.cpp
	PlnSaMessage.cpp
	PlnType.cpp
	../common/PlnInterface.cpp
)
palan_tool(palan-sa)

option(OUTPUT_COVERAGE "Output coverage files.")

if (CMAKE_COMPILER_IS_GNUCXX)
	if (OUTPUT_COVERAGE)
		message("Coverage: ON")
		target_compile_options(palan-sa PRIVATE --coverage)
		set_target_properties(palan-sa PROPERTIES LINK_FLAGS "--coverage")
	endif()
endif()

