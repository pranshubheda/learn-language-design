#include "codegen.h"

#include <unordered_map>

namespace {

class CodeGen {
public:
    Module run(const Program& program) {
        // Create all the Function shells first, so that every function index
        // is valid even before its body is generated.
        for (auto& f : program.funcs) {
            Function fn;
            fn.name = f->name;
            fn.numParams = static_cast<int>(f->params.size());
            fn.numLocals = f->numLocals;
            module.functions.push_back(fn);
            if (f->name == "main") module.mainIndex = static_cast<int>(module.functions.size() - 1);
        }

        for (size_t i = 0; i < program.funcs.size(); i++) {
            current = &module.functions[i];
            genBlock(*program.funcs[i]->body);
            // Implicit "return;" at the end, for when a void function falls
            // off the end. (Non-void functions can't get here, because the
            // type checker proved they always return first.)
            emit(Op::Push, 0);
            emit(Op::Ret);
        }
        return std::move(module);
    }

private:
    Module module;
    Function* current = nullptr;  // the function we're currently emitting into
    std::unordered_map<std::string, int> stringIndex;

    // --- Emission helpers ---------------------------------------------

    // Appends an instruction and returns its address (its index in the code).
    int emit(Op op, int64_t arg = 0) {
        current->code.push_back(Instr{op, arg});
        return static_cast<int>(current->code.size()) - 1;
    }

    // The address the NEXT instruction will get.
    int here() const { return static_cast<int>(current->code.size()); }

    // BACKPATCHING: when we emit a forward jump, we don't know its target yet
    // (that code hasn't been generated). So we emit it with a placeholder,
    // remember its address, and fill in the real target once we get there.
    void patchJumpToHere(int jumpAddr) { current->code[jumpAddr].arg = here(); }

    // Each distinct string literal is stored once in the module's pool.
    int internString(const std::string& s) {
        auto it = stringIndex.find(s);
        if (it != stringIndex.end()) return it->second;
        int idx = static_cast<int>(module.strings.size());
        module.strings.push_back(s);
        stringIndex[s] = idx;
        return idx;
    }

    // --- Statements ---------------------------------------------------
    //
    // INVARIANT: a statement leaves the stack exactly as it found it.
    // An expression leaves exactly ONE extra value on top.

    void genBlock(const BlockStmt& block) {
        for (auto& stmt : block.stmts) genStmt(*stmt);
    }

    void genStmt(const Stmt& s) {
        switch (s.kind) {
        case StmtKind::Block:
            genBlock(static_cast<const BlockStmt&>(s));
            break;

        case StmtKind::Let: {
            auto& let = static_cast<const LetStmt&>(s);
            genExpr(*let.init);
            emit(Op::Store, let.slot);
            break;
        }

        case StmtKind::Assign: {
            auto& a = static_cast<const AssignStmt&>(s);
            genExpr(*a.value);
            emit(Op::Store, a.slot);
            break;
        }

        case StmtKind::If: {
            //       <cond>
            //       JUMP_IF_FALSE else
            //       <then block>
            //       JUMP end            (only if there is an else)
            // else: <else branch>
            // end:
            auto& ifs = static_cast<const IfStmt&>(s);
            genExpr(*ifs.cond);
            int jumpToElse = emit(Op::JumpIfFalse);
            genBlock(*ifs.thenBlock);
            if (ifs.elseBranch) {
                int jumpToEnd = emit(Op::Jump);
                patchJumpToHere(jumpToElse);
                genStmt(*ifs.elseBranch);
                patchJumpToHere(jumpToEnd);
            } else {
                patchJumpToHere(jumpToElse);
            }
            break;
        }

        case StmtKind::While: {
            // top:  <cond>
            //       JUMP_IF_FALSE end
            //       <body>
            //       JUMP top            (a BACKWARD jump, whose target we already know)
            // end:
            auto& w = static_cast<const WhileStmt&>(s);
            int top = here();
            genExpr(*w.cond);
            int jumpToEnd = emit(Op::JumpIfFalse);
            genBlock(*w.body);
            emit(Op::Jump, top);
            patchJumpToHere(jumpToEnd);
            break;
        }

        case StmtKind::Return: {
            auto& r = static_cast<const ReturnStmt&>(s);
            // Every function returns exactly one value, and void ones return
            // a dummy 0. This uniformity keeps CALL/RET simple.
            if (r.value) genExpr(*r.value);
            else emit(Op::Push, 0);
            emit(Op::Ret);
            break;
        }

        case StmtKind::Print: {
            auto& p = static_cast<const PrintStmt&>(s);
            genExpr(*p.value);
            // The static type picks the instruction, because the VM can't tell
            // an int from a bool from a string index.
            switch (p.value->type) {
            case Type::Int:  emit(Op::PrintInt); break;
            case Type::Bool: emit(Op::PrintBool); break;
            case Type::Str:  emit(Op::PrintStr); break;
            default: break;  // unreachable: the type checker rejects these
            }
            break;
        }

        case StmtKind::ExprStmt:
            genExpr(*static_cast<const ExprStmt&>(s).expr);
            emit(Op::Pop);  // throw away the unused result to keep the stack balanced
            break;
        }
    }

