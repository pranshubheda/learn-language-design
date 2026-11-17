#pragma once
// THE AST (Abstract Syntax Tree)
//
// The parser turns the flat token list into this tree. "Abstract" means
// that syntax noise like parentheses, semicolons, and braces is gone. Only
// the *structure* remains:
//
//   source:   print(1 + 2 * 3);
//
//   tree:     Print
//               Binary +
//                 Int 1
//                 Binary *        <- precedence is now encoded by nesting
//                   Int 2
//                   Int 3
//
// Every later phase (type checker, code generator) is a recursive walk over
// this tree.
//
// HOW THE NODES ARE REPRESENTED
// Each node is a C++ struct that inherits from Expr or Stmt and has a `kind`
// tag. Code that walks the tree does `switch (node.kind)` and then
// `static_cast`s to the concrete type. Clang does the same thing (it calls
// it `dyn_cast`). The alternative is the "visitor pattern", which is more
// indirect and harder to read.
//
// DECORATION
// Some fields start empty and are filled in by the type checker:
//   Expr::type, VarExpr::slot, CallExpr::funcIndex, FuncDecl::numLocals ...
// After type checking, the code generator reads them. This "decorated AST"
// is how information flows from one phase to the next.

#include "common.h"
#include "lexer.h"

#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

// The types that exist in Pebble. Unknown means "not checked yet".
enum class Type { Unknown, Int, Bool, Str, Void };

const char* typeName(Type t);

// A type as *written in the source*, e.g. the `int` in `x: int`.
// It's just a name until the type checker resolves it into a Type.
struct TypeName {
    std::string name;
    SourceLoc loc;
};

// ---------------------------------------------------------------------------
// Expressions: things that produce a value
// ---------------------------------------------------------------------------

enum class ExprKind { IntLit, BoolLit, StrLit, Var, Unary, Binary, Call };

struct Expr {
    ExprKind kind;
    SourceLoc loc;
    Type type = Type::Unknown;  // filled in by the type checker

    Expr(ExprKind kind, SourceLoc loc) : kind(kind), loc(loc) {}
    virtual ~Expr() = default;
};
using ExprPtr = std::unique_ptr<Expr>;

struct IntLitExpr : Expr {
    int64_t value;
    IntLitExpr(SourceLoc loc, int64_t value) : Expr(ExprKind::IntLit, loc), value(value) {}
};

struct BoolLitExpr : Expr {
    bool value;
    BoolLitExpr(SourceLoc loc, bool value) : Expr(ExprKind::BoolLit, loc), value(value) {}
};

struct StrLitExpr : Expr {
    std::string value;
    StrLitExpr(SourceLoc loc, std::string value) : Expr(ExprKind::StrLit, loc), value(std::move(value)) {}
};

// A use of a variable, like the `x` in `x + 1`.
struct VarExpr : Expr {
    std::string name;
    int slot = -1;  // filled in by the type checker: which local slot holds this variable
    VarExpr(SourceLoc loc, std::string name) : Expr(ExprKind::Var, loc), name(std::move(name)) {}
};

// -x  or  !b
struct UnaryExpr : Expr {
    TokenKind op;  // Minus or Bang
    ExprPtr operand;
    UnaryExpr(SourceLoc loc, TokenKind op, ExprPtr operand)
        : Expr(ExprKind::Unary, loc), op(op), operand(std::move(operand)) {}
};

// a + b, a < b, a && b, ...
struct BinaryExpr : Expr {
    TokenKind op;
    ExprPtr left, right;
    BinaryExpr(SourceLoc loc, TokenKind op, ExprPtr left, ExprPtr right)
        : Expr(ExprKind::Binary, loc), op(op), left(std::move(left)), right(std::move(right)) {}
};

// f(a, b)
struct CallExpr : Expr {
    std::string callee;
    std::vector<ExprPtr> args;
    int funcIndex = -1;  // filled in by the type checker
    CallExpr(SourceLoc loc, std::string callee, std::vector<ExprPtr> args)
        : Expr(ExprKind::Call, loc), callee(std::move(callee)), args(std::move(args)) {}
};

