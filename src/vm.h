#pragma once
// PHASE 5: EXECUTION, the virtual machine
//
// A loop that fetches the next instruction, decodes it with a switch, and
// executes it. Real CPUs do the same thing in hardware.
//
// MEMORY LAYOUT
// There's one big value stack shared by all function calls. Each call gets
// a FRAME, a window into that stack that starts at `base`:
//
//     stack:  [ ...caller's stuff... | arg0 arg1 | let0 let1 | temporaries... ]
//                                      ^ base
//                                      |-- locals (slots) --|
//
// LOAD/STORE n access stack[base + n]. The arguments the caller pushed
// simply *become* the first locals, so nothing gets copied. RET throws away
// the whole window and pushes the return value back for the caller.

#include "bytecode.h"

#include <ostream>
#include <stdexcept>

// Errors that can only be detected while the program runs.
struct RuntimeError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

void execute(const Module& module, std::ostream& out);
