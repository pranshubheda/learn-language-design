# Pebble: a small compiler to learn from

Pebble is a tiny statically typed language, and this repo is its complete
compiler, written in about 1,500 lines of commented C++17. It has every
phase you'd meet in a university compilers course:

```
source text ─► LEXER ─► tokens ─► PARSER ─► AST ─► TYPE CHECKER ─► typed AST ─► CODEGEN ─► bytecode ─► VM ─► output
              lexer.cpp          parser.cpp       typechecker.cpp              codegen.cpp           vm.cpp
```

```rust
fn fib(n: int) -> int {
    if n < 2 { return n; }
    return fib(n - 1) + fib(n - 2);
}

fn main() {
    let i = 0;
    while i < 10 {
        print(fib(i));
        i = i + 1;
    }
}
```

---

## 1. Build and run

```bash
make                              # builds ./pebble
./pebble examples/fib.pb          # compile and run
make test                         # runs every example and error test
```

Every phase can be stopped early so you can **see what it produced**. This is
the best learning tool in the project:

```bash
./pebble --tokens   examples/hello.pb   # after the lexer
./pebble --ast      examples/fib.pb     # after the parser + type checker (shows types and slots)
./pebble --bytecode examples/fib.pb     # after codegen
```

Try breaking things on purpose: delete a `;`, write `"a" + 1`, or remove a
`return`. Then read the error and find the line in the compiler that produced it.

---

## 2. VS Code: run and debug

The `.vscode/` folder is already configured. You need the **C/C++ extension**
(`ms-vscode.cpptools`). VS Code offers to install it because it's listed in
`.vscode/extensions.json`.

### Running (tasks)

| What | How |
|---|---|
| Build | `Ctrl+Shift+B` |
| Run the tests | `Ctrl+Shift+P` → **Tasks: Run Test Task** |
| Run/inspect the `.pb` file you have open | `Ctrl+Shift+P` → **Tasks: Run Task** → **pebble: run / inspect current file**, then pick `--run`, `--tokens`, `--ast`, or `--bytecode` |

