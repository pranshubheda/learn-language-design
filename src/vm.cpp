#include "vm.h"

#include <cstdint>
#include <limits>
#include <vector>

namespace {

// Infinite recursion should report an error, not crash the VM.
constexpr size_t kMaxFrames = 10000;

struct Frame {
    const Function* fn;
    size_t ip;    // instruction pointer: index of the next instruction in fn->code
    size_t base;  // where this frame's locals start on the value stack
};

// Signed overflow is undefined behavior in C++. Doing the math on unsigned
// values gives well-defined wraparound instead, like most real hardware.
int64_t wrapAdd(int64_t a, int64_t b) { return static_cast<int64_t>(static_cast<uint64_t>(a) + static_cast<uint64_t>(b)); }
int64_t wrapSub(int64_t a, int64_t b) { return static_cast<int64_t>(static_cast<uint64_t>(a) - static_cast<uint64_t>(b)); }
int64_t wrapMul(int64_t a, int64_t b) { return static_cast<int64_t>(static_cast<uint64_t>(a) * static_cast<uint64_t>(b)); }

class VM {
public:
    VM(const Module& module, std::ostream& out) : module(module), out(out) {}

    void run() {
        callFunction(module.mainIndex);

        // THE FETCH-DECODE-EXECUTE LOOP
        while (!frames.empty()) {
            Frame& frame = frames.back();
            const Instr& in = frame.fn->code[frame.ip++];

            switch (in.op) {
            case Op::Push:  push(in.arg); break;
            case Op::Pop:   pop(); break;
            case Op::Load:  push(stack[frame.base + in.arg]); break;
            case Op::Store: stack[frame.base + in.arg] = pop(); break;

            // Note the order: the RIGHT operand is on top, so it's popped first.
            case Op::Add: { int64_t b = pop(), a = pop(); push(wrapAdd(a, b)); break; }
            case Op::Sub: { int64_t b = pop(), a = pop(); push(wrapSub(a, b)); break; }
            case Op::Mul: { int64_t b = pop(), a = pop(); push(wrapMul(a, b)); break; }
            case Op::Div: {
                int64_t b = pop(), a = pop();
                if (b == 0) throw RuntimeError("division by zero in '" + frame.fn->name + "'");
                if (a == std::numeric_limits<int64_t>::min() && b == -1) push(a);  // overflow wraps
                else push(a / b);
                break;
            }
            case Op::Mod: {
                int64_t b = pop(), a = pop();
                if (b == 0) throw RuntimeError("division by zero in '" + frame.fn->name + "'");
                if (b == -1) push(0);
                else push(a % b);
                break;
            }
            case Op::Neg: push(wrapSub(0, pop())); break;
            case Op::Not: push(pop() == 0 ? 1 : 0); break;

            case Op::Eq: { int64_t b = pop(), a = pop(); push(a == b); break; }
            case Op::Ne: { int64_t b = pop(), a = pop(); push(a != b); break; }
            case Op::Lt: { int64_t b = pop(), a = pop(); push(a < b); break; }
            case Op::Le: { int64_t b = pop(), a = pop(); push(a <= b); break; }
            case Op::Gt: { int64_t b = pop(), a = pop(); push(a > b); break; }
            case Op::Ge: { int64_t b = pop(), a = pop(); push(a >= b); break; }

            case Op::Jump:
                frame.ip = static_cast<size_t>(in.arg);
                break;
            case Op::JumpIfFalse:
                if (pop() == 0) frame.ip = static_cast<size_t>(in.arg);
                break;

            case Op::Call:
                // Careful: this pushes onto `frames`, which may reallocate and
                // invalidate the `frame` reference. We don't touch it again in
                // this iteration, and the next iteration re-reads frames.back().
                callFunction(static_cast<int>(in.arg));
                break;

            case Op::Ret: {
                int64_t result = pop();
                size_t base = frame.base;
                frames.pop_back();     // back to the caller's frame
                stack.resize(base);    // discard the callee's args, locals, and temporaries
                push(result);          // the caller sees the result on top of its stack
                break;
            }

            case Op::PrintInt:  out << pop() << "\n"; break;
            case Op::PrintBool: out << (pop() ? "true" : "false") << "\n"; break;
            case Op::PrintStr:  out << module.strings[static_cast<size_t>(pop())] << "\n"; break;
            }
        }
    }

private:
    const Module& module;
    std::ostream& out;
    std::vector<int64_t> stack;
    std::vector<Frame> frames;

    void push(int64_t v) { stack.push_back(v); }

    // No underflow check: the code generator guarantees the stack is
    // balanced. (A VM that runs untrusted bytecode, like the JVM, has to
    // *verify* that up front.)
    int64_t pop() {
        int64_t v = stack.back();
        stack.pop_back();
        return v;
    }

    void callFunction(int index) {
        if (frames.size() >= kMaxFrames) {
            throw RuntimeError("stack overflow (too much recursion)");
        }
        const Function& fn = module.functions[static_cast<size_t>(index)];
        // The caller already pushed numParams arguments, so the frame starts there.
        size_t base = stack.size() - static_cast<size_t>(fn.numParams);
        // Make room for the rest of the locals (the let-variables), zeroed.
        for (int i = fn.numParams; i < fn.numLocals; i++) push(0);
        frames.push_back(Frame{&fn, 0, base});
    }
};

}  // namespace

void execute(const Module& module, std::ostream& out) {
    VM(module, out).run();
}
