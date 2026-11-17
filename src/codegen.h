#pragma once
// PHASE 4: CODE GENERATION
//
// Input:  the type-checked, decorated AST.
// Output: a Module of bytecode (see bytecode.h).
//
// Like the type checker, it's a recursive walk over the tree. Each kind of
// node has a fixed "template" of instructions, and the interesting parts
// are how control flow (if, while, &&, ||) turns into jumps.
//
// The code generator assumes the program is valid. Every error was already
// reported by the type checker, so this phase never fails.

#include "ast.h"
#include "bytecode.h"

Module generate(const Program& program);