// ---------------------------------------------------------------------------
// Statements: things that *do* something and produce no value
// ---------------------------------------------------------------------------

enum class StmtKind { Let, Assign, If, While, Return, Print, ExprStmt, Block };

struct Stmt {
    StmtKind kind;
    SourceLoc loc;
    Stmt(StmtKind kind, SourceLoc loc) : kind(kind), loc(loc) {}
    virtual ~Stmt() = default;
};
using StmtPtr = std::unique_ptr<Stmt>;

// { stmt; stmt; ... }  A block also introduces a new scope.
struct BlockStmt : Stmt {
    std::vector<StmtPtr> stmts;
    explicit BlockStmt(SourceLoc loc) : Stmt(StmtKind::Block, loc) {}
};

// let x: int = 5;   (the ": int" part is optional, and the type is inferred if missing)
struct LetStmt : Stmt {
    std::string name;
    std::optional<TypeName> declaredType;
    ExprPtr init;
    int slot = -1;  // filled in by the type checker
    LetStmt(SourceLoc loc, std::string name, std::optional<TypeName> declaredType, ExprPtr init)
        : Stmt(StmtKind::Let, loc), name(std::move(name)),
          declaredType(std::move(declaredType)), init(std::move(init)) {}
};

// x = expr;
struct AssignStmt : Stmt {
    std::string name;
    ExprPtr value;
    int slot = -1;  // filled in by the type checker
    AssignStmt(SourceLoc loc, std::string name, ExprPtr value)
        : Stmt(StmtKind::Assign, loc), name(std::move(name)), value(std::move(value)) {}
};

// if cond { ... } else { ... }
// `elseBranch` is null, a BlockStmt, or another IfStmt (for "else if").
struct IfStmt : Stmt {
    ExprPtr cond;
    std::unique_ptr<BlockStmt> thenBlock;
    StmtPtr elseBranch;
    IfStmt(SourceLoc loc, ExprPtr cond, std::unique_ptr<BlockStmt> thenBlock, StmtPtr elseBranch)
        : Stmt(StmtKind::If, loc), cond(std::move(cond)),
          thenBlock(std::move(thenBlock)), elseBranch(std::move(elseBranch)) {}
};

struct WhileStmt : Stmt {
    ExprPtr cond;
    std::unique_ptr<BlockStmt> body;
    WhileStmt(SourceLoc loc, ExprPtr cond, std::unique_ptr<BlockStmt> body)
        : Stmt(StmtKind::While, loc), cond(std::move(cond)), body(std::move(body)) {}
};

// return;  or  return expr;   (value may be null)
struct ReturnStmt : Stmt {
    ExprPtr value;
    ReturnStmt(SourceLoc loc, ExprPtr value) : Stmt(StmtKind::Return, loc), value(std::move(value)) {}
};

// print(expr);   print is built into the language, not a library function.
struct PrintStmt : Stmt {
    ExprPtr value;
    PrintStmt(SourceLoc loc, ExprPtr value) : Stmt(StmtKind::Print, loc), value(std::move(value)) {}
};

// An expression used as a statement, usually a call like `doThing();`
struct ExprStmt : Stmt {
    ExprPtr expr;
    ExprStmt(SourceLoc loc, ExprPtr expr) : Stmt(StmtKind::ExprStmt, loc), expr(std::move(expr)) {}
};

// ---------------------------------------------------------------------------
// Top level
// ---------------------------------------------------------------------------

struct Param {
    std::string name;
    TypeName typeName;
    SourceLoc loc;
    Type type = Type::Unknown;  // filled in by the type checker
    int slot = -1;              // filled in by the type checker
};

struct FuncDecl {
    std::string name;
    SourceLoc loc;
    std::vector<Param> params;
    std::optional<TypeName> returnTypeName;  // missing means "returns nothing" (void)
    std::unique_ptr<BlockStmt> body;

    Type returnType = Type::Unknown;  // filled in by the type checker
    int numLocals = 0;                // filled in by the type checker (params + lets)
};

struct Program {
    std::vector<std::unique_ptr<FuncDecl>> funcs;
};

// Pretty-prints the tree (used by `pebble --ast`).
void printAst(const Program& program, std::ostream& out);
