#pragma once

#include <cstddef>
#include <string>

namespace kite {

enum class TokenType {
    Eof,
    Let,
    If,
    Else,
    While,
    Fn,
    Return,
    True,
    False,
    Identifier,
    Integer,
    Float,
    String,
    Equal,
    EqualEqual,
    BangEqual,
    Bang,
    AndAnd,
    OrOr,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    Plus,
    Minus,
    Star,
    Slash,
    LeftParen,
    RightParen,
    LeftBrace,
    RightBrace,
    LeftBracket,
    RightBracket,
    Colon,
    Comma,
    Invalid
};

struct Token {
    TokenType type;
    std::string lexeme;
    std::size_t line;
    std::size_t column;
};

const char* token_type_name(TokenType type);

} // namespace kite