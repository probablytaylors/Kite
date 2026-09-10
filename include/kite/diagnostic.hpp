#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace kite {

struct Diagnostic {
    std::string message;
    std::size_t line = 0;
    std::size_t column = 0;
};

std::string format_diagnostic(const Diagnostic& diagnostic, const std::string& source,
    const std::string& path);

void print_diagnostics(const std::vector<Diagnostic>& diagnostics, const std::string& source,
    const std::string& path);

} // namespace kite