    // --- Expressions --------------------------------------------------

    static Op binaryOp(TokenKind kind) {
        switch (kind) {
        case TokenKind::Plus:      return Op::Add;
        case TokenKind::Minus:     return Op::Sub;
        case TokenKind::Star:      return Op::Mul;
        case TokenKind::Slash:     return Op::Div;
        case TokenKind::Percent:   return Op::Mod;
        case TokenKind::EqEq:      return Op::Eq;
        case TokenKind::BangEq:    return Op::Ne;
        case TokenKind::Less:      return Op::Lt;
        case TokenKind::LessEq:    return Op::Le;
        case TokenKind::Greater:   return Op::Gt;
        case TokenKind::GreaterEq: return Op::Ge;
        default:                   return Op::Add;  // unreachable
        }
    }

    void genExpr(const Expr& e) {
        switch (e.kind) {
        case ExprKind::IntLit:
            emit(Op::Push, static_cast<const IntLitExpr&>(e).value);
            break;
        case ExprKind::BoolLit:
            emit(Op::Push, static_cast<const BoolLitExpr&>(e).value ? 1 : 0);
            break;
        case ExprKind::StrLit:
            emit(Op::Push, internString(static_cast<const StrLitExpr&>(e).value));
            break;
        case ExprKind::Var:
            emit(Op::Load, static_cast<const VarExpr&>(e).slot);
            break;

        case ExprKind::Unary: {
            auto& u = static_cast<const UnaryExpr&>(e);
            genExpr(*u.operand);
            emit(u.op == TokenKind::Minus ? Op::Neg : Op::Not);
            break;
        }

        case ExprKind::Binary: {
            auto& b = static_cast<const BinaryExpr&>(e);

            // SHORT-CIRCUIT evaluation: in `a && b`, if a is false then b is
            // never evaluated. So && and || are really control flow, not
            // arithmetic, and they compile to jumps.
            if (b.op == TokenKind::AndAnd) {
                //        <a>
                //        JUMP_IF_FALSE false
                //        <b>                    result is b
                //        JUMP end
                // false: PUSH 0                 result is false
                // end:
                genExpr(*b.left);
                int toFalse = emit(Op::JumpIfFalse);
                genExpr(*b.right);
                int toEnd = emit(Op::Jump);
                patchJumpToHere(toFalse);
                emit(Op::Push, 0);
                patchJumpToHere(toEnd);
                break;
            }
            if (b.op == TokenKind::OrOr) {
                //        <a>
                //        JUMP_IF_FALSE right
                //        PUSH 1                 a was true, so the result is true
                //        JUMP end
                // right: <b>                    result is b
                // end:
                genExpr(*b.left);
                int toRight = emit(Op::JumpIfFalse);
                emit(Op::Push, 1);
                int toEnd = emit(Op::Jump);
                patchJumpToHere(toRight);
                genExpr(*b.right);
                patchJumpToHere(toEnd);
                break;
            }

            // Ordinary operators: evaluate both sides, then combine (post-order).
            genExpr(*b.left);
            genExpr(*b.right);
            emit(binaryOp(b.op));
            break;
        }

        case ExprKind::Call: {
            auto& c = static_cast<const CallExpr&>(e);
            // Push the arguments left to right. The callee will find them as
            // its locals 0..n-1 (see VM::callFunction).
            for (auto& arg : c.args) genExpr(*arg);
            emit(Op::Call, c.funcIndex);
            break;
        }
        }
    }
};

}  // namespace

Module generate(const Program& program) {
    return CodeGen().run(program);
}
