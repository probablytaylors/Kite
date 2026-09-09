#pragma once

#include <string>

#include "kite/ast/ast.hpp"

namespace kite {

bool emit_c(const Program& program, std::string& c_source, std::string& error);
bool compile_native(const Program& program, const std::string& output_path, std::string& error);

} // namespace kite
