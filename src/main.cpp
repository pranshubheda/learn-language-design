// The compiler driver: runs each phase in order. READ THIS FILE FIRST.
//
//   source text
//      |  lex()        lexer.cpp        characters -> tokens
//      v
//   tokens
//      |  parse()      parser.cpp       tokens -> tree
//      v
//   AST
//      |  typecheck()  typechecker.cpp  checks meaning, decorates the tree
//      v
//   typed AST
//      |  generate()   codegen.cpp      tree -> flat instructions
//      v
//   bytecode
//      |  execute()    vm.cpp           runs the instructions
//      v
//   output
//
// The flags let you stop after any phase and look at what it produced.

#include "ast.h"
#include "bytecode.h"
#include "codegen.h"
#include "lexer.h"
#include "parser.h"
#include "typechecker.h"
#include "vm.h"

#include <fstream>
#include <iostream>
#include <sstream>

namespace
{

    void printUsage()
    {
        std::cerr << "usage: pebble [--run | --tokens | --ast | --bytecode] <file.pb>\n"
                     "\n"
                     "  --run        compile and run the program (the default)\n"
                     "  --tokens     stop after lexing and print the tokens\n"
                     "  --ast        stop after type checking and print the typed AST\n"
                     "  --bytecode   stop after code generation and print the bytecode\n";
    }

    // Prints an error like:
    //
    //   examples/bad.pb:3:18: error: operator '+' expects int operands, found str and int
    //     print("a" + 1);
    //               ^
    void reportCompileError(const std::string &path, const std::string &source, const CompileError &e)
    {
        std::cerr << path << ":" << e.loc.line << ":" << e.loc.col << ": error: " << e.what() << "\n";

        std::istringstream lines(source);
        std::string text;
        for (int i = 0; i < e.loc.line; i++)
        {
            if (!std::getline(lines, text))
                return;
        }
        // Copy tabs from the source line so the caret lines up under the error.
        std::string pad;
        for (int i = 0; i < e.loc.col - 1 && i < static_cast<int>(text.size()); i++)
        {
            pad += (text[i] == '\t') ? '\t' : ' ';
        }
        std::cerr << "  " << text << "\n  " << pad << "^\n";
    }

} // namespace

int main(int argc, char **argv)
{
    std::string mode = "run";
    std::string path;
    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--run" || arg == "--tokens" || arg == "--ast" || arg == "--bytecode")
        {
            mode = arg.substr(2);
        }
        else if (!arg.empty() && arg[0] == '-')
        {
            printUsage();
            return 2;
        }
        else
        {
            path = arg;
        }
    }
    if (path.empty())
    {
        printUsage();
        return 2;
    }

    std::ifstream file(path);
    if (!file)
    {
        std::cerr << "pebble: cannot open '" << path << "'\n";
        return 2;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    try
    {
        // Phase 1: lexing
        std::vector<Token> tokens = lex(source);
        if (mode == "tokens")
        {
            for (const Token &t : tokens)
            {
                std::cout << t.loc.line << ":" << t.loc.col << "\t" << tokenKindName(t.kind) << "\t" << t.text << "\n";
            }
            return 0;
        }

        // Phase 2: parsing
        std::unique_ptr<Program> program = parse(tokens);

        // Phase 3: semantic analysis
        typecheck(*program);
        if (mode == "ast")
        {
            printAst(*program, std::cout);
            return 0;
        }

        // Phase 4: code generation
        Module module = generate(*program);
        if (mode == "bytecode")
        {
            disassemble(module, std::cout);
            return 0;
        }

        // Phase 5: execution
        execute(module, std::cout);
    }
    catch (const CompileError &e)
    {
        reportCompileError(path, source, e);
        return 1;
    }
    catch (const RuntimeError &e)
    {
        std::cout.flush();
        std::cerr << "runtime error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
