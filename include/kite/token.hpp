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
    For,
    In,
    Fn,
    Return,
    Break,
    Continue,
    Try,
    Catch,
    Throw,
    Import,
    Struct,
    True,
    False,
    Identifier,
    Integer,
    Float,
    String,
    Equal,
    PlusEqual,
    MinusEqual,
    StarEqual,
    SlashEqual,
    PercentEqual,
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
    Percent,
    Arrow,
    Dot,
    LeftParen,
    RightParen,
    LeftBrace,
    RightBrace,
    LeftBracket,
    RightBracket,
    Colon,
    Semicolon,
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