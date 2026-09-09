#include "kite/update.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
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

std::string spinner(const std::string& label, const std::string& command) {
    std::atomic<bool> done{false};
    std::string result;
    std::thread worker([&] {
        result = run_capture(command);
        done.store(true);
    });
    const char* frames = "|/-\\";
    int index = 0;
    while (!done.load()) {
        std::cout << "\r  " << cyan(std::string(1, frames[index & 3])) << ' ' << label << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(90));
        ++index;
    }
    worker.join();
    std::cout << "\r  " << green(tick()) << ' ' << label << "   \n";
    return result;
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
    const std::string effective = trim(spinner("Checking for updates",
        "curl -s -L -o NUL -w \"%{url_effective}\" " + std::string(kRepoUrl) + "/releases/latest"));
    const std::size_t marker = effective.find("/tag/");
    if (marker == std::string::npos) return "";
    const std::string tag = effective.substr(marker + 5);
    return is_safe_tag(tag) ? tag : "";
}

std::vector<std::string> all_tags() {
    return parse_release_tags(spinner("Fetching releases",
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
    std::vector<std::string> highlights;
    bool security = false;
};

std::string strip_markdown(const std::string& text) {
    std::string out;
    for (std::size_t index = 0; index < text.size(); ++index) {
        const char character = text[index];
        if (character == '`' || character == '*') continue;
        if (character == '[') {
            const std::size_t close = text.find(']', index);
            if (close != std::string::npos && close + 1 < text.size() && text[close + 1] == '(') {
                const std::size_t paren = text.find(')', close);
                if (paren != std::string::npos) {
                    out += text.substr(index + 1, close - index - 1);
                    index = paren;
                    continue;
                }
            }
        }
        out += character;
    }
    return out;
}

std::string first_sentences(std::string entry, int count) {
    entry = strip_markdown(trim(entry));
    int seen = 0;
    for (std::size_t index = 0; index + 1 < entry.size(); ++index) {
        if (entry[index] == '.' && entry[index + 1] == ' ') {
            if (++seen >= count) { entry.resize(index + 1); break; }
        }
    }
    return trim(entry);
}

std::string headline(std::string entry) {
    entry = first_sentences(std::move(entry), 1);
    if (!entry.empty() && entry.back() == '.') entry.pop_back();
    if (entry.size() > 74) {
        std::size_t cut = entry.rfind(' ', 74);
        if (cut == std::string::npos || cut < 40) cut = 74;
        entry.resize(cut);
        entry += "...";
    }
    return trim(entry);
}

void print_wrapped(const std::string& text, const std::string& indent, std::size_t width) {
    std::size_t start = 0;
    while (start < text.size()) {
        if (text.size() - start <= width) {
            std::cout << indent << text.substr(start) << '\n';
            return;
        }
        std::size_t space = text.rfind(' ', start + width);
        if (space == std::string::npos || space <= start) space = start + width;
        std::cout << indent << text.substr(start, space - start) << '\n';
        start = space + 1;
    }
}

ReleaseNotes release_notes(const std::string& tag) {
    const std::string text = run_capture(
        "curl -s -L -f " + std::string(kRawUrl) + "/" + tag + "/CHANGELOG.md");
    std::vector<std::string> lines;
    std::string current;
    for (char character : text) {
        if (character == '\n') { lines.push_back(current); current.clear(); }
        else if (character != '\r') { current.push_back(character); }
    }
    if (!current.empty()) lines.push_back(current);

    ReleaseNotes notes;
    std::string lead;
    std::string pending;
    bool seen_structure = false;
    const auto flush = [&] {
        const std::string raw = trim(pending);
        pending.clear();
        if (raw.empty()) return;
        if (lowercase(raw).find("security") != std::string::npos) notes.security = true;
        if (notes.highlights.size() >= 3) return;
        const std::string line = headline(raw);
        if (!line.empty()) notes.highlights.push_back(line);
    };

    bool inside = false;
    for (const std::string& line : lines) {
        if (line.rfind("## ", 0) == 0) {
            if (inside) break;
            if (lowercase(line).find("unreleased") != std::string::npos) continue;
            inside = true;
            continue;
        }
        if (!inside) continue;
        const std::string entry = trim(line);
        if (entry.rfind("- ", 0) == 0) { seen_structure = true; flush(); pending = entry.substr(2); }
        else if (entry.rfind("#", 0) == 0) {
            seen_structure = true;
            if (lowercase(entry).find("security") != std::string::npos) notes.security = true;
            flush();
        }
        else if (entry.empty()) { flush(); }
        else if (!pending.empty()) pending += ' ' + entry;
        else if (!seen_structure) lead += (lead.empty() ? "" : " ") + entry;
    }
    flush();

    if (!lead.empty()) {
        notes.summary = first_sentences(lead, 2);
        if (lowercase(lead).find("security") != std::string::npos) notes.security = true;
    }
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
        std::cerr << "  " << red(std::string(cross()) + " Download failed.")
                  << " Check your connection and try again.\n";
        return 1;
    }

    std::system(("curl -sL -f -o \"" + checksum + "\" " + base + "kite-setup.exe.sha256").c_str());
    std::ifstream checksum_file(checksum);
    std::string expected;
    if (checksum_file) {
        checksum_file >> expected;
        expected = lowercase(expected);
    }
    if (expected.size() != 64) {
        std::cerr << "  " << red(std::string(cross()) + " Could not fetch the checksum for this download.")
                  << " Aborting.\n"
                  << "  " << dim("Download the installer yourself from " + std::string(kRepoUrl) + "/releases")
                  << "\n\n";
        return 1;
    }
    if (expected != file_sha256(installer)) {
        std::cerr << "  " << red(std::string(cross()) + " Checksum mismatch")
                  << " - the download does not match what was published. Aborting.\n\n";
        return 1;
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
    if (tags.empty()) {
        std::cerr << "  " << red(std::string(cross()) + " Could not reach the release feed.") << '\n';
        return 1;
    }
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
        if (tags.empty()) {
            std::cerr << "  " << red(std::string(cross()) + " Could not reach the release feed.") << '\n';
            return 1;
        }
        tag = match_version(options.version, tags);
        if (tag.empty()) {
            std::cerr << "  " << red(std::string(cross()) + " No release ") << options.version
                      << ". Available: ";
            for (std::size_t index = 0; index < tags.size(); ++index) {
                std::cerr << (index > 0 ? ", " : "") << without_v(tags[index]);
            }
            std::cerr << "\n\n";
            return 1;
        }
    } else {
        tag = latest_tag();
    }

    if (tag.empty()) {
        std::cerr << "  " << red(std::string(cross()) + " Could not reach the release feed.") << '\n';
        return 1;
    }

    SemVer target;
    const SemVer current{kVersionMajor, kVersionMinor, kVersionPatch};
    if (!parse_semver(tag, target)) {
        std::cerr << "  " << red(std::string(cross()) + " Unexpected release tag: ") << tag << '\n';
        return 1;
    }

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
    if (!notes.summary.empty()) {
        print_wrapped(notes.summary, "  ", 76);
    } else {
        for (const std::string& entry : notes.highlights) {
            std::cout << "    " << dim("-") << ' ' << entry << '\n';
        }
    }
    if (!notes.summary.empty() || !notes.highlights.empty()) {
        std::cout << "  " << dim(std::string(kRepoUrl) + "/blob/" + tag + "/CHANGELOG.md") << "\n\n";
    }

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
