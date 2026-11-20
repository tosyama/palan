/// Writes a module's declarations, or a library's, as an interface (.pai).
///
/// @file PlnInterface.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once

#include <string>
#include <vector>
#include "../../lib/json/single_include/nlohmann/json.hpp"

// ast: a module's AST, with macro constants folded. Throws std::runtime_error
// on a declaration Palan source cannot state, such as an unimplemented parameter.
std::string writeInterface(const nlohmann::json& ast);

// The interface of the library whose main source is `main`, with `internals` the
// other modules it reaches. Also throws std::runtime_error on a public declaration
// needing a local header or an internal module's type.
std::string writeLibraryInterface(const nlohmann::json& main, const std::vector<nlohmann::json>& internals);

// An AST type as Palan source writes it (`@[]uint8`, `[...]@utf8`).
std::string writeType(const nlohmann::json& astType);
