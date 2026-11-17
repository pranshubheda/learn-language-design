#pragma once
// PHASE 1: LEXING (a.k.a. scanning / tokenizing)
//
// Input:  the raw source text, one long string of characters.
// Output: a flat list of tokens, the "words" of the language.
//
//   source:  let x = 42;
//   tokens:  [Let] [Ident "x"] [Assign] [Int 42] [Semicolon] [Eof]
//
// The lexer knows nothing about grammar. It doesn't know that `let` must be
// followed by a name. It only groups characters into meaningful chunks and
// throws away whitespace and comments.

#include "common.h"
#include <cstdint>
#include <string>
#include <vector>

enum class TokenKind {
    // Literals and names
    Int, String, Ident,

    // Keywords: reserved words that look like identifiers but aren't
    Fn, Let, If, Else, While, Return, Print, True, False,

    // Punctuation
    LParen, RParen, LBrace, RBrace, Comma, Colon, Semicolon, Arrow,

    // Operators
    Plus, Minus, Star, Slash, Percent,
    Bang, Assign, EqEq, BangEq, Less, LessEq, Greater, GreaterEq,
    AndAnd, OrOr,

    // Marks the end of input, so the parser never runs off the end of the list
    Eof,
};

struct Token {
    TokenKind kind;
    std::string text;         // exact characters from the source: fib, 42, "hi"
    SourceLoc loc;            // where the token starts
    int64_t intValue = 0;     // only meaningful for Int tokens
    std::string stringValue;  // only for String tokens (quotes removed, escapes processed)
};

// Human-readable name, e.g. "+" or "identifier". Used in error messages.
const char* tokenKindName(TokenKind kind);

// Throws CompileError on bad input (e.g. an unterminated string).
std::vector<Token> lex(const std::string& source);
