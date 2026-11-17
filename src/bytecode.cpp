#include "bytecode.h"

#include <iomanip>

const char* opName(Op op) {
    switch (op) {
    case Op::Push:        return "PUSH";
    case Op::Pop:         return "POP";
    case Op::Load:        return "LOAD";
    case Op::Store:       return "STORE";
    case Op::Add:         return "ADD";
    case Op::Sub:         return "SUB";
    case Op::Mul:         return "MUL";
    case Op::Div:         return "DIV";
    case Op::Mod:         return "MOD";
    case Op::Neg:         return "NEG";
    case Op::Not:         return "NOT";
    case Op::Eq:          return "EQ";
    case Op::Ne:          return "NE";
    case Op::Lt:          return "LT";
    case Op::Le:          return "LE";
    case Op::Gt:          return "GT";
    case Op::Ge:          return "GE";
    case Op::Jump:        return "JUMP";
    case Op::JumpIfFalse: return "JUMP_IF_FALSE";
    case Op::Call:        return "CALL";
    case Op::Ret:         return "RET";
    case Op::PrintInt:    return "PRINT_INT";
    case Op::PrintBool:   return "PRINT_BOOL";
    case Op::PrintStr:    return "PRINT_STR";
    }
    return "???";
}

bool opHasArg(Op op) {
    switch (op) {
    case Op::Push:
    case Op::Load:
    case Op::Store:
    case Op::Jump:
    case Op::JumpIfFalse:
    case Op::Call:
        return true;
    default:
        return false;
    }
}

void disassemble(const Module& module, std::ostream& out) {
    for (size_t f = 0; f < module.functions.size(); f++) {
        const Function& fn = module.functions[f];
        out << "fn #" << f << " " << fn.name << "  (params: " << fn.numParams
            << ", locals: " << fn.numLocals << ")\n";
        for (size_t i = 0; i < fn.code.size(); i++) {
            const Instr& in = fn.code[i];
            out << "  " << std::setw(4) << i << "  " << std::left << std::setw(14) << opName(in.op) << std::right;
            if (opHasArg(in.op)) out << in.arg;
            if (in.op == Op::Call) out << "    ; " << module.functions[in.arg].name;
            if (in.op == Op::Load || in.op == Op::Store) out << "    ; slot";
            out << "\n";
        }
        out << "\n";
    }
    if (!module.strings.empty()) {
        out << "strings:\n";
        for (size_t i = 0; i < module.strings.size(); i++) {
            out << "  #" << i << "  \"" << module.strings[i] << "\"\n";
        }
    }
}
