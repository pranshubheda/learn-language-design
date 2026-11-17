#pragma once
// PHASE 3: SEMANTIC ANALYSIS (name resolution + type checking)
//
// The parser only checks that the program is *shaped* right. This phase
// checks that it *means* something:
//
//   - Does every variable or function name refer to something that exists?
//     (name resolution / scoping)
//   - Do the types line up? You can't write `true + 1`, call `f` with the
//     wrong number of arguments, or use an int as an `if` condition.
//     (type checking)
//   - Does every non-void function return a value on every path?
//     (a tiny bit of control-flow analysis)
//
// It also DECORATES the AST so the code generator has what it needs:
//   - every Expr gets its `type`
//   - every variable gets a `slot` number (its index in the call frame)
//   - every call gets the `funcIndex` of its target
//   - every function gets `numLocals`
//
// Pebble is STATICALLY typed: all of this happens before the program runs.
// That's why the VM (vm.cpp) never checks types at runtime.

#include "ast.h"

// Throws CompileError on the first problem found.
void typecheck(Program& program);
