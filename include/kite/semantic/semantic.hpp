#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "kite/ast/ast.hpp"

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
    const std::vector<std::string>& errors() const;

private:
    SemanticType analyze_expression(const Expression& expression);
    void analyze_statement(const Statement& statement);
    void analyze_block(const std::vector<std::unique_ptr<Statement>>& statements);
    void report_error(const std::string& message);
    bool is_numeric(SemanticType type) const;
    bool is_condition(SemanticType type) const;
    bool is_assignable(SemanticType expected, SemanticType actual) const;

    std::vector<std::unordered_map<std::string, SemanticType>> scopes_;
    std::unordered_map<std::string, std::vector<SemanticType>> functions_;
    std::unordered_map<std::string, std::vector<std::string>> structs_;
    std::vector<std::string> errors_;
    bool in_function_ = false;
    int loop_depth_ = 0;
};

} // namespace kite
