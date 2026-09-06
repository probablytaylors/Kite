#include "kite/parser/parser.hpp"

#include <charconv>
#include <cstdlib>
#include <utility>

namespace kite {

Parser::Parser(Lexer lexer) : lexer_(std::move(lexer)), current_(lexer_.next_token()) {}

Program Parser::parse_program() {
    Program program;

    while (current_.type != TokenType::Eof) {
        if (current_.type == TokenType::Invalid) {
            report_error("invalid token");
            advance();
            continue;
        }

        if (auto statement = parse_statement()) {
            program.statements.push_back(std::move(statement));
        } else if (current_.type != TokenType::Eof) {
            advance();
        }
    }

    return program;
}

const std::vector<std::string>& Parser::errors() const {
    return errors_;
}

void Parser::advance() {
    current_ = lexer_.next_token();
}

void Parser::report_error(const std::string& message) {
    errors_.push_back(
        "line " + std::to_string(current_.line) + ", column " +
        std::to_string(current_.column) + ": " + message);
}

bool Parser::expect(TokenType type, const std::string& message) {
    if (current_.type != type) {
        report_error(message);
        return false;
    }

    advance();
    return true;
}

std::unique_ptr<Statement> Parser::parse_statement() {
    if (current_.type == TokenType::Let) {
        return parse_let_statement();
    }
    if (current_.type == TokenType::If) {
        return parse_if_statement();
    }
    if (current_.type == TokenType::While) {
        return parse_while_statement();
    }
    if (current_.type == TokenType::Fn) {
        return parse_function_statement();
    }
    if (current_.type == TokenType::Return) {
        return parse_return_statement();
    }

    return parse_assignment_or_expression_statement();
}

std::unique_ptr<Statement> Parser::parse_let_statement() {
    advance();

    if (current_.type != TokenType::Identifier) {
        report_error("expected an identifier after 'let'");
        return nullptr;
    }

    auto statement = std::make_unique<LetStatement>();
    statement->name = current_.lexeme;
    advance();

    if (!expect(TokenType::Equal, "expected '=' after let name")) {
        return nullptr;
    }

    statement->initializer = parse_expression();
    if (!statement->initializer) {
        return nullptr;
    }

    return statement;
}

std::unique_ptr<Statement> Parser::parse_if_statement() {
    advance();
    if (!expect(TokenType::LeftParen, "expected '(' after 'if'")) {
        return nullptr;
    }

    auto statement = std::make_unique<IfStatement>();
    statement->condition = parse_expression();
    if (!statement->condition || !expect(TokenType::RightParen, "expected ')' after if condition")) {
        return nullptr;
    }
    if (!expect(TokenType::LeftBrace, "expected '{' after if condition")) {
        return nullptr;
    }

    statement->then_branch = parse_block();
    if (current_.type == TokenType::Else) {
        advance();
        if (!expect(TokenType::LeftBrace, "expected '{' after 'else'")) {
            return nullptr;
        }
        statement->else_branch = parse_block();
    }

    return statement;
}

std::unique_ptr<Statement> Parser::parse_while_statement() {
    advance();
    if (!expect(TokenType::LeftParen, "expected '(' after 'while'")) {
        return nullptr;
    }

    auto statement = std::make_unique<WhileStatement>();
    statement->condition = parse_expression();
    if (!statement->condition || !expect(TokenType::RightParen, "expected ')' after while condition")) {
        return nullptr;
    }
    if (!expect(TokenType::LeftBrace, "expected '{' after while condition")) {
        return nullptr;
    }

    statement->body = parse_block();
    return statement;
}

std::unique_ptr<Statement> Parser::parse_function_statement() {
    advance();
    if (current_.type != TokenType::Identifier) {
        report_error("expected a function name after 'fn'");
        return nullptr;
    }

    auto statement = std::make_unique<FunctionStatement>();
    statement->name = current_.lexeme;
    advance();
    if (!expect(TokenType::LeftParen, "expected '(' after function name")) {
        return nullptr;
    }

    if (current_.type != TokenType::RightParen) {
        while (true) {
            if (current_.type != TokenType::Identifier) {
                report_error("expected parameter name");
                return nullptr;
            }
            statement->parameters.push_back(current_.lexeme);
            advance();
            if (current_.type == TokenType::RightParen) {
                break;
            }
            if (!expect(TokenType::Comma, "expected ',' or ')' after parameter")) {
                return nullptr;
            }
        }
    }

    if (!expect(TokenType::RightParen, "expected ')' after parameters") ||
        !expect(TokenType::LeftBrace, "expected '{' before function body")) {
        return nullptr;
    }
    statement->body = parse_block();
    return statement;
}

std::unique_ptr<Statement> Parser::parse_return_statement() {
    advance();
    auto statement = std::make_unique<ReturnStatement>();
    statement->value = parse_expression();
    if (!statement->value) {
        return nullptr;
    }
    return statement;
}

std::unique_ptr<Statement> Parser::parse_expression_statement() {
    auto statement = std::make_unique<ExpressionStatement>();
    statement->expression = parse_expression();
    if (!statement->expression) {
        return nullptr;
    }

    return statement;
}

std::unique_ptr<Statement> Parser::parse_assignment_or_expression_statement() {
    if (current_.type == TokenType::Identifier) {
        const std::string name = current_.lexeme;
        advance();
        if (current_.type == TokenType::Equal) {
            advance();
            auto statement = std::make_unique<AssignmentStatement>();
            statement->name = name;
            statement->value = parse_expression();
            return statement->value ? std::move(statement) : nullptr;
        }

        std::unique_ptr<Expression> expression = std::make_unique<IdentifierExpression>(name);
        expression = parse_postfix(std::move(expression));
        auto statement = std::make_unique<ExpressionStatement>();
        statement->expression = std::move(expression);
        return statement;
    }

    return parse_expression_statement();
}

std::unique_ptr<Expression> Parser::parse_expression() {
    return parse_logical_or();
}

std::unique_ptr<Expression> Parser::parse_logical_or() {
    auto expression = parse_logical_and();
    if (!expression) {
        return nullptr;
    }
    while (current_.type == TokenType::OrOr) {
        advance();
        auto right = parse_logical_and();
        if (!right) {
            return nullptr;
        }
        auto binary = std::make_unique<BinaryExpression>();
        binary->operator_type = BinaryOperator::Or;
        binary->left = std::move(expression);
        binary->right = std::move(right);
        expression = std::move(binary);
    }
    return expression;
}

std::unique_ptr<Expression> Parser::parse_logical_and() {
    auto expression = parse_comparison();
    if (!expression) {
        return nullptr;
    }
    while (current_.type == TokenType::AndAnd) {
        advance();
        auto right = parse_comparison();
        if (!right) {
            return nullptr;
        }
        auto binary = std::make_unique<BinaryExpression>();
        binary->operator_type = BinaryOperator::And;
        binary->left = std::move(expression);
        binary->right = std::move(right);
        expression = std::move(binary);
    }
    return expression;
}

std::unique_ptr<Expression> Parser::parse_comparison() {
    auto expression = parse_additive();
    if (!expression) {
        return nullptr;
    }

    while (current_.type == TokenType::EqualEqual || current_.type == TokenType::BangEqual ||
        current_.type == TokenType::Less || current_.type == TokenType::LessEqual ||
        current_.type == TokenType::Greater || current_.type == TokenType::GreaterEqual) {
        BinaryOperator operator_type;
        switch (current_.type) {
        case TokenType::EqualEqual:
            operator_type = BinaryOperator::Equal;
            break;
        case TokenType::BangEqual:
            operator_type = BinaryOperator::NotEqual;
            break;
        case TokenType::Less:
            operator_type = BinaryOperator::Less;
            break;
        case TokenType::LessEqual:
            operator_type = BinaryOperator::LessEqual;
            break;
        case TokenType::Greater:
            operator_type = BinaryOperator::Greater;
            break;
        case TokenType::GreaterEqual:
            operator_type = BinaryOperator::GreaterEqual;
            break;
        default:
            return nullptr;
        }
        advance();
        auto right = parse_additive();
        if (!right) {
            return nullptr;
        }

        auto binary = std::make_unique<BinaryExpression>();
        binary->operator_type = operator_type;
        binary->left = std::move(expression);
        binary->right = std::move(right);
        expression = std::move(binary);
    }

    return expression;
}

std::unique_ptr<Expression> Parser::parse_additive() {
    auto expression = parse_multiplicative();
    if (!expression) {
        return nullptr;
    }

    while (current_.type == TokenType::Plus || current_.type == TokenType::Minus) {
        const BinaryOperator operator_type = current_.type == TokenType::Plus
            ? BinaryOperator::Add
            : BinaryOperator::Subtract;
        advance();
        auto right = parse_multiplicative();
        if (!right) {
            return nullptr;
        }

        auto binary = std::make_unique<BinaryExpression>();
        binary->operator_type = operator_type;
        binary->left = std::move(expression);
        binary->right = std::move(right);
        expression = std::move(binary);
    }

    return expression;
}

std::unique_ptr<Expression> Parser::parse_multiplicative() {
    auto expression = parse_unary();
    if (!expression) {
        return nullptr;
    }

    while (current_.type == TokenType::Star || current_.type == TokenType::Slash) {
        const BinaryOperator operator_type = current_.type == TokenType::Star
            ? BinaryOperator::Multiply
            : BinaryOperator::Divide;
        advance();
        auto right = parse_unary();
        if (!right) {
            return nullptr;
        }

        auto binary = std::make_unique<BinaryExpression>();
        binary->operator_type = operator_type;
        binary->left = std::move(expression);
        binary->right = std::move(right);
        expression = std::move(binary);
    }

    return expression;
}

std::unique_ptr<Expression> Parser::parse_unary() {
    if (current_.type == TokenType::Minus || current_.type == TokenType::Bang) {
        const UnaryOperator operator_type = current_.type == TokenType::Minus
            ? UnaryOperator::Negate
            : UnaryOperator::Not;
        advance();
        auto expression = parse_unary();
        if (!expression) {
            return nullptr;
        }
        auto unary = std::make_unique<UnaryExpression>();
        unary->operator_type = operator_type;
        unary->operand = std::move(expression);
        return unary;
    }
    return parse_primary();
}

std::unique_ptr<Expression> Parser::parse_primary() {
    switch (current_.type) {
    case TokenType::Identifier: {
        auto expression = std::make_unique<IdentifierExpression>(current_.lexeme);
        advance();
        return parse_postfix(std::move(expression));
    }
    case TokenType::Integer: {
        std::int64_t value = 0;
        const auto* begin = current_.lexeme.data();
        const auto* end = begin + current_.lexeme.size();
        const auto result = std::from_chars(begin, end, value);
        if (result.ec != std::errc() || result.ptr != end) {
            report_error("invalid integer literal");
            advance();
            return nullptr;
        }

        advance();
        return std::make_unique<IntegerExpression>(value);
    }
    case TokenType::True:
        advance();
        return std::make_unique<BooleanExpression>(true);
    case TokenType::False:
        advance();
        return std::make_unique<BooleanExpression>(false);
    case TokenType::Float: {
        char* end = nullptr;
        const double value = std::strtod(current_.lexeme.c_str(), &end);
        if (end == current_.lexeme.c_str() || *end != '\0') {
            report_error("invalid floating-point literal");
            advance();
            return nullptr;
        }

        advance();
        return std::make_unique<FloatExpression>(value);
    }
    case TokenType::String: {
        const std::string value = current_.lexeme.substr(1, current_.lexeme.size() - 2);
        advance();
        return std::make_unique<StringExpression>(value);
    }
    case TokenType::LeftBracket:
        return parse_array();
    case TokenType::LeftBrace:
        return parse_map();
    case TokenType::LeftParen: {
        advance();
        auto expression = parse_expression();
        if (!expression) {
            return nullptr;
        }

        if (!expect(TokenType::RightParen, "expected ')' after expression")) {
            return nullptr;
        }
        return parse_postfix(std::move(expression));
    }
    default:
        report_error("expected an expression");
        return nullptr;
    }
}

std::unique_ptr<Expression> Parser::parse_postfix(std::unique_ptr<Expression> expression) {
    while (current_.type == TokenType::LeftParen || current_.type == TokenType::LeftBracket) {
        if (current_.type == TokenType::LeftParen) {
            expression = parse_call(std::move(expression));
            if (!expression) {
                return nullptr;
            }
            continue;
        }

        advance();
        auto index = parse_expression();
        if (!index || !expect(TokenType::RightBracket, "expected ']' after index")) {
            return nullptr;
        }
        auto indexed = std::make_unique<IndexExpression>();
        indexed->target = std::move(expression);
        indexed->index = std::move(index);
        expression = std::move(indexed);
    }
    return expression;
}

std::unique_ptr<Expression> Parser::parse_array() {
    advance();
    auto array = std::make_unique<ArrayExpression>();
    if (current_.type == TokenType::RightBracket) {
        advance();
        return array;
    }

    while (true) {
        auto element = parse_expression();
        if (!element) {
            return nullptr;
        }
        array->elements.push_back(std::move(element));
        if (current_.type == TokenType::RightBracket) {
            advance();
            return array;
        }
        if (!expect(TokenType::Comma, "expected ',' or ']' after array element")) {
            return nullptr;
        }
    }
}

std::unique_ptr<Expression> Parser::parse_map() {
    advance();
    auto map = std::make_unique<MapExpression>();
    if (current_.type == TokenType::RightBrace) {
        advance();
        return map;
    }

    while (true) {
        auto key = parse_expression();
        if (!key || !expect(TokenType::Colon, "expected ':' after map key")) {
            return nullptr;
        }
        auto value = parse_expression();
        if (!value) {
            return nullptr;
        }
        map->entries.emplace_back(std::move(key), std::move(value));
        if (current_.type == TokenType::RightBrace) {
            advance();
            return map;
        }
        if (!expect(TokenType::Comma, "expected ',' or '}' after map entry")) {
            return nullptr;
        }
    }
}

std::vector<std::unique_ptr<Statement>> Parser::parse_block() {
    std::vector<std::unique_ptr<Statement>> statements;
    while (current_.type != TokenType::RightBrace && current_.type != TokenType::Eof) {
        if (auto statement = parse_statement()) {
            statements.push_back(std::move(statement));
        } else if (current_.type != TokenType::Eof && current_.type != TokenType::RightBrace) {
            advance();
        }
    }

    if (current_.type == TokenType::Eof) {
        report_error("expected '}' to close block");
    } else {
        advance();
    }
    return statements;
}

std::unique_ptr<Expression> Parser::parse_call(std::unique_ptr<Expression> callee) {
    advance();
    auto call = std::make_unique<CallExpression>();
    call->callee = std::move(callee);

    if (current_.type == TokenType::RightParen) {
        advance();
        return call;
    }

    while (true) {
        if (current_.type == TokenType::Eof) {
            report_error("expected ')' to close call");
            return nullptr;
        }

        auto argument = parse_expression();
        if (!argument) {
            return nullptr;
        }
        call->arguments.push_back(std::move(argument));

        if (current_.type == TokenType::RightParen) {
            advance();
            return call;
        }

        if (current_.type == TokenType::Eof) {
            report_error("expected ')' to close call");
            return nullptr;
        }

        if (!expect(TokenType::Comma, "expected ',' or ')' after call argument")) {
            return nullptr;
        }
    }
}

} // namespace kite