Or just use the built-in terminal (`` Ctrl+` ``) and type the commands from section 1.

### Debugging (stepping through the compiler)

1. Set a **breakpoint**: click in the gutter left of a line number (a red dot appears).
2. Open the **Run and Debug** sidebar (`Ctrl+Shift+D`) and pick a configuration at the top:
   - **Debug compiler on current .pb file**: debugs `pebble` on whatever `.pb` file is in the active editor tab.
     (Click into the `.pb` tab before pressing F5, not a `.cpp` tab.)
   - **Debug compiler on an example**: lets you choose from a list.
3. Press **F5**. It rebuilds, asks which mode (`--run`, `--ast`, ...), and stops at your breakpoint.

| Key | Action |
|---|---|
| `F5` | continue to the next breakpoint |
| `F10` | step **over** (run this line, don't enter function calls) |
| `F11` | step **into** the function call on this line |
| `Shift+F11` | step **out** (finish this function, return to the caller) |
| `Shift+F5` | stop |

While stopped, look at:
- **VARIABLES** panel: locals like `tokens`, `stack`, `frames`. Expand them.
- **WATCH** panel: add expressions such as `peek()`, `stack`, `frame.ip`, `in.op`, `e.loc.line`.
- **CALL STACK** panel: *very* instructive in the parser. Each frame is one grammar
  rule currently being parsed (parseExpression → parseUnary → parsePrimary ...).

### Good breakpoints to try

| Breakpoint | Program | What you'll see |
|---|---|---|
| `lexer.cpp`, first line of `nextToken()` | `hello.pb` | each token being cut from the character stream |
| `parser.cpp`, the `while (true)` in `parseExpression` | `fib.pb` | precedence climbing; watch `minPrec` and the call stack |
| `typechecker.cpp`, `case ExprKind::Binary` | `tests/errors/type_bad_operator.pb` | the exact moment a type error is detected |
| `codegen.cpp`, `case StmtKind::While` | `fib.pb` with `--bytecode` | the jump being emitted, then backpatched |
| `vm.cpp`, the `switch (in.op)` line | `fib.pb` | the VM executing; watch `stack` and `frames` change at every F5 |

**Tip:** in the VM a plain breakpoint fires on every instruction. Right-click
the red dot → **Edit Breakpoint** → **Expression** and enter `in.op == Op::Call`
to stop only on function calls.

---

## 3. Reading order

Read the files in this order. Each header (`.h`) explains the *idea* of the
phase, and the `.cpp` file is the implementation.

| # | File | Concept to understand |
|---|---|---|
| 0 | `src/main.cpp` | The pipeline. How phases hand data to each other. |
| 1 | `src/lexer.h/.cpp` | **Lexical analysis**: characters into tokens, lookahead (`peek`), keywords vs identifiers, 1- vs 2-char operators. |
| 2 | `src/ast.h` | **The AST**: what a tree of the program looks like, and fields left empty for later phases ("decoration"). |
| 3 | `src/parser.cpp` | **Recursive descent** (one function per grammar rule, grammar at the top of the file) and **precedence climbing** for operators. |
| 4 | `src/typechecker.cpp` | **Semantic analysis**: symbol tables and scopes, typing rules, two-pass signature collection, definite-return analysis. |
| 5 | `src/bytecode.h` | **Stack machines**, and why types are *erased* after checking. |
| 6 | `src/codegen.cpp` | **Code generation**: an AST walk that emits instructions; control flow becomes jumps; **backpatching**; short-circuit `&&`/`\|\|`. |
| 7 | `src/vm.h/.cpp` | **Runtime**: fetch-decode-execute loop, call frames, how arguments become locals. |

Questions to be able to answer after each phase:

- **Lexer:** Why does `->` need `match('>')`? Where does a comment go?
- **Parser:** Trace `1 + 2 * 3 - 4` through `parseExpression` by hand. Why `prec + 1`? How does `x = 1;` get told apart from `x;`?
- **Type checker:** Why does `let x = x + 1;` check the right side *before* declaring `x`? Why doesn't a `while` loop count as "always returns"?
- **Codegen:** Why does `ExprStmt` emit a `POP`? Draw the jumps for an `if/else`.
- **VM:** In `fib(3)`, what does the stack look like right after the inner `CALL`? (Debug it to check.)

---

## 4. The Pebble language

**Types:** `int` (64-bit), `bool`, `str` (literals only; they can be stored, passed, and printed).

```rust
fn name(a: int, b: bool) -> int { ... }   // "-> type" omitted means no return value
let x: int = 5;          // explicit type
let y = x * 2;           // inferred type
y = y + 1;               // assignment (types must match)
if cond { } else if c2 { } else { }
while cond { }
return expr;   return;
print(expr);             // works for int, bool, str
{ let x = 1; }           // blocks create scopes; inner `let` may shadow outer
```

**Operators** (from loosest to tightest): `||`, `&&`, `== !=`, `< <= > >=`, `+ -`, `* / %`, unary `- !`.
`&&` and `||` short-circuit. `==` works on two ints or two bools.

**Rules the type checker enforces:** names must be declared before use; no
duplicate names in one scope; conditions must be `bool`; argument counts and
types must match; non-void functions must return on every path; `main()` must
exist, take nothing, and return nothing.

---

## 5. Project layout

```
src/            the compiler (read in the order of section 3)
examples/       sample programs + their .expected outputs
tests/errors/   programs that MUST fail; line 1 says which error to expect
tests/run_tests.sh
.vscode/        build tasks + debug configurations
```

---

## 6. Exercises (when you're ready to write code)

Roughly from easiest to hardest. Each one touches several phases, which is
the point: you'll feel how they connect.

1. **`==` for strings.** Allow `str == str` in the type checker. Is it safe
   for the VM to just compare string *indices*? (Hint: look at `internString`.
   Would the answer change if you later add string concatenation?)
2. **Block comments** `/* ... */` in the lexer.
3. **A `for` loop:** `for i = 0; i < 10; i = i + 1 { ... }`. You can do it with *zero* new
   opcodes by **desugaring** it into a `while` in the parser.
4. **`break` / `continue`:** the code generator needs a stack of jump lists to backpatch.
5. **Unreachable code error:** report statements that come after a `return` in the same block.
6. **Constant folding:** in codegen, if both operands of `+` are `IntLit`, emit one `PUSH`
   with the result. This is your first optimization.
7. **Better errors:** collect *all* type errors instead of stopping at the first.
8. **Global variables**, or **arrays** (`[int]`). These need new types, opcodes, and a heap.
9. **A second backend:** emit C source instead of bytecode, then compile it with gcc.

---

## 7. Further reading

- *Crafting Interpreters* (Robert Nystrom), free at craftinginterpreters.com. Part III builds a bytecode VM in C, very close to this project.
- *Engineering a Compiler* (Cooper & Torczon): the standard modern textbook.
- *Modern Compiler Implementation in C/ML/Java* (Appel): classic course book, strong on IR and backends.
- *Types and Programming Languages* (Pierce): for when you want the theory behind type checkers.
- *Compilers: Principles, Techniques, and Tools* ("the Dragon Book"): the classic, heavy on parsing theory.
