#pragma once

#include <string>
#include <vector>

#include "kite/ast/ast.hpp"

namespace kite {

bool load_program(const std::string& entry_path, Program& program, std::vector<std::string>& errors);

} // namespace kite
