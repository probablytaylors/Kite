#include "kite/update.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "kite/version.hpp"

#if defined(_WIN32)
#include <windows.h>
#endif

namespace kite {

namespace {

constexpr const char* kRepoUrl = "https://github.com/probablytaylors/Kite";
constexpr const char* kRawUrl = "https://raw.githubusercontent.com/probablytaylors/Kite";

#if defined(_WIN32)

bool g_style = false;

void enable_console_style() {
    SetConsoleOutputCP(CP_UTF8);
    const HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (handle == INVALID_HANDLE_VALUE || !GetConsoleMode(handle, &mode)) return;
    if (SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) g_style = true;
}

std::string paint(const char* code, const std::string& text) {
    if (!g_style) return text;
    return "\x1b[" + std::string(code) + "m" + text + "\x1b[0m";
}
std::string bold(const std::string& t) { return paint("1", t); }
std::string dim(const std::string& t) { return paint("2", t); }
std::string cyan(const std::string& t) { return paint("36", t); }
std::string green(const std::string& t) { return paint("32", t); }
std::string yellow(const std::string& t) { return paint("33", t); }
std::string red(const std::string& t) { return paint("31", t); }

const char* tick() { return g_style ? "\xe2\x9c\x93" : "ok"; }
const char* warn_mark() { return g_style ? "\xe2\x9a\xa0" : "!"; }
const char* cross() { return g_style ? "\xe2\x9c\x97" : "x"; }
const char* arrow() { return g_style ? "\xe2\x86\x92" : "->"; }

std::string lowercase(std::string text) {
    for (auto& character : text) character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    return text;
}

std::string run_capture(const std::string& command) {
    std::string output;
    std::array<char, 512> buffer{};
    FILE* pipe = _popen(command.c_str(), "r");
    if (pipe == nullptr) return output;
    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }
    _pclose(pipe);
    return output;
}

std::string trim(const std::string& text) {
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return "";
    const auto end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

std::string without_v(const std::string& tag) {
    return (!tag.empty() && (tag[0] == 'v' || tag[0] == 'V')) ? tag.substr(1) : tag;
}

std::string fetch(const std::string& label, const std::string& command) {
    std::cout << "  " << dim(label + "...") << std::endl;
    return run_capture(command);
}

int fail(const std::string& message) {
    std::cerr << "  " << red(std::string(cross()) + ' ' + message) << "\n\n";
    return 1;
}

void print_wrapped(const std::string& text, std::size_t width) {
    for (std::size_t start = 0; start < text.size(); ) {
        std::size_t length = text.size() - start;
        if (length > width) {
            const std::size_t space = text.rfind(' ', start + width);
            length = space > start ? space - start : width;
        }
        std::cout << "  " << text.substr(start, length) << '\n';
        start += length + (start + length < text.size() ? 1 : 0);
    }
}

std::string temp_dir() {
    char buffer[MAX_PATH]{};
    const DWORD length = GetTempPathA(MAX_PATH, buffer);
    if (length == 0 || length > MAX_PATH) return ".";
    std::string path(buffer, length);
    while (!path.empty() && (path.back() == '\\' || path.back() == '/')) path.pop_back();
    if (path.find_first_of("\"|&<>^") != std::string::npos) return ".";
    return path;
}

struct SemVer {
    int major = 0;
    int minor = 0;
    int patch = 0;
};

bool parse_semver(const std::string& text, SemVer& out) {
    std::size_t position = text.find_first_of("0123456789");
    if (position == std::string::npos) return false;
    int* fields[3] = {&out.major, &out.minor, &out.patch};
    for (int index = 0; index < 3; ++index) {
        if (position >= text.size() || !std::isdigit(static_cast<unsigned char>(text[position]))) return false;
        int number = 0;
        while (position < text.size() && std::isdigit(static_cast<unsigned char>(text[position]))) {
            number = number * 10 + (text[position] - '0');
            ++position;
        }
        *fields[index] = number;
        if (index < 2) {
            if (position >= text.size() || text[position] != '.') return false;
            ++position;
        }
    }
    return true;
}

int compare(const SemVer& a, const SemVer& b) {
    if (a.major != b.major) return a.major < b.major ? -1 : 1;
    if (a.minor != b.minor) return a.minor < b.minor ? -1 : 1;
    if (a.patch != b.patch) return a.patch < b.patch ? -1 : 1;
    return 0;
}

bool is_safe_tag(const std::string& tag) {
    if (tag.empty() || tag.size() > 32) return false;
    for (char character : tag) {
        if (!std::isalnum(static_cast<unsigned char>(character)) && character != '.' &&
            character != '-' && character != '_') {
            return false;
        }
    }
    return true;
}

std::vector<std::string> parse_release_tags(const std::string& feed) {
    std::vector<std::string> tags;
    const std::string marker = "/releases/tag/";
    std::size_t position = 0;
    while ((position = feed.find(marker, position)) != std::string::npos) {
        position += marker.size();
        const std::size_t end = feed.find_first_of("\"'< \r\n", position);
        if (end == std::string::npos) break;
        std::string tag = feed.substr(position, end - position);
        if (is_safe_tag(tag) && std::find(tags.begin(), tags.end(), tag) == tags.end()) {
            tags.push_back(tag);
        }
        position = end;
    }
    return tags;
}

std::string latest_tag() {
    const std::string effective = trim(fetch("Checking for updates",
        "curl -s -L -o NUL -w \"%{url_effective}\" " + std::string(kRepoUrl) + "/releases/latest"));
    const std::size_t marker = effective.find("/tag/");
    if (marker == std::string::npos) return "";
    const std::string tag = effective.substr(marker + 5);
    return is_safe_tag(tag) ? tag : "";
}

std::vector<std::string> all_tags() {
    return parse_release_tags(fetch("Fetching releases",
        "curl -s -L " + std::string(kRepoUrl) + "/releases.atom"));
}

std::string match_version(const std::string& spec, const std::vector<std::string>& tags) {
    const std::string want = without_v(spec);
    for (const auto& tag : tags) {
        if (tag == spec || without_v(tag) == want) return tag;
    }
    return "";
}

struct ReleaseNotes {
    std::string summary;
    bool security = false;
};

// Reads the CHANGELOG section for `tag`: the lead paragraph becomes the summary,
// and any mention of "security" flags the release. The section is expected to
// open with a plain sentence or two before the first `-`/`#` line.
ReleaseNotes release_notes(const std::string& tag) {
    const std::string text = run_capture(
        "curl -s -L -f " + std::string(kRawUrl) + "/" + tag + "/CHANGELOG.md");

    ReleaseNotes notes;
    bool inside = false;
    std::string line;
    for (std::size_t start = 0; start <= text.size(); ) {
        const std::size_t newline = text.find('\n', start);
        line = trim(text.substr(start, newline == std::string::npos ? std::string::npos : newline - start));
        start = newline == std::string::npos ? text.size() + 1 : newline + 1;

        if (line.rfind("## ", 0) == 0) {
            if (inside) break;
            inside = lowercase(line).find("unreleased") == std::string::npos;
            continue;
        }
        if (!inside) continue;
        if (lowercase(line).find("security") != std::string::npos) notes.security = true;
        if (line.rfind('-', 0) == 0 || line.rfind('#', 0) == 0) {
            if (!notes.summary.empty()) break;
            continue;
        }
        if (!line.empty()) notes.summary += (notes.summary.empty() ? "" : " ") + line;
    }

    notes.summary.erase(std::remove(notes.summary.begin(), notes.summary.end(), '`'),
        notes.summary.end());
    if (notes.summary.size() > 200) notes.summary.resize(200);
    return notes;
}

std::string file_sha256(const std::string& path) {
    const std::string output = run_capture("certutil -hashfile \"" + path + "\" SHA256");
    std::size_t line_start = output.find('\n');
    if (line_start == std::string::npos) return "";
    std::size_t line_end = output.find('\n', line_start + 1);
    std::string hash = output.substr(line_start + 1, line_end - line_start - 1);
    hash.erase(std::remove_if(hash.begin(), hash.end(), [](unsigned char c) { return std::isspace(c); }),
        hash.end());
    return lowercase(hash);
}

bool prompt_yes(const std::string& question, bool default_yes) {
    std::cout << "  " << question << ' ' << dim(default_yes ? "[Y/n]" : "[y/N]") << ' ' << std::flush;
    std::string answer;
    if (!std::getline(std::cin, answer)) return false;
    answer = lowercase(trim(answer));
    if (answer.empty()) return default_yes;
    return answer == "y" || answer == "yes";
}

int perform_update(const std::string& tag) {
    const std::string base = std::string(kRepoUrl) + "/releases/download/" + tag + "/";
    const std::string directory = temp_dir();
    const std::string installer = directory + "\\kite-setup.exe";
    const std::string checksum = directory + "\\kite-setup.exe.sha256";

    std::cout << '\n' << "  " << cyan(std::string(arrow()) + " Downloading ") << dim(without_v(tag)) << '\n';
    if (std::system(("curl -fL --progress-bar -o \"" + installer + "\" " + base + "kite-setup.exe").c_str()) != 0) {
        return fail("Download failed. Check your connection and try again.");
    }

    std::system(("curl -sL -f -o \"" + checksum + "\" " + base + "kite-setup.exe.sha256").c_str());
    std::string expected;
    if (std::ifstream checksum_file{checksum}) {
        checksum_file >> expected;
        expected = lowercase(expected);
    }
    if (expected.size() != 64) {
        return fail("Could not fetch the checksum for this download. Aborting.");
    }
    if (expected != file_sha256(installer)) {
        return fail("Checksum mismatch - the download does not match what was published. Aborting.");
    }
    std::cout << "  " << green(tick()) << " Checksum verified\n";

    std::cout << "  " << cyan(std::string(arrow()) + " Launching the installer") << " ...\n";
    std::system(("cmd /c start \"\" \"" + installer +
        "\" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART").c_str());
    std::cout << "  " << green(tick()) << " kite.exe is being replaced in place. "
              << dim("Open a new terminal to pick it up.") << "\n\n";
    return 0;
}

int show_list(const std::string& current_version) {
    const std::vector<std::string> tags = all_tags();
    if (tags.empty()) return fail("Could not reach the release feed.");
    std::cout << "\n  " << bold("Releases") << '\n';
    for (const std::string& tag : tags) {
        const std::string version = without_v(tag);
        const bool installed = version == current_version;
        std::cout << "    " << (installed ? green(version) : version)
                  << (installed ? dim("   (installed)") : "") << '\n';
    }
    std::cout << "\n  " << dim("kite update <version>") << " installs a specific one.\n\n";
    return 0;
}

int update_windows(const UpdateOptions& options) {
    enable_console_style();
    std::cout << '\n' << "  " << bold("Kite update") << "\n\n";

    if (options.list) {
        return show_list(kVersion);
    }

    const bool wants_specific = !options.version.empty() && lowercase(options.version) != "latest";
    std::string tag;
    if (wants_specific) {
        const std::vector<std::string> tags = all_tags();
        if (tags.empty()) return fail("Could not reach the release feed.");
        tag = match_version(options.version, tags);
        if (tag.empty()) {
            std::string available;
            for (const std::string& known : tags) available += (available.empty() ? "" : ", ") + without_v(known);
            return fail("No release " + options.version + ". Available: " + available);
        }
    } else {
        tag = latest_tag();
    }

    if (tag.empty()) return fail("Could not reach the release feed.");

    SemVer target;
    const SemVer current{kVersionMajor, kVersionMinor, kVersionPatch};
    if (!parse_semver(tag, target)) return fail("Unexpected release tag: " + tag);

    const int direction = compare(target, current);
    const std::string shown = without_v(tag);
    std::cout << "  installed   " << bold(kVersion) << '\n';
    const std::string role = direction > 0 ? green(shown)
        : direction < 0 ? yellow(shown + "  (older)")
        : dim(shown);
    std::cout << "  " << (wants_specific ? "target      " : "latest      ") << role << '\n';

    if (direction == 0 && !options.force && !wants_specific) {
        std::cout << "\n  " << green(tick()) << " You are on the latest version.\n"
                  << "  " << dim("kite update --force reinstalls it; kite update <version> installs another.")
                  << "\n\n";
        return 0;
    }

    const ReleaseNotes notes = release_notes(tag);
    std::cout << '\n';
    if (notes.security && direction > 0) {
        std::cout << "  " << yellow(std::string(warn_mark()) + " Includes security fixes")
                  << dim(" - updating is recommended") << "\n\n";
    }
    if (!notes.summary.empty()) print_wrapped(notes.summary, 76);
    std::cout << "  " << dim(std::string(kRepoUrl) + "/blob/" + tag + "/CHANGELOG.md") << "\n\n";

    if (options.check) {
        std::cout << "  Run " << cyan(wants_specific ? "kite update " + shown : "kite update")
                  << " to install it.\n\n";
        return 0;
    }

    if (!options.yes) {
        const std::string question = direction > 0 ? "Install " + shown + " now?"
            : direction < 0 ? "Downgrade from " + std::string(kVersion) + " to " + shown + "?"
            : "Reinstall " + shown + "?";
        if (!prompt_yes(question, direction >= 0)) {
            std::cout << "  Cancelled.\n\n";
            return 0;
        }
    }

    return perform_update(tag);
}

#endif

} // namespace

int run_update(const UpdateOptions& options) {
#if defined(_WIN32)
    return update_windows(options);
#else
    (void)options;
    std::cerr << "kite update is only available on Windows. Use your package manager elsewhere.\n";
    return 1;
#endif
}

} // namespace kite
