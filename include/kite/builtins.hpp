#pragma once

#include <ostream>
#include <string>
#include <vector>

#include "kite/value.hpp"

namespace kite {

struct BuiltinContext {
    std::ostream& out;
    std::string error;
};

using BuiltinFn = bool (*)(BuiltinContext& context, std::vector<Value>& arguments, Value& result);

// Returns nullptr when `name` is not a built-in.
BuiltinFn find_builtin(const std::string& name);

void set_program_args(std::vector<std::string> args);

} // namespace kite
