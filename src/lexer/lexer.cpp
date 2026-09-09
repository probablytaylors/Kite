#include "kite/lexer.hpp"

#include <cctype>
#include <utility>

namespace kite {

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

char Lexer::current() const {
    return position_ < source_.size() ? source_[position_] : '\0';
}

char Lexer::peek(std::size_t offset) const {
    const std::size_t index = position_ + offset;
    return index < source_.size() ? source_[index] : '\0';
}

char Lexer::advance() {
    const char character = current();
    if (character == '\0') {
        return character;
    }

    ++position_;
    if (character == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }

    return character;
}

void Lexer::skip_whitespace() {
    while (true) {
        if (std::isspace(static_cast<unsigned char>(current()))) {
            advance();
        } else if (current() == '#') {
            while (current() != '\n' && current() != '\0') advance();
        } else {
            return;
        }
    }
}

Token Lexer::make_token(TokenType type, std::size_t start, std::size_t line, std::size_t column) const {
    return {type, source_.substr(start, position_ - start), line, column};
}

Token Lexer::scan_identifier_or_keyword() {
    const std::size_t start = position_;
    const std::size_t line = line_;
    const std::size_t column = column_;

    while (std::isalnum(static_cast<unsigned char>(current())) || current() == '_') {
        advance();
    }

    const std::string lexeme = source_.substr(start, position_ - start);
    TokenType type = TokenType::Identifier;
    if (lexeme == "let") {
        type = TokenType::Let;
    } else if (lexeme == "if") {
        type = TokenType::If;
    } else if (lexeme == "else") {
        type = TokenType::Else;
    } else if (lexeme == "while") {
        type = TokenType::While;
    } else if (lexeme == "for") {
        type = TokenType::For;
    } else if (lexeme == "fn") {
        type = TokenType::Fn;
    } else if (lexeme == "return") {
        type = TokenType::Return;
    } else if (lexeme == "break") {
        type = TokenType::Break;
    } else if (lexeme == "continue") {
        type = TokenType::Continue;
    } else if (lexeme == "true") {
        type = TokenType::True;
    } else if (lexeme == "false") {
        type = TokenType::False;
    }
    return make_token(type, start, line, column);
}

Token Lexer::scan_number() {
    const std::size_t start = position_;
    const std::size_t line = line_;
    const std::size_t column = column_;

    while (std::isdigit(static_cast<unsigned char>(current()))) {
        advance();
    }

    TokenType type = TokenType::Integer;
    if (current() == '.' && std::isdigit(static_cast<unsigned char>(peek(1)))) {
        type = TokenType::Float;
        advance();
        while (std::isdigit(static_cast<unsigned char>(current()))) {
            advance();
        }
    }

    return make_token(type, start, line, column);
}

Token Lexer::scan_string() {
    const std::size_t start = position_;
    const std::size_t line = line_;
    const std::size_t column = column_;
    advance();

    while (current() != '"' && current() != '\n' && current() != '\0') {
        if (current() == '\\' && peek(1) != '\0' && peek(1) != '\n') {
            advance();
        }
        advance();
    }

    if (current() != '"') {
        return make_token(TokenType::Invalid, start, line, column);
    }

    advance();
    return make_token(TokenType::String, start, line, column);
}

Token Lexer::next_token() {
	skip_whitespace();

    const std::size_t start = position_;
    const std::size_t line = line_;
    const std::size_t column = column_;

    if (current() == '\0') {
        return {TokenType::Eof, "", line, column};
    }

    if (std::isalpha(static_cast<unsigned char>(current())) || current() == '_') {
        return scan_identifier_or_keyword();
    }

    if (std::isdigit(static_cast<unsigned char>(current()))) {
        return scan_number();
    }

    if (current() == '"') {
        return scan_string();
    }

    const char character = advance();
    switch (character) {
    case '=':
        if (current() == '=') {
            advance();
            return make_token(TokenType::EqualEqual, start, line, column);
        }
        return make_token(TokenType::Equal, start, line, column);
    case '!':
        if (current() == '=') {
            advance();
            return make_token(TokenType::BangEqual, start, line, column);
        }
        return make_token(TokenType::Bang, start, line, column);
    case '&':
        if (current() == '&') {
            advance();
            return make_token(TokenType::AndAnd, start, line, column);
        }
        return make_token(TokenType::Invalid, start, line, column);
    case '|':
        if (current() == '|') {
            advance();
            return make_token(TokenType::OrOr, start, line, column);
        }
        return make_token(TokenType::Invalid, start, line, column);
    case '+':
        if (current() == '=') {
            advance();
            return make_token(TokenType::PlusEqual, start, line, column);
        }
        return make_token(TokenType::Plus, start, line, column);
    case '-':
        if (current() == '=') {
            advance();
            return make_token(TokenType::MinusEqual, start, line, column);
        }
        if (current() == '>') {
            advance();
            return make_token(TokenType::Arrow, start, line, column);
        }
        return make_token(TokenType::Minus, start, line, column);
    case '*':
        if (current() == '=') {
            advance();
            return make_token(TokenType::StarEqual, start, line, column);
        }
        return make_token(TokenType::Star, start, line, column);
    case '/':
        if (current() == '=') {
            advance();
            return make_token(TokenType::SlashEqual, start, line, column);
        }
        return make_token(TokenType::Slash, start, line, column);
    case '%':
        if (current() == '=') {
            advance();
            return make_token(TokenType::PercentEqual, start, line, column);
        }
        return make_token(TokenType::Percent, start, line, column);
    case '<':
        if (current() == '=') {
            advance();
            return make_token(TokenType::LessEqual, start, line, column);
        }
        return make_token(TokenType::Less, start, line, column);
    case '>':
        if (current() == '=') {
            advance();
            return make_token(TokenType::GreaterEqual, start, line, column);
        }
        return make_token(TokenType::Greater, start, line, column);
    case '{':
        return make_token(TokenType::LeftBrace, start, line, column);
    case '}':
        return make_token(TokenType::RightBrace, start, line, column);
    case '[':
        return make_token(TokenType::LeftBracket, start, line, column);
    case ']':
        return make_token(TokenType::RightBracket, start, line, column);
    case ':':
        return make_token(TokenType::Colon, start, line, column);
    case ';':
        return make_token(TokenType::Semicolon, start, line, column);
    case '(':
        return make_token(TokenType::LeftParen, start, line, column);
    case ')':
        return make_token(TokenType::RightParen, start, line, column);
    case ',':
        return make_token(TokenType::Comma, start, line, column);
    default:
        return make_token(TokenType::Invalid, start, line, column);
    }
}

} // namespace kite
