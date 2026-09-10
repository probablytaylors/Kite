#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "kite/ast/ast.hpp"
#include "kite/diagnostic.hpp"

namespace kite {

enum class SemanticType {
    Unknown,
    Boolean,
    Integer,
    Float,
    String,
    Array,
    Map,
    Function,
    Empty
};

const char* semantic_type_name(SemanticType type);

class SemanticAnalyzer {
public:
    bool analyze(const Program& program);
    const std::vector<Diagnostic>& errors() const;

private:
    SemanticType analyze_expression(const Expression& expression);
    SemanticType analyze_binary(const BinaryExpression& binary);
    SemanticType analyze_call(const CallExpression& call);
    void analyze_statement(const Statement& statement);
    void analyze_block(const std::vector<std::unique_ptr<Statement>>& statements);
    void analyze_struct_literal(const MapExpression& literal);
    SemanticType* lookup(const std::string& name);
    void report_error(const std::string& message);
    void at(const Statement& node);
    void at(const Expression& node);
    bool is_numeric(SemanticType type) const;
    bool is_condition(SemanticType type) const;
    bool is_assignable(SemanticType expected, SemanticType actual) const;

    std::vector<std::unordered_map<std::string, SemanticType>> scopes_;
    std::unordered_map<std::string, std::vector<SemanticType>> functions_;
    std::unordered_map<std::string, std::vector<std::string>> structs_;
    std::vector<Diagnostic> errors_;
    std::size_t error_line_ = 0;
    std::size_t error_column_ = 0;
    bool in_function_ = false;
    int loop_depth_ = 0;
};

} // namespace kite
