#include "kite/module/module.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <system_error>
#include <utility>

#include "kite/parser/parser.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace kite {

namespace {

namespace fs = std::filesystem;

fs::path executable_directory() {
#if defined(_WIN32)
    std::wstring buffer(1024, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) return {};
    buffer.resize(length);
    return fs::path(buffer).parent_path();
#else
    std::error_code error;
    const fs::path self = fs::read_symlink("/proc/self/exe", error);
    return error ? fs::path{} : self.parent_path();
#endif
}

bool read_file_text(const fs::path& path, std::string& out) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    out.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return true;
}

fs::path with_extension(fs::path path) {
    if (path.extension() != ".kite") path += ".kite";
    return path;
}

class Loader {
public:
    std::vector<std::string> errors;

    bool load(const fs::path& path, Program& program, int depth = 0) {
        if (depth > 64) {
            errors.push_back("import chain is too deep");
            return false;
        }
        std::error_code error;
        const fs::path resolved = fs::weakly_canonical(path, error);
        const fs::path file = error ? path : resolved;
        if (!loaded_.insert(file.string()).second) return true;
        if (loaded_.size() > 4096) {
            errors.push_back("too many imported files");
            return false;
        }

        std::string source;
        if (!read_file_text(file, source)) {
            errors.push_back("could not open imported file: " + file.string());
            return false;
        }

        Parser parser{Lexer(std::move(source))};
        Program parsed = parser.parse_program();
        if (!parser.errors().empty()) {
            for (const auto& message : parser.errors()) {
                errors.push_back(file.filename().string() + ": " + message);
            }
            return false;
        }

        const fs::path directory = file.parent_path();
        for (const auto& statement : parsed.statements) {
            if (statement->kind != NodeKind::Import) continue;
            const auto& import = static_cast<const ImportStatement&>(*statement);
            fs::path target;
            if (!resolve(import.path, directory, target)) {
                errors.push_back("could not resolve import \"" + import.path + "\" from " +
                    file.filename().string());
                continue;
            }
            load(target, program, depth + 1);
        }
        for (auto& statement : parsed.statements) {
            if (statement->kind != NodeKind::Import) program.statements.push_back(std::move(statement));
        }
        return errors.empty();
    }

private:
    std::set<std::string> loaded_;

    bool resolve(const std::string& spec, const fs::path& importer_dir, fs::path& out) const {
        const fs::path raw(spec);
        std::vector<fs::path> candidates;
        if (raw.is_absolute()) {
            candidates.push_back(with_extension(raw));
        } else {
            candidates.push_back(with_extension(importer_dir / raw));
            const fs::path exe = executable_directory();
            if (!exe.empty()) {
                candidates.push_back(with_extension(exe / raw));
                candidates.push_back(with_extension(exe.parent_path() / raw));
                candidates.push_back(with_extension(exe.parent_path().parent_path() / raw));
            }
        }
        for (const auto& candidate : candidates) {
            std::error_code error;
            if (fs::is_regular_file(candidate, error)) {
                out = candidate;
                return true;
            }
        }
        return false;
    }
};

} // namespace

bool load_program(const std::string& entry_path, Program& program, std::vector<std::string>& errors) {
    Loader loader;
    const bool ok = loader.load(fs::path(entry_path), program);
    for (auto& message : loader.errors) errors.push_back(std::move(message));
    return ok && errors.empty();
}

} // namespace kite
