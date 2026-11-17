// The Pebble grammar, in EBNF. Each rule has a matching parseXxx() method below.
//
//   program    = function* EOF
//   function   = "fn" IDENT "(" params? ")" ( "->" type )? block
//   params     = param ( "," param )*
//   param      = IDENT ":" type
//   type       = IDENT                                  (int, bool, str)
//   block      = "{" statement* "}"
//
//   statement  = "let" IDENT ( ":" type )? "=" expr ";"
//              | IDENT "=" expr ";"                     (assignment)
//              | ifStmt
//              | "while" expr block
//              | "return" expr? ";"
//              | "print" "(" expr ")" ";"
//              | block
//              | expr ";"
//   ifStmt     = "if" expr block ( "else" ( ifStmt | block ) )?
//
//   expr       = unary ( BINOP unary )*                 (precedence climbing, see below)
//   unary      = ( "-" | "!" ) unary | primary
//   primary    = INT | STRING | "true" | "false"
//              | IDENT                                  (variable)
//              | IDENT "(" ( expr ( "," expr )* )? ")"  (call)
//              | "(" expr ")"
//
// Binary operator precedence, from loosest to tightest binding:
//   1: ||    2: &&    3: == !=    4: < <= > >=    5: + -    6: * / %

#include "parser.h"

#include <algorithm>

namespace {

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens) : tokens(tokens) {}

    std::unique_ptr<Program> parseProgram() {
        auto program = std::make_unique<Program>();
        while (!check(TokenKind::Eof)) {
            program->funcs.push_back(parseFunction());
        }
        return program;
    }

