/// palan-c2ast invocation for cinclude'd C headers.
///
/// @file PlnGenAstC2Ast.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once

#include <string>
#include <utility>
#include <vector>
#include "../../lib/json/single_include/nlohmann/json.hpp"

/// The c-state each cinclude left, by source position (line, column), so a
/// block's end can return to the state at its '{'. Grammar actions run in
/// source order, so entries are appended in position order.
class CStateHistory {
public:
	using Pos = std::pair<int, int>;
	const std::string& current() const;
	void push(Pos at, std::string state);
	/// Return to the state before `open`, from `close` on. False when no
	/// cinclude lies between them.
	bool closeScope(Pos open, Pos close);
private:
	std::vector<std::pair<Pos, std::string>> entries_;
};

/// Run palan-c2ast on one cinclude'd header and return its AST JSON.
///
/// path is raw token text with only its delimiters stripped (PlnLexer.ll's
/// PATH/INCLUDE_FILE patterns), so it must never be handed to a shell.
///
/// c_state is the "c-state" output of the cinclude before it in scope, so this header is
/// preprocessed and parsed with the macros and types in effect at its
/// cinclude, as in one C translation unit; empty for the first cinclude.
///
/// Throws std::runtime_error (E_C2AstFailed) if c2ast cannot be started,
/// exits non-zero, or emits output that is not valid JSON.
nlohmann::json execute_c2ast(const std::string& path_type,
                             const std::string& path,
                             const std::string& base_dir,
                             const std::string& c_state,
                             const std::string& target);
