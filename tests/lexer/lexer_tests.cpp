#include <iostream>
#include <initializer_list>
#include <string>
#include <vector>

#include "kite/lexer.hpp"

namespace {

bool check(bool condition, const char* name) {
    std::cout << (condition ? "[PASS] " : "[FAIL] ") << name << '\n';
    return condition;
}

std::vector<kite::Token> tokenize(const std::string& source) {
    kite::Lexer lexer(source);
    std::vector<kite::Token> tokens;
    kite::Token token = lexer.next_token();
    while (token.type != kite::TokenType::Eof) {
        tokens.push_back(token);
        token = lexer.next_token();
    }
    tokens.push_back(token);
    return tokens;
}

bool has_types(const std::vector<kite::Token>& tokens, std::initializer_list<kite::TokenType> expected) {
    if (tokens.size() != expected.size()) {
        return false;
    }

    std::size_t index = 0;
    for (const auto type : expected) {
        if (tokens[index++].type != type) {
            return false;
        }
    }
    return true;
}

} // namespace

int main() {
    std::cout << "## Kite lexer tests\n";
    bool all_passed = true;

    all_passed &= check(
        has_types(tokenize("let x = 42"), {kite::TokenType::Let, kite::TokenType::Identifier,
            kite::TokenType::Equal, kite::TokenType::Integer, kite::TokenType::Eof}),
        "let declaration");
    all_passed &= check(
        has_types(tokenize("let name = \"Kite\""), {kite::TokenType::Let, kite::TokenType::Identifier,
            kite::TokenType::Equal, kite::TokenType::String, kite::TokenType::Eof}),
        "string declaration");
    all_passed &= check(
        has_types(tokenize("print(x)"), {kite::TokenType::Identifier, kite::TokenType::LeftParen,
            kite::TokenType::Identifier, kite::TokenType::RightParen, kite::TokenType::Eof}),
        "function-style call");
    all_passed &= check(
        has_types(tokenize("print(x, y)"), {kite::TokenType::Identifier, kite::TokenType::LeftParen,
            kite::TokenType::Identifier, kite::TokenType::Comma, kite::TokenType::Identifier,
            kite::TokenType::RightParen, kite::TokenType::Eof}),
        "comma-separated arguments");
    all_passed &= check(
        has_types(tokenize("1 + 2 - 3 * 4 / 5"), {kite::TokenType::Integer, kite::TokenType::Plus,
            kite::TokenType::Integer, kite::TokenType::Minus, kite::TokenType::Integer,
            kite::TokenType::Star, kite::TokenType::Integer, kite::TokenType::Slash,
            kite::TokenType::Integer, kite::TokenType::Eof}),
        "arithmetic operators");
    all_passed &= check(
        has_types(tokenize("3.14"), {kite::TokenType::Float, kite::TokenType::Eof}),
        "floating-point literal");
    all_passed &= check(
        has_types(tokenize("-4"), {kite::TokenType::Minus, kite::TokenType::Integer, kite::TokenType::Eof}),
        "unary minus input");
    all_passed &= check(
        has_types(tokenize("!true && false || true"), {kite::TokenType::Bang, kite::TokenType::True,
            kite::TokenType::AndAnd, kite::TokenType::False, kite::TokenType::OrOr,
            kite::TokenType::True, kite::TokenType::Eof}),
        "logical operators");
    all_passed &= check(
        has_types(tokenize("if (true) { x } else { y }"), {kite::TokenType::If,
            kite::TokenType::LeftParen, kite::TokenType::True, kite::TokenType::RightParen,
            kite::TokenType::LeftBrace, kite::TokenType::Identifier, kite::TokenType::RightBrace,
            kite::TokenType::Else, kite::TokenType::LeftBrace, kite::TokenType::Identifier,
            kite::TokenType::RightBrace, kite::TokenType::Eof}),
        "conditional keywords and blocks");
    all_passed &= check(
        has_types(tokenize("1 == 1 != 2 < 3 <= 3 > 2 >= 2"), {kite::TokenType::Integer,
            kite::TokenType::EqualEqual, kite::TokenType::Integer, kite::TokenType::BangEqual,
            kite::TokenType::Integer, kite::TokenType::Less, kite::TokenType::Integer,
            kite::TokenType::LessEqual, kite::TokenType::Integer, kite::TokenType::Greater,
            kite::TokenType::Integer, kite::TokenType::GreaterEqual, kite::TokenType::Integer,
            kite::TokenType::Eof}),
        "comparison operators");
    all_passed &= check(
        has_types(tokenize("fn add(a, b) { return a + b }"), {kite::TokenType::Fn,
            kite::TokenType::Identifier, kite::TokenType::LeftParen, kite::TokenType::Identifier,
            kite::TokenType::Comma, kite::TokenType::Identifier, kite::TokenType::RightParen,
            kite::TokenType::LeftBrace, kite::TokenType::Return, kite::TokenType::Identifier,
            kite::TokenType::Plus, kite::TokenType::Identifier, kite::TokenType::RightBrace,
            kite::TokenType::Eof}),
        "function and return keywords");
    all_passed &= check(
        has_types(tokenize("[1, 2][0]"), {kite::TokenType::LeftBracket, kite::TokenType::Integer,
            kite::TokenType::Comma, kite::TokenType::Integer, kite::TokenType::RightBracket,
            kite::TokenType::LeftBracket, kite::TokenType::Integer, kite::TokenType::RightBracket,
            kite::TokenType::Eof}),
        "array punctuation");
    all_passed &= check(
        has_types(tokenize("let x = 42\nprint(x)"), {kite::TokenType::Let, kite::TokenType::Identifier,
            kite::TokenType::Equal, kite::TokenType::Integer, kite::TokenType::Identifier,
            kite::TokenType::LeftParen, kite::TokenType::Identifier, kite::TokenType::RightParen,
            kite::TokenType::Eof}),
        "multiple statements");
    all_passed &= check(tokenize("@")[0].type == kite::TokenType::Invalid, "unknown character");
    all_passed &= check(tokenize("").size() == 1 && tokenize("")[0].type == kite::TokenType::Eof, "empty input");
    all_passed &= check(tokenize("let\n x")[1].line == 2, "line tracking");
    all_passed &= check(tokenize("\"unfinished")[0].type == kite::TokenType::Invalid, "unterminated string");

    if (all_passed) {
        std::cout << "\nAll lexer tests passed.\n";
        return 0;
    }

    std::cout << "\nLexer tests failed.\n";
    return 1;
}