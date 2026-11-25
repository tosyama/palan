/// Palan common definitions.
///
/// @file PlnDefs.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once

#include <string>

#define PALAN_VERSION "0.2.7"
#define PALAN_TARGET "x86_64-linux-gnu"

inline bool isSupportedTarget(const std::string& target)
{
	return target == PALAN_TARGET;
}

// Stem of the module build-mgr generates the __pln_* allocators into. palan-sa
// names that module in the symbols of the calls it makes to them.
#define PALAN_ALLOCATORS_MODULE "__allocators"

// A `.` appears in a module's identity only through a `library` declaration.
inline bool isLibraryModule(const std::string& moduleId)
{
	return moduleId.find('.') != std::string::npos;
}

// The module holding the allocators `moduleId` calls.
inline std::string allocatorsModuleOf(const std::string& moduleId)
{
	return isLibraryModule(moduleId) ? moduleId + "." PALAN_ALLOCATORS_MODULE : PALAN_ALLOCATORS_MODULE;
}

// build-mgr claims these identities, so no source module has one.
inline bool isAllocatorsModule(const std::string& moduleId)
{
	static const std::string suffix = "." PALAN_ALLOCATORS_MODULE; // LCOV_EXCL_BR_LINE
	return moduleId == PALAN_ALLOCATORS_MODULE
		|| (moduleId.size() > suffix.size() && moduleId.compare(moduleId.size() - suffix.size(), suffix.size(), suffix) == 0);
}
