#include "kite/token.hpp"

namespace kite {

const char* token_type_name(TokenType type) {
    switch (type) {
    case TokenType::Eof:
        return "EOF";
    case TokenType::Let:
        return "LET";
    case TokenType::If:
        return "IF";
    case TokenType::Else:
        return "ELSE";
    case TokenType::While:
        return "WHILE";
    case TokenType::Fn:
        return "FN";
    case TokenType::Return:
        return "RETURN";
    case TokenType::True:
        return "TRUE";
    case TokenType::False:
        return "FALSE";
    case TokenType::Identifier:
        return "IDENTIFIER";
    case TokenType::Integer:
        return "INTEGER";
    case TokenType::Float:
        return "FLOAT";
    case TokenType::String:
        return "STRING";
    case TokenType::Equal:
        return "EQUAL";
    case TokenType::EqualEqual:
        return "EQUAL_EQUAL";
    case TokenType::BangEqual:
        return "BANG_EQUAL";
    case TokenType::Bang:
        return "BANG";
    case TokenType::AndAnd:
        return "AND_AND";
    case TokenType::OrOr:
        return "OR_OR";
    case TokenType::Less:
        return "LESS";
    case TokenType::LessEqual:
        return "LESS_EQUAL";
    case TokenType::Greater:
        return "GREATER";
    case TokenType::GreaterEqual:
        return "GREATER_EQUAL";
    case TokenType::Plus:
        return "PLUS";
    case TokenType::Minus:
        return "MINUS";
    case TokenType::Star:
        return "STAR";
    case TokenType::Slash:
        return "SLASH";
    case TokenType::LeftParen:
        return "LEFT_PAREN";
    case TokenType::RightParen:
        return "RIGHT_PAREN";
    case TokenType::LeftBrace:
        return "LEFT_BRACE";
    case TokenType::RightBrace:
        return "RIGHT_BRACE";
    case TokenType::LeftBracket:
        return "LEFT_BRACKET";
    case TokenType::RightBracket:
        return "RIGHT_BRACKET";
    case TokenType::Colon:
        return "COLON";
    case TokenType::Comma:
        return "COMMA";
    case TokenType::Invalid:
        return "INVALID";
    }

    return "UNKNOWN";
}

} // namespace kite