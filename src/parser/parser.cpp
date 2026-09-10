#include "kite/parser/parser.hpp"

#include <cctype>
#include <charconv>
#include <cstdlib>
#include <optional>
#include <utility>

namespace kite {

namespace {

std::string decode_string_literal(const std::string& lexeme) {
    std::string value;
    for (std::size_t index = 1; index + 1 < lexeme.size(); ++index) {
        if (lexeme[index] != '\\' || index + 2 >= lexeme.size()) {
            value.push_back(lexeme[index]);
            continue;
        }
        switch (lexeme[++index]) {
        case 'n': value.push_back('\n'); break;
        case 't': value.push_back('\t'); break;
        case 'r': value.push_back('\r'); break;
        case '0': value.push_back('\0'); break;
        case '\\': value.push_back('\\'); break;
        case '"': value.push_back('"'); break;
        default: value.push_back(lexeme[index]); break;
        }
    }
    return value;
}

std::unique_ptr<Expression> make_binary(BinaryOperator op, std::unique_ptr<Expression> left,
    std::unique_ptr<Expression> right) {
    auto binary = std::make_unique<BinaryExpression>();
    binary->operator_type = op;
    binary->line = left->line;
    binary->column = left->column;
    binary->left = std::move(left);
    binary->right = std::move(right);
    return binary;
}

std::unique_ptr<Expression> make_index(std::unique_ptr<Expression> target,
    std::unique_ptr<Expression> position) {
    auto index = std::make_unique<IndexExpression>();
    index->target = std::move(target);
    index->index = std::move(position);
    return index;
}

std::unique_ptr<Expression> make_index(std::unique_ptr<Expression> target, std::int64_t position) {
    return make_index(std::move(target), std::make_unique<IntegerExpression>(position));
}

std::unique_ptr<Expression> make_call(std::string callee, std::unique_ptr<Expression> argument) {
    auto call = std::make_unique<CallExpression>();
    call->callee = std::make_unique<IdentifierExpression>(std::move(callee));
    call->arguments.push_back(std::move(argument));
    return call;
}

bool is_type_name(const std::string& lexeme) {
    return !lexeme.empty() && std::isupper(static_cast<unsigned char>(lexeme[0]));
}

void restamp(Expression& expression, std::size_t line, std::size_t column) {
    expression.line = line;
    expression.column = column;
    switch (expression.kind) {
    case NodeKind::Array:
        for (auto& element : static_cast<ArrayExpression&>(expression).elements) {
            restamp(*element, line, column);
        }
        return;
    case NodeKind::Map:
        for (auto& entry : static_cast<MapExpression&>(expression).entries) {
            restamp(*entry.first, line, column);
            restamp(*entry.second, line, column);
        }
        return;
    case NodeKind::Index: {
        auto& index = static_cast<IndexExpression&>(expression);
        restamp(*index.target, line, column);
        restamp(*index.index, line, column);
        return;
    }
    case NodeKind::Unary:
        restamp(*static_cast<UnaryExpression&>(expression).operand, line, column);
        return;
    case NodeKind::Binary: {
        auto& binary = static_cast<BinaryExpression&>(expression);
        restamp(*binary.left, line, column);
        restamp(*binary.right, line, column);
        return;
    }
    case NodeKind::Call: {
        auto& call = static_cast<CallExpression&>(expression);
        restamp(*call.callee, line, column);
        for (auto& argument : call.arguments) restamp(*argument, line, column);
        return;
    }
    default:
        return;
    }
}

} // namespace

Parser::Parser(Lexer lexer)
    : lexer_(std::move(lexer)), current_(lexer_.next_token()), peek_(lexer_.next_token()) {}

Program Parser::parse_program() {
    Program program;

    while (current_.type != TokenType::Eof && !fatal_) {
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

const std::vector<Diagnostic>& Parser::errors() const {
    return errors_;
}

void Parser::advance() {
    current_ = peek_;
    peek_ = lexer_.next_token();
}

void Parser::report_error(const std::string& message) {
    errors_.push_back({message, current_.line, current_.column});
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
    Nest nest(*this);
    if (nest.too_deep()) {
        report_error("nesting is too deep");
        return nullptr;
    }
    const Token start = current_;
    auto statement = dispatch_statement();
    if (statement != nullptr && statement->line == 0) {
        statement->line = start.line;
        statement->column = start.column;
    }
    return statement;
}

std::unique_ptr<Statement> Parser::dispatch_statement() {
    if (current_.type == TokenType::Let) {
        return parse_let_statement();
    }
    if (current_.type == TokenType::If) {
        return parse_if_statement();
    }
    if (current_.type == TokenType::While) {
        return parse_while_statement();
    }
    if (current_.type == TokenType::For) {
        return parse_for_statement();
    }
    if (current_.type == TokenType::Fn) {
        return parse_function_statement();
    }
    if (current_.type == TokenType::Struct) {
        return parse_struct_statement();
    }
    if (current_.type == TokenType::Return) {
        return parse_return_statement();
    }
    if (current_.type == TokenType::Try) {
        return parse_try_statement();
    }
    if (current_.type == TokenType::Throw) {
        return parse_throw_statement();
    }
    if (current_.type == TokenType::Import) {
        advance();
        if (current_.type != TokenType::String) {
            report_error("expected a quoted path after 'import'");
            return nullptr;
        }
        auto statement = std::make_unique<ImportStatement>();
        statement->path = decode_string_literal(current_.lexeme);
        advance();
        return statement;
    }
    if (current_.type == TokenType::Break) {
        advance();
        return std::make_unique<BreakStatement>();
    }
    if (current_.type == TokenType::Continue) {
        advance();
        return std::make_unique<ContinueStatement>();
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

    if (current_.type == TokenType::Colon) {
        advance();
        if (current_.type != TokenType::Identifier) {
            report_error("expected a type name after ':'");
            return nullptr;
        }
        statement->declared_type = current_.lexeme;
        advance();
    }

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
        if (current_.type == TokenType::If) {
            auto nested = parse_if_statement();
            if (!nested) {
                return nullptr;
            }
            statement->else_branch.push_back(std::move(nested));
        } else {
            if (!expect(TokenType::LeftBrace, "expected '{' after 'else'")) {
                return nullptr;
            }
            statement->else_branch = parse_block();
        }
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

std::unique_ptr<Statement> Parser::parse_for_statement() {
    advance();
    if (!expect(TokenType::LeftParen, "expected '(' after 'for'")) {
        return nullptr;
    }

    if (current_.type == TokenType::Identifier && peek_.type == TokenType::In) {
        const std::string variable = current_.lexeme;
        const std::string cursor = " for" + std::to_string(synthetic_++);
        advance();
        advance();
        auto iterable = parse_expression();
        if (!iterable ||
            !expect(TokenType::RightParen, "expected ')' after for-in iterable") ||
            !expect(TokenType::LeftBrace, "expected '{' after for-in header")) {
            return nullptr;
        }

        auto state = std::make_unique<ArrayExpression>();
        state->elements.push_back(make_call("__iter", std::move(iterable)));
        state->elements.push_back(std::make_unique<IntegerExpression>(0));

        auto initializer = std::make_unique<LetStatement>();
        initializer->name = cursor;
        initializer->initializer = std::move(state);

        auto step = std::make_unique<IndexAssignmentStatement>();
        step->target = std::make_unique<IdentifierExpression>(cursor);
        step->index = std::make_unique<IntegerExpression>(1);
        step->value = make_binary(BinaryOperator::Add,
            make_index(std::make_unique<IdentifierExpression>(cursor), 1),
            std::make_unique<IntegerExpression>(1));

        auto bind = std::make_unique<LetStatement>();
        bind->name = variable;
        bind->initializer = make_index(
            make_index(std::make_unique<IdentifierExpression>(cursor), 0),
            make_index(std::make_unique<IdentifierExpression>(cursor), 1));

        auto statement = std::make_unique<ForStatement>();
        statement->initializer = std::move(initializer);
        statement->condition = make_binary(BinaryOperator::Less,
            make_index(std::make_unique<IdentifierExpression>(cursor), 1),
            make_call("len", make_index(std::make_unique<IdentifierExpression>(cursor), 0)));
        statement->step = std::move(step);
        statement->body.push_back(std::move(bind));
        for (auto& child : parse_block()) statement->body.push_back(std::move(child));
        return statement;
    }

    auto statement = std::make_unique<ForStatement>();

    if (current_.type != TokenType::Semicolon) {
        statement->initializer = parse_statement();
        if (!statement->initializer) {
            return nullptr;
        }
    }
    if (!expect(TokenType::Semicolon, "expected ';' after for initializer")) {
        return nullptr;
    }

    if (current_.type != TokenType::Semicolon) {
        statement->condition = parse_expression();
        if (!statement->condition) {
            return nullptr;
        }
    }
    if (!expect(TokenType::Semicolon, "expected ';' after for condition")) {
        return nullptr;
    }

    if (current_.type != TokenType::RightParen) {
        statement->step = parse_statement();
        if (!statement->step) {
            return nullptr;
        }
    }
    if (!expect(TokenType::RightParen, "expected ')' after for clauses") ||
        !expect(TokenType::LeftBrace, "expected '{' after for clauses")) {
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
            std::string parameter_type;
            if (current_.type == TokenType::Colon) {
                advance();
                if (current_.type != TokenType::Identifier) {
                    report_error("expected a type name after ':'");
                    return nullptr;
                }
                parameter_type = current_.lexeme;
                advance();
            }
            statement->parameter_types.push_back(parameter_type);
            if (current_.type == TokenType::RightParen) {
                break;
            }
            if (!expect(TokenType::Comma, "expected ',' or ')' after parameter")) {
                return nullptr;
            }
        }
    }

    if (!expect(TokenType::RightParen, "expected ')' after parameters")) {
        return nullptr;
    }
    if (current_.type == TokenType::Arrow) {
        advance();
        if (current_.type != TokenType::Identifier) {
            report_error("expected a return type after '->'");
            return nullptr;
        }
        statement->return_type = current_.lexeme;
        advance();
    }
    if (!expect(TokenType::LeftBrace, "expected '{' before function body")) {
        return nullptr;
    }
    statement->body = parse_block();
    return statement;
}

std::unique_ptr<Statement> Parser::parse_struct_statement() {
    advance();
    if (current_.type != TokenType::Identifier) {
        report_error("expected a struct name after 'struct'");
        return nullptr;
    }
    if (!is_type_name(current_.lexeme)) {
        report_error("struct names must start with an uppercase letter");
        return nullptr;
    }
    auto statement = std::make_unique<StructStatement>();
    statement->name = current_.lexeme;
    advance();
    if (!expect(TokenType::LeftBrace, "expected '{' after the struct name")) {
        return nullptr;
    }
    while (current_.type != TokenType::RightBrace) {
        if (current_.type != TokenType::Identifier) {
            report_error("expected a field name in the struct body");
            return nullptr;
        }
        statement->fields.push_back(current_.lexeme);
        advance();
        if (current_.type == TokenType::Comma) {
            advance();
        } else if (current_.type != TokenType::RightBrace) {
            report_error("expected ',' or '}' in the struct body");
            return nullptr;
        }
    }
    advance();
    return statement;
}

std::unique_ptr<Expression> Parser::parse_struct_literal() {
    auto literal = std::make_unique<MapExpression>();
    literal->type_name = current_.lexeme;
    advance();
    advance();
    while (current_.type != TokenType::RightBrace) {
        if (current_.type != TokenType::Identifier) {
            report_error("expected a field name in the struct literal");
            return nullptr;
        }
        auto key = std::make_unique<StringExpression>(current_.lexeme);
        advance();
        if (!expect(TokenType::Colon, "expected ':' after the field name")) {
            return nullptr;
        }
        auto value = parse_expression();
        if (!value) {
            return nullptr;
        }
        literal->entries.emplace_back(std::move(key), std::move(value));
        if (current_.type == TokenType::Comma) {
            advance();
        } else if (current_.type != TokenType::RightBrace) {
            report_error("expected ',' or '}' in the struct literal");
            return nullptr;
        }
    }
    advance();
    return parse_postfix(std::move(literal));
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

std::unique_ptr<Statement> Parser::parse_try_statement() {
    advance();
    if (!expect(TokenType::LeftBrace, "expected '{' after 'try'")) {
        return nullptr;
    }
    auto statement = std::make_unique<TryStatement>();
    statement->try_branch = parse_block();
    if (!expect(TokenType::Catch, "expected 'catch' after the try block") ||
        !expect(TokenType::LeftParen, "expected '(' after 'catch'")) {
        return nullptr;
    }
    if (current_.type != TokenType::Identifier) {
        report_error("expected a name for the caught error");
        return nullptr;
    }
    statement->name = current_.lexeme;
    advance();
    if (!expect(TokenType::RightParen, "expected ')' after the catch name") ||
        !expect(TokenType::LeftBrace, "expected '{' before the catch block")) {
        return nullptr;
    }
    statement->catch_branch = parse_block();
    return statement;
}

std::unique_ptr<Statement> Parser::parse_throw_statement() {
    advance();
    auto statement = std::make_unique<ThrowStatement>();
    statement->value = parse_expression();
    return statement->value ? std::move(statement) : nullptr;
}

std::unique_ptr<Expression> Parser::parse_string_expression(const std::string& lexeme) {
    std::unique_ptr<Expression> result;
    std::string literal;
    const auto flush = [&] {
        if (literal.empty()) return;
        auto piece = std::make_unique<StringExpression>(std::move(literal));
        literal.clear();
        piece->line = current_.line;
        piece->column = current_.column;
        result = result == nullptr
            ? std::move(piece)
            : make_binary(BinaryOperator::Add, std::move(result), std::move(piece));
    };

    for (std::size_t index = 1; index + 1 < lexeme.size(); ++index) {
        const char character = lexeme[index];
        if (character == '\\' && index + 2 < lexeme.size()) {
            switch (lexeme[++index]) {
            case 'n': literal.push_back('\n'); break;
            case 't': literal.push_back('\t'); break;
            case 'r': literal.push_back('\r'); break;
            case '0': literal.push_back('\0'); break;
            case '\\': literal.push_back('\\'); break;
            case '"': literal.push_back('"'); break;
            default: literal.push_back(lexeme[index]); break;
            }
            continue;
        }
        if (character == '$' && index + 1 < lexeme.size() && lexeme[index + 1] == '{') {
            flush();
            index += 2;
            const std::size_t begin = index;
            for (int depth = 1; index + 1 < lexeme.size(); ++index) {
                if (lexeme[index] == '{') ++depth;
                else if (lexeme[index] == '}' && --depth == 0) break;
            }
            Parser sub{Lexer(lexeme.substr(begin, index - begin))};
            auto embedded = sub.parse_expression();
            for (const auto& diagnostic : sub.errors()) errors_.push_back(diagnostic);
            if (embedded == nullptr) {
                report_error("invalid expression in string interpolation");
                return nullptr;
            }
            restamp(*embedded, current_.line, current_.column);
            auto call = std::make_unique<CallExpression>();
            call->callee = std::make_unique<IdentifierExpression>("str");
            call->arguments.push_back(std::move(embedded));
            call->line = current_.line;
            call->column = current_.column;
            result = result == nullptr
                ? std::move(call)
                : make_binary(BinaryOperator::Add, std::move(result), std::move(call));
            continue;
        }
        literal.push_back(character);
    }
    flush();
    return result != nullptr ? std::move(result) : std::make_unique<StringExpression>("");
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

        const auto compound = [&]() -> std::optional<BinaryOperator> {
            switch (current_.type) {
            case TokenType::PlusEqual: return BinaryOperator::Add;
            case TokenType::MinusEqual: return BinaryOperator::Subtract;
            case TokenType::StarEqual: return BinaryOperator::Multiply;
            case TokenType::SlashEqual: return BinaryOperator::Divide;
            case TokenType::PercentEqual: return BinaryOperator::Modulo;
            default: return std::nullopt;
            }
        }();
        if (compound) {
            advance();
            auto right = parse_expression();
            if (!right) {
                return nullptr;
            }
            auto statement = std::make_unique<AssignmentStatement>();
            statement->name = name;
            statement->value = make_binary(*compound, std::make_unique<IdentifierExpression>(name),
                std::move(right));
            return statement;
        }

        std::unique_ptr<Expression> expression = std::make_unique<IdentifierExpression>(name);
        expression = parse_postfix(std::move(expression));
        if (!expression) {
            return nullptr;
        }

        if (expression->kind == NodeKind::Index && current_.type == TokenType::Equal) {
            auto& indexed = static_cast<IndexExpression&>(*expression);
            advance();
            auto right = parse_expression();
            if (!right) {
                return nullptr;
            }
            auto statement = std::make_unique<IndexAssignmentStatement>();
            statement->target = std::move(indexed.target);
            statement->index = std::move(indexed.index);
            statement->value = std::move(right);
            return statement;
        }

        auto statement = std::make_unique<ExpressionStatement>();
        statement->expression = std::move(expression);
        return statement;
    }

    return parse_expression_statement();
}

std::unique_ptr<Expression> Parser::parse_expression() {
    Nest nest(*this);
    if (nest.too_deep()) {
        report_error("expression nesting is too deep");
        return nullptr;
    }
    const Token start = current_;
    auto expression = parse_logical_or();
    if (expression != nullptr && expression->line == 0) {
        expression->line = start.line;
        expression->column = start.column;
    }
    return expression;
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
        expression = make_binary(BinaryOperator::Or, std::move(expression), std::move(right));
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
        expression = make_binary(BinaryOperator::And, std::move(expression), std::move(right));
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
        expression = make_binary(operator_type, std::move(expression), std::move(right));
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
        expression = make_binary(operator_type, std::move(expression), std::move(right));
    }

    return expression;
}

std::unique_ptr<Expression> Parser::parse_multiplicative() {
    auto expression = parse_unary();
    if (!expression) {
        return nullptr;
    }

    while (current_.type == TokenType::Star || current_.type == TokenType::Slash ||
        current_.type == TokenType::Percent) {
        const BinaryOperator operator_type = current_.type == TokenType::Star
            ? BinaryOperator::Multiply
            : current_.type == TokenType::Slash ? BinaryOperator::Divide : BinaryOperator::Modulo;
        advance();
        auto right = parse_unary();
        if (!right) {
            return nullptr;
        }
        expression = make_binary(operator_type, std::move(expression), std::move(right));
    }

    return expression;
}

std::unique_ptr<Expression> Parser::parse_unary() {
    Nest nest(*this);
    if (nest.too_deep()) {
        report_error("expression nesting is too deep");
        return nullptr;
    }
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
    const Token start = current_;
    auto expression = parse_primary_inner();
    if (expression != nullptr && expression->line == 0) {
        expression->line = start.line;
        expression->column = start.column;
    }
    return expression;
}

std::unique_ptr<Expression> Parser::parse_primary_inner() {
    switch (current_.type) {
    case TokenType::Identifier: {
        if (peek_.type == TokenType::LeftBrace && is_type_name(current_.lexeme)) {
            return parse_struct_literal();
        }
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
        auto expression = parse_string_expression(current_.lexeme);
        advance();
        return expression;
    }
    case TokenType::LeftBracket:
        return parse_postfix(parse_array());
    case TokenType::LeftBrace:
        return parse_postfix(parse_map());
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
    while (current_.type == TokenType::LeftParen || current_.type == TokenType::LeftBracket ||
        current_.type == TokenType::Dot) {
        if (current_.type == TokenType::LeftParen) {
            expression = parse_call(std::move(expression));
            if (!expression) {
                return nullptr;
            }
            continue;
        }

        if (current_.type == TokenType::Dot) {
            advance();
            if (current_.type != TokenType::Identifier) {
                report_error("expected a field name after '.'");
                return nullptr;
            }
            auto field = std::make_unique<IndexExpression>();
            field->target = std::move(expression);
            field->index = std::make_unique<StringExpression>(current_.lexeme);
            advance();
            expression = std::move(field);
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
    while (current_.type != TokenType::RightBrace && current_.type != TokenType::Eof && !fatal_) {
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