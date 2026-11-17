#include "typechecker.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace {

// What we know about a function without looking inside its body.
struct FuncSig {
    int index;
    std::vector<Type> params;
    Type returnType;
};

// What we know about a variable that is in scope.
struct LocalVar {
    Type type;
    int slot;
};

class TypeChecker {
public:
    void checkProgram(Program& program) {
        // PASS 1: collect every function's signature BEFORE checking any body.
        // This is what lets a function call another one that is defined
        // later in the file (see examples/mutual.pb). A one-pass compiler
        // like old C compilers can't do this, which is why C needs
        // forward declarations.
        for (size_t i = 0; i < program.funcs.size(); i++) {
            FuncDecl& f = *program.funcs[i];
            if (funcs.count(f.name)) {
                throw CompileError(f.loc, "function '" + f.name + "' is already defined");
            }
            FuncSig sig;
            sig.index = static_cast<int>(i);
            for (Param& p : f.params) {
                p.type = resolveType(p.typeName);
                sig.params.push_back(p.type);
            }
            f.returnType = f.returnTypeName ? resolveType(*f.returnTypeName) : Type::Void;
            sig.returnType = f.returnType;
            funcs[f.name] = sig;
        }

        auto mainIt = funcs.find("main");
        if (mainIt == funcs.end()) {
            throw CompileError(SourceLoc{1, 1}, "program has no 'main' function");
        }
        FuncDecl& mainFn = *program.funcs[mainIt->second.index];
        if (!mainFn.params.empty() || mainFn.returnType != Type::Void) {
            throw CompileError(mainFn.loc, "'main' must take no parameters and return nothing");
        }

        // PASS 2: check each function body.
        for (auto& f : program.funcs) checkFunction(*f);
    }

private:
    std::unordered_map<std::string, FuncSig> funcs;

    // SCOPES: a stack of maps from variable name to variable info. Entering
    // a block pushes a map and leaving it pops one. To look a name up we
    // search from the innermost scope outward, which is exactly how
    // shadowing works.
    std::vector<std::unordered_map<std::string, LocalVar>> scopes;

    int nextSlot = 0;                   // next free local slot in the current function
    FuncDecl* currentFunc = nullptr;    // so `return` knows what type to expect

    // --- Helpers ------------------------------------------------------

    Type resolveType(const TypeName& tn) {
        if (tn.name == "int") return Type::Int;
        if (tn.name == "bool") return Type::Bool;
        if (tn.name == "str") return Type::Str;
        throw CompileError(tn.loc, "unknown type '" + tn.name + "'");
    }

    void pushScope() { scopes.emplace_back(); }
    void popScope() { scopes.pop_back(); }

    // Gives the variable a fresh slot. Slots are never reused, even after a
    // block ends. That wastes a little space but keeps things simple.
    int declare(const std::string& name, Type type, SourceLoc loc) {
        auto& scope = scopes.back();
        if (scope.count(name)) {
            throw CompileError(loc, "'" + name + "' is already declared in this scope");
        }
        int slot = nextSlot++;
        scope[name] = LocalVar{type, slot};
        return slot;
    }

