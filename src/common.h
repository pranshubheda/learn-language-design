#pragma once
// Things every phase of the compiler shares.

#include <stdexcept>
#include <string>

// A position in the source file. Every token and AST node carries one, so
// that any phase (lexer, parser, type checker) can point at the exact spot
// where something went wrong.
struct SourceLoc {
    int line = 1;
    int col = 1;
};

// Thrown by the lexer, parser, and type checker. main.cpp catches it and
// prints a nice "file:line:col: error: ..." message with a caret.
//
// Real compilers usually *collect* many errors and keep going ("error
// recovery"). We stop at the first one to keep the code simple.
struct CompileError : std::runtime_error {
    SourceLoc loc;
    CompileError(SourceLoc loc, const std::string& message)
        : std::runtime_error(message), loc(loc) {}
};