private:
    const std::vector<Token>& tokens;
    size_t pos = 0;

    // --- Token-stream helpers ------------------------------------------
    // These mirror the lexer's character helpers, but one level up.

    const Token& peek(size_t ahead = 0) const {
        // The last token is always Eof, so clamping keeps us safe.
        return tokens[std::min(pos + ahead, tokens.size() - 1)];
    }

    bool check(TokenKind kind) const { return peek().kind == kind; }

    const Token& advance() {
        const Token& t = tokens[pos];
        if (t.kind != TokenKind::Eof) pos++;
        return t;
    }

    bool match(TokenKind kind) {
        if (!check(kind)) return false;
        advance();
        return true;
    }

    // "The grammar says a ';' MUST come next." If it doesn't, that's a syntax error.
    const Token& expect(TokenKind kind, const std::string& what) {
        if (!check(kind)) {
            throw CompileError(peek().loc, "expected " + what + ", found " + describe(peek()));
        }
        return advance();
    }

    static std::string describe(const Token& t) {
        if (t.kind == TokenKind::Eof) return "end of file";
        return "'" + t.text + "'";
    }

    // --- Declarations --------------------------------------------------

    std::unique_ptr<FuncDecl> parseFunction() {
        auto fn = std::make_unique<FuncDecl>();
        fn->loc = expect(TokenKind::Fn, "'fn'").loc;
        fn->name = expect(TokenKind::Ident, "function name").text;

        expect(TokenKind::LParen, "'('");
        if (!check(TokenKind::RParen)) {
            do {
                Param p;
                const Token& name = expect(TokenKind::Ident, "parameter name");
                p.name = name.text;
                p.loc = name.loc;
                expect(TokenKind::Colon, "':' after parameter name");
                p.typeName = parseType();
                fn->params.push_back(std::move(p));
            } while (match(TokenKind::Comma));
        }
        expect(TokenKind::RParen, "')'");

        if (match(TokenKind::Arrow)) fn->returnTypeName = parseType();

        fn->body = parseBlock();
        return fn;
    }

    // Types are just names at this point. Whether "int" means anything is
    // the type checker's problem, not the parser's.
    TypeName parseType() {
        const Token& t = expect(TokenKind::Ident, "a type name");
        return TypeName{t.text, t.loc};
    }

    // --- Statements ----------------------------------------------------

    std::unique_ptr<BlockStmt> parseBlock() {
        auto block = std::make_unique<BlockStmt>(expect(TokenKind::LBrace, "'{'").loc);
        while (!check(TokenKind::RBrace) && !check(TokenKind::Eof)) {
            block->stmts.push_back(parseStatement());
        }
        expect(TokenKind::RBrace, "'}'");
        return block;
    }

    // The first token (sometimes two) decides which kind of statement this is.
    // That's why it's called "predictive" parsing: we never have to backtrack.
    StmtPtr parseStatement() {
        SourceLoc loc = peek().loc;

        switch (peek().kind) {
        case TokenKind::Let: {
            advance();
            std::string name = expect(TokenKind::Ident, "variable name").text;
            std::optional<TypeName> type;
            if (match(TokenKind::Colon)) type = parseType();
            expect(TokenKind::Assign, "'='");
            ExprPtr init = parseExpression();
            expect(TokenKind::Semicolon, "';'");
            return std::make_unique<LetStmt>(loc, name, std::move(type), std::move(init));
        }
        case TokenKind::If:
            return parseIf();
        case TokenKind::While: {
            advance();
            ExprPtr cond = parseExpression();
            auto body = parseBlock();
            return std::make_unique<WhileStmt>(loc, std::move(cond), std::move(body));
        }
        case TokenKind::Return: {
            advance();
            ExprPtr value;
            if (!check(TokenKind::Semicolon)) value = parseExpression();
            expect(TokenKind::Semicolon, "';'");
            return std::make_unique<ReturnStmt>(loc, std::move(value));
        }
        case TokenKind::Print: {
            advance();
            expect(TokenKind::LParen, "'(' after print");
            ExprPtr value = parseExpression();
            expect(TokenKind::RParen, "')'");
            expect(TokenKind::Semicolon, "';'");
            return std::make_unique<PrintStmt>(loc, std::move(value));
        }
        case TokenKind::LBrace:
            return parseBlock();
        default:
            break;
        }

        // `x = ...` vs an expression like `f(x);`. Both start with an IDENT,
        // so we look TWO tokens ahead to tell them apart.
        if (check(TokenKind::Ident) && peek(1).kind == TokenKind::Assign) {
            std::string name = advance().text;
            advance();  // '='
            ExprPtr value = parseExpression();
            expect(TokenKind::Semicolon, "';'");
            return std::make_unique<AssignStmt>(loc, name, std::move(value));
        }

        ExprPtr expr = parseExpression();
        expect(TokenKind::Semicolon, "';'");
        return std::make_unique<ExprStmt>(loc, std::move(expr));
    }

    std::unique_ptr<IfStmt> parseIf() {
        SourceLoc loc = expect(TokenKind::If, "'if'").loc;
        ExprPtr cond = parseExpression();
        auto thenBlock = parseBlock();
        StmtPtr elseBranch;
        if (match(TokenKind::Else)) {
            // "else if" is not special syntax. It's just an else whose body
            // is another if statement.
            if (check(TokenKind::If)) elseBranch = parseIf();
            else elseBranch = parseBlock();
        }
        return std::make_unique<IfStmt>(loc, std::move(cond), std::move(thenBlock), std::move(elseBranch));
    }

    // --- Expressions ---------------------------------------------------

    // 0 means "not a binary operator", which makes the loop below stop.
    static int precedence(TokenKind kind) {
        switch (kind) {
        case TokenKind::OrOr:      return 1;
        case TokenKind::AndAnd:    return 2;
        case TokenKind::EqEq:
        case TokenKind::BangEq:    return 3;
        case TokenKind::Less:
        case TokenKind::LessEq:
        case TokenKind::Greater:
        case TokenKind::GreaterEq: return 4;
        case TokenKind::Plus:
        case TokenKind::Minus:     return 5;
        case TokenKind::Star:
        case TokenKind::Slash:
        case TokenKind::Percent:   return 6;
        default:                   return 0;
        }
    }

    // PRECEDENCE CLIMBING
    //
    // parseExpression(minPrec) parses an expression that only contains
    // operators binding at least as tightly as minPrec.
    //
    // Example: 1 + 2 * 3 - 4
    //   left = 1
    //   see '+' (prec 5): parse its right side with minPrec 6, which grabs
    //       "2 * 3" (since '*' is 6) but stops at '-' (only 5).
    //       left = (1 + (2*3))
    //   see '-' (prec 5): right side = 4.  left = ((1 + (2*3)) - 4)
    //
    // Passing `prec + 1` for the right side makes operators LEFT-associative:
    // 10 - 3 - 2 parses as (10 - 3) - 2, not 10 - (3 - 2).
    ExprPtr parseExpression(int minPrec = 1) {
        ExprPtr left = parseUnary();
        while (true) {
            int prec = precedence(peek().kind);
            if (prec == 0 || prec < minPrec) break;
            Token op = advance();
            ExprPtr right = parseExpression(prec + 1);
            left = std::make_unique<BinaryExpr>(op.loc, op.kind, std::move(left), std::move(right));
        }
        return left;
    }

    ExprPtr parseUnary() {
        if (check(TokenKind::Minus) || check(TokenKind::Bang)) {
            Token op = advance();
            ExprPtr operand = parseUnary();  // allows --x and !!b
            return std::make_unique<UnaryExpr>(op.loc, op.kind, std::move(operand));
        }
        return parsePrimary();
    }

    ExprPtr parsePrimary() {
        const Token& t = peek();
        switch (t.kind) {
        case TokenKind::Int:
            advance();
            return std::make_unique<IntLitExpr>(t.loc, t.intValue);
        case TokenKind::True:
        case TokenKind::False:
            advance();
            return std::make_unique<BoolLitExpr>(t.loc, t.kind == TokenKind::True);
        case TokenKind::String:
            advance();
            return std::make_unique<StrLitExpr>(t.loc, t.stringValue);
        case TokenKind::Ident: {
            advance();
            if (!match(TokenKind::LParen)) {
                return std::make_unique<VarExpr>(t.loc, t.text);
            }
            std::vector<ExprPtr> args;
            if (!check(TokenKind::RParen)) {
                do {
                    args.push_back(parseExpression());
                } while (match(TokenKind::Comma));
            }
            expect(TokenKind::RParen, "')' after arguments");
            return std::make_unique<CallExpr>(t.loc, t.text, std::move(args));
        }
        case TokenKind::LParen: {
            // Parentheses only affect how the tree is shaped. They leave no
            // node behind, which is part of what makes the tree "abstract".
            advance();
            ExprPtr inner = parseExpression();
            expect(TokenKind::RParen, "')'");
            return inner;
        }
        default:
            throw CompileError(t.loc, "expected an expression, found " + describe(t));
        }
    }
};

}  // namespace

std::unique_ptr<Program> parse(const std::vector<Token>& tokens) {
    return Parser(tokens).parseProgram();
}
