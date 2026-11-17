#include "lexer.h"

#include <cctype>
#include <unordered_map>

const char* tokenKindName(TokenKind kind) {
    switch (kind) {
    case TokenKind::Int:       return "integer";
    case TokenKind::String:    return "string";
    case TokenKind::Ident:     return "identifier";
    case TokenKind::Fn:        return "fn";
    case TokenKind::Let:       return "let";
    case TokenKind::If:        return "if";
    case TokenKind::Else:      return "else";
    case TokenKind::While:     return "while";
    case TokenKind::Return:    return "return";
    case TokenKind::Print:     return "print";
    case TokenKind::True:      return "true";
    case TokenKind::False:     return "false";
    case TokenKind::LParen:    return "(";
    case TokenKind::RParen:    return ")";
    case TokenKind::LBrace:    return "{";
    case TokenKind::RBrace:    return "}";
    case TokenKind::Comma:     return ",";
    case TokenKind::Colon:     return ":";
    case TokenKind::Semicolon: return ";";
    case TokenKind::Arrow:     return "->";
    case TokenKind::Plus:      return "+";
    case TokenKind::Minus:     return "-";
    case TokenKind::Star:      return "*";
    case TokenKind::Slash:     return "/";
    case TokenKind::Percent:   return "%";
    case TokenKind::Bang:      return "!";
    case TokenKind::Assign:    return "=";
    case TokenKind::EqEq:      return "==";
    case TokenKind::BangEq:    return "!=";
    case TokenKind::Less:      return "<";
    case TokenKind::LessEq:    return "<=";
    case TokenKind::Greater:   return ">";
    case TokenKind::GreaterEq: return ">=";
    case TokenKind::AndAnd:    return "&&";
    case TokenKind::OrOr:      return "||";
    case TokenKind::Eof:       return "end of file";
    }
    return "?";
}

namespace {

// After reading a word like "while", we look it up here. If it's not a
// keyword, it's an ordinary identifier (a variable, function, or type name).
const std::unordered_map<std::string, TokenKind> keywords = {
    {"fn", TokenKind::Fn},         {"let", TokenKind::Let},
    {"if", TokenKind::If},         {"else", TokenKind::Else},
    {"while", TokenKind::While},   {"return", TokenKind::Return},
    {"print", TokenKind::Print},   {"true", TokenKind::True},
    {"false", TokenKind::False},
};

class Lexer {
public:
    explicit Lexer(const std::string& source) : src(source) {}

    std::vector<Token> run() {
        std::vector<Token> tokens;
        while (true) {
            skipWhitespaceAndComments();
            // Remember where this token starts, for its text and location.
            startPos = pos;
            startLoc = {line, col};
            if (atEnd()) {
                tokens.push_back(make(TokenKind::Eof));
                return tokens;
            }
            tokens.push_back(nextToken());
        }
    }

private:
    const std::string& src;
    size_t pos = 0;          // index of the next unread character
    int line = 1, col = 1;   // human-friendly position of `pos`
    size_t startPos = 0;     // where the current token began
    SourceLoc startLoc;

    // --- Character-level helpers ---------------------------------------

    bool atEnd() const { return pos >= src.size(); }

    // Look at a character without consuming it. "Lookahead" is the core
    // idea of hand-written lexers and parsers.
    char peek(size_t ahead = 0) const {
        return pos + ahead < src.size() ? src[pos + ahead] : '\0';
    }

    char advance() {
        char c = src[pos++];
        if (c == '\n') { line++; col = 1; } else { col++; }
        return c;
    }

    // Consume the next character only if it is `expected`.
    // This is how two-character operators like "==" and "->" are recognized.
    bool match(char expected) {
        if (peek() != expected) return false;
        advance();
        return true;
    }

    Token make(TokenKind kind) const {
        Token t;
        t.kind = kind;
        t.text = src.substr(startPos, pos - startPos);
        t.loc = startLoc;
        return t;
    }

    static bool isDigit(char c) { return std::isdigit(static_cast<unsigned char>(c)); }
    static bool isIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
    static bool isIdentChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

    // --- Token-level logic --------------------------------------------

    void skipWhitespaceAndComments() {
        while (!atEnd()) {
            char c = peek();
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                advance();
            } else if (c == '/' && peek(1) == '/') {
                while (!atEnd() && peek() != '\n') advance();  // comment runs to end of line
            } else {
                return;
            }
        }
    }

    Token nextToken() {
        char c = advance();

        if (isDigit(c)) return number();
        if (isIdentStart(c)) return identifierOrKeyword();

        switch (c) {
        case '"': return string();
        case '(': return make(TokenKind::LParen);
        case ')': return make(TokenKind::RParen);
        case '{': return make(TokenKind::LBrace);
        case '}': return make(TokenKind::RBrace);
        case ',': return make(TokenKind::Comma);
        case ':': return make(TokenKind::Colon);
        case ';': return make(TokenKind::Semicolon);
        case '+': return make(TokenKind::Plus);
        case '*': return make(TokenKind::Star);
        case '/': return make(TokenKind::Slash);
        case '%': return make(TokenKind::Percent);
        // Characters that may start a one- OR two-character token:
        case '-': return make(match('>') ? TokenKind::Arrow : TokenKind::Minus);
        case '=': return make(match('=') ? TokenKind::EqEq : TokenKind::Assign);
        case '!': return make(match('=') ? TokenKind::BangEq : TokenKind::Bang);
        case '<': return make(match('=') ? TokenKind::LessEq : TokenKind::Less);
        case '>': return make(match('=') ? TokenKind::GreaterEq : TokenKind::Greater);
        case '&': if (match('&')) return make(TokenKind::AndAnd); break;
        case '|': if (match('|')) return make(TokenKind::OrOr); break;
        }
        throw CompileError(startLoc, std::string("unexpected character '") + c + "'");
    }

    Token number() {
        while (isDigit(peek())) advance();
        Token t = make(TokenKind::Int);
        try {
            t.intValue = std::stoll(t.text);
        } catch (const std::out_of_range&) {
            throw CompileError(startLoc, "integer literal '" + t.text + "' is too large");
        }
        return t;
    }

    Token identifierOrKeyword() {
        while (isIdentChar(peek())) advance();
        std::string word = src.substr(startPos, pos - startPos);
        auto it = keywords.find(word);
        return make(it != keywords.end() ? it->second : TokenKind::Ident);
    }

    // The opening quote has already been consumed.
    Token string() {
        std::string value;
        while (true) {
            if (atEnd() || peek() == '\n')
                throw CompileError(startLoc, "unterminated string literal");
            char c = advance();
            if (c == '"') break;
            if (c != '\\') { value += c; continue; }

            // Escape sequence: backslash followed by one character.
            SourceLoc escLoc = {line, col - 1};
            if (atEnd()) throw CompileError(startLoc, "unterminated string literal");
            char e = advance();
            switch (e) {
            case 'n':  value += '\n'; break;
            case 't':  value += '\t'; break;
            case '"':  value += '"';  break;
            case '\\': value += '\\'; break;
            default:
                throw CompileError(escLoc, std::string("unknown escape sequence '\\") + e + "'");
            }
        }
        Token t = make(TokenKind::String);
        t.stringValue = value;
        return t;
    }
};

}  // namespace

std::vector<Token> lex(const std::string& source) {
    return Lexer(source).run();
}
