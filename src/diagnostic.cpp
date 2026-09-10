#include "kite/diagnostic.hpp"

#include <iostream>
#include <sstream>

namespace kite {

namespace {

std::string source_line(const std::string& source, std::size_t line) {
    std::size_t start = 0;
    for (std::size_t current = 1; current < line && start < source.size(); ++current) {
        const std::size_t newline = source.find('\n', start);
        if (newline == std::string::npos) return "";
        start = newline + 1;
    }
    const std::size_t end = source.find('\n', start);
    std::string text = source.substr(start, end == std::string::npos ? std::string::npos : end - start);
    if (!text.empty() && text.back() == '\r') text.pop_back();
    return text;
}

} // namespace

std::string format_diagnostic(const Diagnostic& diagnostic, const std::string& source,
    const std::string& path) {
    std::ostringstream out;
    out << path;
    if (diagnostic.line > 0) out << ':' << diagnostic.line << ':' << diagnostic.column;
    out << ": " << diagnostic.message;

    if (diagnostic.line > 0) {
        const std::string text = source_line(source, diagnostic.line);
        if (!text.empty()) {
            const std::string gutter(std::to_string(diagnostic.line).size(), ' ');
            out << '\n' << ' ' << diagnostic.line << " | " << text;
            out << '\n' << ' ' << gutter << " | ";
            for (std::size_t index = 1; index < diagnostic.column && index <= text.size() + 1; ++index) {
                out << (index <= text.size() && text[index - 1] == '\t' ? '\t' : ' ');
            }
            out << '^';
        }
    }
    return out.str();
}

void print_diagnostics(const std::vector<Diagnostic>& diagnostics, const std::string& source,
    const std::string& path) {
    for (const auto& diagnostic : diagnostics) {
        std::cerr << format_diagnostic(diagnostic, source, path) << '\n';
    }
}

} // namespace kite
