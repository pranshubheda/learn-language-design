#pragma once
// PHASE 2: PARSING
//
// Input:  the token list from the lexer.
// Output: an AST (see ast.h).
//
// This is a hand-written RECURSIVE DESCENT parser: there is one function
// per grammar rule, and the functions call each other the same way the
// rules refer to each other. Expressions use PRECEDENCE CLIMBING, so one
// small loop handles all the binary operators instead of one function per
// precedence level.
//
// The full grammar is written out at the top of parser.cpp.

#include "ast.h"
#include "lexer.h"

#include <memory>
#include <vector>

// Throws CompileError on a syntax error.
std::unique_ptr<Program> parse(const std::vector<Token>& tokens);