    const LocalVar& lookup(const std::string& name, SourceLoc loc) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) return found->second;
        }
        throw CompileError(loc, "undefined variable '" + name + "'");
    }

    void expectType(Expr& e, Type want, const std::string& what) {
        Type got = checkExpr(e);
        if (got != want) {
            throw CompileError(e.loc, what + " must be " + typeName(want) + ", found " + typeName(got));
        }
    }

    // --- Functions ----------------------------------------------------

    void checkFunction(FuncDecl& f) {
        currentFunc = &f;
        nextSlot = 0;
        scopes.clear();

        // Parameters take the first slots (0, 1, ...). The VM relies on this:
        // the caller pushes the arguments, and they become locals 0..n-1.
        pushScope();
        for (Param& p : f.params) p.slot = declare(p.name, p.type, p.loc);

        bool alwaysReturns = checkBlock(*f.body);
        if (f.returnType != Type::Void && !alwaysReturns) {
            throw CompileError(f.loc, "function '" + f.name + "' might not return a value on every path");
        }

        popScope();
        f.numLocals = nextSlot;
    }

    // --- Statements ---------------------------------------------------
    //
    // Each checkXxx returns true if the statement DEFINITELY returns, meaning
    // that control can never fall off the end of it. That's how we catch:
    //
    //     fn f(x: int) -> int { if x > 0 { return 1; } }   // what if x <= 0?

    bool checkBlock(BlockStmt& block) {
        pushScope();
        bool returns = false;
        for (auto& stmt : block.stmts) {
            if (checkStmt(*stmt)) returns = true;
        }
        popScope();
        return returns;
    }

    bool checkStmt(Stmt& s) {
        switch (s.kind) {
        case StmtKind::Block:
            return checkBlock(static_cast<BlockStmt&>(s));

        case StmtKind::Let: {
            auto& let = static_cast<LetStmt&>(s);
            // Check the initializer BEFORE declaring the name, so that in
            // `let x = x + 1;` the right-hand x means the *outer* x.
            Type initType = checkExpr(*let.init);
            if (initType == Type::Void) {
                throw CompileError(let.init->loc, "cannot store a void value in '" + let.name + "'");
            }
            Type varType = initType;  // TYPE INFERENCE (the simplest possible kind)
            if (let.declaredType) {
                varType = resolveType(*let.declaredType);
                if (varType != initType) {
                    throw CompileError(let.init->loc, "cannot initialize '" + let.name + "' of type " +
                                       typeName(varType) + " with a value of type " + typeName(initType));
                }
            }
            let.slot = declare(let.name, varType, let.loc);
            return false;
        }

        case StmtKind::Assign: {
            auto& assign = static_cast<AssignStmt&>(s);
            const LocalVar& var = lookup(assign.name, assign.loc);
            Type valueType = checkExpr(*assign.value);
            if (valueType != var.type) {
                throw CompileError(assign.value->loc, "cannot assign a value of type " +
                                   std::string(typeName(valueType)) + " to '" + assign.name +
                                   "' of type " + typeName(var.type));
            }
            assign.slot = var.slot;
            return false;
        }

        case StmtKind::If: {
            auto& ifs = static_cast<IfStmt&>(s);
            expectType(*ifs.cond, Type::Bool, "if condition");
            bool thenReturns = checkBlock(*ifs.thenBlock);
            bool elseReturns = ifs.elseBranch ? checkStmt(*ifs.elseBranch) : false;
            return thenReturns && elseReturns;  // must return on BOTH paths
        }

        case StmtKind::While: {
            auto& w = static_cast<WhileStmt&>(s);
            expectType(*w.cond, Type::Bool, "while condition");
            checkBlock(*w.body);
            return false;  // the loop might run zero times, so it doesn't count
        }

        case StmtKind::Return: {
            auto& ret = static_cast<ReturnStmt&>(s);
            Type want = currentFunc->returnType;
            if (!ret.value) {
                if (want != Type::Void) {
                    throw CompileError(ret.loc, "function '" + currentFunc->name + "' must return a value of type " + typeName(want));
                }
                return true;
            }
            if (want == Type::Void) {
                throw CompileError(ret.value->loc, "function '" + currentFunc->name + "' does not return a value");
            }
            Type got = checkExpr(*ret.value);
            if (got != want) {
                throw CompileError(ret.value->loc, "return type mismatch: expected " +
                                   std::string(typeName(want)) + ", found " + typeName(got));
            }
            return true;
        }

        case StmtKind::Print: {
            auto& p = static_cast<PrintStmt&>(s);
            if (checkExpr(*p.value) == Type::Void) {
                throw CompileError(p.value->loc, "cannot print a void value");
            }
            return false;
        }

        case StmtKind::ExprStmt:
            checkExpr(*static_cast<ExprStmt&>(s).expr);
            return false;
        }
        return false;
    }

    // --- Expressions --------------------------------------------------
    //
    // Types are computed BOTTOM-UP: work out the children's types first,
    // then apply the rule for this node. Each `case` below is one TYPING
    // RULE. On paper they're often written like this:
    //
    //     e1 : int    e2 : int
    //     --------------------
    //       e1 + e2 : int

    Type checkExpr(Expr& e) {
        e.type = computeType(e);
        return e.type;
    }

    Type computeType(Expr& e) {
        switch (e.kind) {
        case ExprKind::IntLit:  return Type::Int;
        case ExprKind::BoolLit: return Type::Bool;
        case ExprKind::StrLit:  return Type::Str;

        case ExprKind::Var: {
            auto& v = static_cast<VarExpr&>(e);
            const LocalVar& var = lookup(v.name, v.loc);
            v.slot = var.slot;
            return var.type;
        }

        case ExprKind::Unary: {
            auto& u = static_cast<UnaryExpr&>(e);
            if (u.op == TokenKind::Minus) {
                expectType(*u.operand, Type::Int, "operand of '-'");
                return Type::Int;
            }
            expectType(*u.operand, Type::Bool, "operand of '!'");
            return Type::Bool;
        }

        case ExprKind::Binary: {
            auto& b = static_cast<BinaryExpr&>(e);
            Type l = checkExpr(*b.left);
            Type r = checkExpr(*b.right);
            auto mismatch = [&](const std::string& expected) {
                return CompileError(b.loc, std::string("operator '") + tokenKindName(b.op) + "' expects " +
                                    expected + ", found " + typeName(l) + " and " + typeName(r));
            };
            switch (b.op) {
            case TokenKind::Plus:
            case TokenKind::Minus:
            case TokenKind::Star:
            case TokenKind::Slash:
            case TokenKind::Percent:
                if (l != Type::Int || r != Type::Int) throw mismatch("int operands");
                return Type::Int;
            case TokenKind::Less:
            case TokenKind::LessEq:
            case TokenKind::Greater:
            case TokenKind::GreaterEq:
                if (l != Type::Int || r != Type::Int) throw mismatch("int operands");
                return Type::Bool;
            case TokenKind::EqEq:
            case TokenKind::BangEq:
                // Strings are deliberately not comparable (see the exercises in the README).
                if (l != r || (l != Type::Int && l != Type::Bool)) throw mismatch("two ints or two bools");
                return Type::Bool;
            case TokenKind::AndAnd:
            case TokenKind::OrOr:
                if (l != Type::Bool || r != Type::Bool) throw mismatch("bool operands");
                return Type::Bool;
            default:
                throw CompileError(b.loc, "internal error: unknown binary operator");
            }
        }

        case ExprKind::Call: {
            auto& c = static_cast<CallExpr&>(e);
            auto it = funcs.find(c.callee);
            if (it == funcs.end()) {
                throw CompileError(c.loc, "undefined function '" + c.callee + "'");
            }
            const FuncSig& sig = it->second;
            if (c.args.size() != sig.params.size()) {
                throw CompileError(c.loc, "function '" + c.callee + "' expects " +
                                   std::to_string(sig.params.size()) + " arguments, found " +
                                   std::to_string(c.args.size()));
            }
            for (size_t i = 0; i < c.args.size(); i++) {
                expectType(*c.args[i], sig.params[i], "argument " + std::to_string(i + 1) + " of '" + c.callee + "'");
            }
            c.funcIndex = sig.index;
            return sig.returnType;
        }
        }
        return Type::Unknown;
    }
};

}  // namespace

void typecheck(Program& program) {
    TypeChecker().checkProgram(program);
}
