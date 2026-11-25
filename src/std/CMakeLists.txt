cmake_minimum_required(VERSION 3.16)

# The public mains; any other source is an internal module.
set(PALAN_STD_MAINS utf8 rt)

file(GLOB_RECURSE PALAN_STD_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.pa)
set(std_dir ${PALAN_BUILD_LIB_DIR}/${PALAN_TARGET})

set(std_outputs)
set(std_commands)
foreach(main ${PALAN_STD_MAINS})
	list(APPEND std_outputs ${std_dir}/std/${main}.pai ${std_dir}/std/${main}.a)
	# The work files stay in the build tree: a build must not write to the user's home, which a
	# package build may not even have.
	list(APPEND std_commands COMMAND ${CMAKE_COMMAND} -E env PALAN_WORK=${CMAKE_BINARY_DIR}/std-work
		$<TARGET_FILE:palan> --lib -o ${std_dir} ${CMAKE_CURRENT_SOURCE_DIR}/${main}.pa)
endforeach()

# Every main again on any change, since --lib does not detect an older copy of a module two mains share.
add_custom_command(
	OUTPUT ${std_outputs}
	${std_commands}
	DEPENDS ${PALAN_STD_SOURCES} palan palan-gen-ast palan-c2ast palan-sa palan-codegen
		${std_dir}/c2ast/predefined.h
	COMMENT "Building std"
	VERBATIM
)
add_custom_target(palan-std ALL DEPENDS ${std_outputs})

install(FILES ${std_outputs} DESTINATION ${PALAN_INSTALL_LIB_DIR}/${PALAN_TARGET}/std COMPONENT tools)
install(DIRECTORY ./ DESTINATION ${PALAN_INSTALL_LIB_DIR}/src/std COMPONENT tools
	FILES_MATCHING PATTERN "*.pa" PATTERN "LICENSE-Unicode.txt")
