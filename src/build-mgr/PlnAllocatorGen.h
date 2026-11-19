/// Generates the allocators' Palan source from the shapes palan-sa records.
///
/// @file PlnAllocatorGen.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once

#include <ostream>
#include <set>
#include <string>
#include <vector>
#include "../../lib/json/single_include/nlohmann/json.hpp"

// The shapes of one set of allocators, each once, in the order palan-sa met
// them: an element's helpers come before those of the arrays holding it.
struct PlnAllocShapes {
	std::set<std::string> seen;
	std::vector<nlohmann::json> structs;
	std::vector<nlohmann::json> arrays;
	void add(const nlohmann::json& sa);
	bool empty() const { return structs.empty() && arrays.empty(); }
};

void writeAllocatorsHeader(std::ostream& hdr, const PlnAllocShapes& shapes);
// library: the module's identity the allocators belong to, or empty for a program's.
// checks: whether the run-time checks of a debug build are written.
void writeAllocatorsSource(std::ostream& out, const PlnAllocShapes& shapes, const std::string& library, bool checks);
