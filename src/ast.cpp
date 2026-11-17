// AST pretty-printer. It isn't part of compiling, but it's very useful for
// *seeing* what the parser built and what the type checker added.
// Try: ./pebble --ast examples/fib.pb

#include "ast.h"

#include <string>

const char* typeName(Type t) {
    switch (t) {
    case Type::Unknown: return "?";
    case Type::Int:     return "int";
    case Type::Bool:    return "bool";
    case Type::Str:     return "str";
    case Type::Void:    return "void";
    }
    return "?";
}

namespace {

void line(std::ostream& out, int indent, const std::string& text) {
    out << std::string(indent * 2, ' ') << text << "\n";
}

// Appends ": <type>" once the type checker has run.
std::string withType(const std::string& text, Type t) {
    return t == Type::Unknown ? text : text + "  : " + typeName(t);
}

std::string quote(const std::string& s) {
    std::string r = "\"";
    for (char c : s) {
        if (c == '\n') r += "\\n";
        else if (c == '\t') r += "\\t";
        else if (c == '"') r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else r += c;
    }
    return r + "\"";
}

std::string slotNote(int slot) {
    return slot >= 0 ? " (slot " + std::to_string(slot) + ")" : "";
}

void printExpr(const Expr& e, std::ostream& out, int ind) {
    switch (e.kind) {
    case ExprKind::IntLit: {
        auto& n = static_cast<const IntLitExpr&>(e);
        line(out, ind, withType("Int " + std::to_string(n.value), e.type));
        break;
    }
    case ExprKind::BoolLit: {
        auto& n = static_cast<const BoolLitExpr&>(e);
        line(out, ind, withType(std::string("Bool ") + (n.value ? "true" : "false"), e.type));
        break;
    }
    case ExprKind::StrLit: {
        auto& n = static_cast<const StrLitExpr&>(e);
        line(out, ind, withType("Str " + quote(n.value), e.type));
        break;
    }
    case ExprKind::Var: {
        auto& n = static_cast<const VarExpr&>(e);
        line(out, ind, withType("Var " + n.name + slotNote(n.slot), e.type));
        break;
    }
    case ExprKind::Unary: {
        auto& n = static_cast<const UnaryExpr&>(e);
        line(out, ind, withType(std::string("Unary ") + tokenKindName(n.op), e.type));
        printExpr(*n.operand, out, ind + 1);
        break;
    }
    case ExprKind::Binary: {
        auto& n = static_cast<const BinaryExpr&>(e);
        line(out, ind, withType(std::string("Binary ") + tokenKindName(n.op), e.type));
        printExpr(*n.left, out, ind + 1);
        printExpr(*n.right, out, ind + 1);
        break;
    }
    case ExprKind::Call: {
        auto& n = static_cast<const CallExpr&>(e);
        std::string note = n.funcIndex >= 0 ? " (fn #" + std::to_string(n.funcIndex) + ")" : "";
        line(out, ind, withType("Call " + n.callee + note, e.type));
        for (auto& arg : n.args) printExpr(*arg, out, ind + 1);
        break;
    }
    }
}

void printStmt(const Stmt& s, std::ostream& out, int ind) {
    switch (s.kind) {
    case StmtKind::Block: {
        auto& n = static_cast<const BlockStmt&>(s);
        line(out, ind, "Block");
        for (auto& st : n.stmts) printStmt(*st, out, ind + 1);
        break;
    }
    case StmtKind::Let: {
        auto& n = static_cast<const LetStmt&>(s);
        std::string declared = n.declaredType ? ": " + n.declaredType->name : "";
        line(out, ind, "Let " + n.name + declared + slotNote(n.slot));
        printExpr(*n.init, out, ind + 1);
        break;
    }
    case StmtKind::Assign: {
        auto& n = static_cast<const AssignStmt&>(s);
        line(out, ind, "Assign " + n.name + slotNote(n.slot));
        printExpr(*n.value, out, ind + 1);
        break;
    }
    case StmtKind::If: {
        auto& n = static_cast<const IfStmt&>(s);
        line(out, ind, "If");
        printExpr(*n.cond, out, ind + 1);
        printStmt(*n.thenBlock, out, ind + 1);
        if (n.elseBranch) {
            line(out, ind, "Else");
            printStmt(*n.elseBranch, out, ind + 1);
        }
        break;
    }
    case StmtKind::While: {
        auto& n = static_cast<const WhileStmt&>(s);
        line(out, ind, "While");
        printExpr(*n.cond, out, ind + 1);
        printStmt(*n.body, out, ind + 1);
        break;
    }
    case StmtKind::Return: {
        auto& n = static_cast<const ReturnStmt&>(s);
        line(out, ind, "Return");
        if (n.value) printExpr(*n.value, out, ind + 1);
        break;
    }
    case StmtKind::Print: {
        auto& n = static_cast<const PrintStmt&>(s);
        line(out, ind, "Print");
        printExpr(*n.value, out, ind + 1);
        break;
    }
    case StmtKind::ExprStmt: {
        auto& n = static_cast<const ExprStmt&>(s);
        line(out, ind, "ExprStmt");
        printExpr(*n.expr, out, ind + 1);
        break;
    }
    }
}

}  // namespace

void printAst(const Program& program, std::ostream& out) {
    out << "Program\n";
    for (auto& f : program.funcs) {
        std::string header = "Fn " + f->name + "(";
        for (size_t i = 0; i < f->params.size(); i++) {
            if (i > 0) header += ", ";
            header += f->params[i].name + ": " + f->params[i].typeName.name + slotNote(f->params[i].slot);
        }
        header += ")";
        if (f->returnType != Type::Unknown) header += " -> " + std::string(typeName(f->returnType));
        header += "  [locals: " + std::to_string(f->numLocals) + "]";
        line(out, 1, header);
        printStmt(*f->body, out, 2);
    }
}
