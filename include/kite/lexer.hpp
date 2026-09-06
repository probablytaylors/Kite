#pragma once

#include <cstddef>
#include <string>

#include "kite/token.hpp"

namespace kite {

class Lexer {
public:
	explicit Lexer(std::string source);

	Token next_token();

private:
	char current() const;
	char peek(std::size_t offset) const;
	char advance();
	void skip_whitespace();
	Token make_token(TokenType type, std::size_t start, std::size_t line, std::size_t column) const;
	Token scan_identifier_or_keyword();
	Token scan_number();
	Token scan_string();

	std::string source_;
	std::size_t position_ = 0;
	std::size_t line_ = 1;
	std::size_t column_ = 1;
};

} // namespace kite
