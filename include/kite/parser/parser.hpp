#pragma once

#include <string>
#include <vector>

#include "kite/ast/ast.hpp"
#include "kite/lexer.hpp"

namespace kite {

class Parser {
public:
    explicit Parser(Lexer lexer);

    Program parse_program();
    const std::vector<std::string>& errors() const;

private:
    struct Nest {
        explicit Nest(Parser& parser) : parser_(parser) { ++parser_.depth_; }
        ~Nest() { --parser_.depth_; }
        bool too_deep() const {
            if (parser_.depth_ > 500) parser_.fatal_ = true;
            return parser_.fatal_;
        }
        Parser& parser_;
    };

    void advance();
    void report_error(const std::string& message);
    bool expect(TokenType type, const std::string& message);
    std::unique_ptr<Statement> parse_statement();
    std::unique_ptr<Statement> parse_let_statement();
    std::unique_ptr<Statement> parse_assignment_or_expression_statement();
    std::unique_ptr<Statement> parse_if_statement();
    std::unique_ptr<Statement> parse_while_statement();
    std::unique_ptr<Statement> parse_for_statement();
    std::unique_ptr<Statement> parse_function_statement();
    std::unique_ptr<Statement> parse_struct_statement();
    std::unique_ptr<Expression> parse_struct_literal();
    std::unique_ptr<Statement> parse_return_statement();
    std::unique_ptr<Statement> parse_try_statement();
    std::unique_ptr<Statement> parse_throw_statement();
    std::unique_ptr<Statement> parse_expression_statement();
    std::unique_ptr<Expression> parse_expression();
    std::unique_ptr<Expression> parse_logical_or();
    std::unique_ptr<Expression> parse_logical_and();
    std::unique_ptr<Expression> parse_comparison();
    std::unique_ptr<Expression> parse_additive();
    std::unique_ptr<Expression> parse_multiplicative();
    std::unique_ptr<Expression> parse_unary();
    std::unique_ptr<Expression> parse_primary();
    std::unique_ptr<Expression> parse_call(std::unique_ptr<Expression> callee);
    std::unique_ptr<Expression> parse_postfix(std::unique_ptr<Expression> expression);
    std::unique_ptr<Expression> parse_array();
    std::unique_ptr<Expression> parse_map();
    std::vector<std::unique_ptr<Statement>> parse_block();

    Lexer lexer_;
    Token current_;
    Token peek_;
    int synthetic_ = 0;
    int depth_ = 0;
    bool fatal_ = false;
    std::vector<std::string> errors_;
};

} // namespace kite