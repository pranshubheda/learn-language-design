#pragma once
// THE BYTECODE: the instruction set of our virtual machine.
//
// This is the contract between the code generator (which writes these
// instructions) and the VM (which executes them).
//
// It's a STACK MACHINE: instructions take their inputs from the top of a
// value stack and push their results back onto it. `1 + 2 * 3` becomes:
//
//     PUSH 1        stack: [1]
//     PUSH 2        stack: [1, 2]
//     PUSH 3        stack: [1, 2, 3]
//     MUL           stack: [1, 6]
//     ADD           stack: [7]
//
// Notice this is just a POST-ORDER walk of the AST (children first, then
// the operator). That's why stack machines are so easy to generate code for.
// The JVM, Python's VM, and WebAssembly all work this way.
//
// Note that the VM has NO TYPES. Every value is an int64_t:
//   int  -> itself
//   bool -> 0 or 1
//   str  -> an index into Module::strings
// The type checker already proved every operation is valid, so the types
// were "erased" after checking. The only place type information survives is
// in the choice of instruction: print(x) compiles to PRINT_INT, PRINT_BOOL,
// or PRINT_STR depending on x's static type.
//
// (A real VM packs instructions into raw bytes, which is where the name
// "byte"code comes from. We use a struct per instruction because it's
// easier to read.)

#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

enum class Op {
    Push,         // arg = value          push arg
    Pop,          //                      discard the top value
    Load,         // arg = slot           push locals[slot]
    Store,        // arg = slot           locals[slot] = pop()

    Add, Sub, Mul, Div, Mod,   // b = pop(), a = pop(), push(a OP b)
    Neg,                       // push(-pop())
    Not,                       // push(!pop())
    Eq, Ne, Lt, Le, Gt, Ge,    // b = pop(), a = pop(), push(a OP b ? 1 : 0)

    Jump,         // arg = target         ip = target
    JumpIfFalse,  // arg = target         if pop() == 0 then ip = target

    Call,         // arg = function index call it; its arguments are already on the stack
    Ret,          //                      return pop() to the caller

    PrintInt,     //                      print pop() as a number
    PrintBool,    //                      print pop() as true/false
    PrintStr,     //                      print strings[pop()]
};

struct Instr {
    Op op;
    int64_t arg = 0;  // only used by ops that take an argument
};

struct Function {
    std::string name;
    int numParams = 0;
    int numLocals = 0;  // params + let-variables
    std::vector<Instr> code;
};

// The output of the compiler: everything the VM needs to run the program.
struct Module {
    std::vector<Function> functions;
    std::vector<std::string> strings;  // the string constant pool
    int mainIndex = -1;
};

const char* opName(Op op);
bool opHasArg(Op op);

// Human-readable listing (used by `pebble --bytecode`).
void disassemble(const Module& module, std::ostream& out);
