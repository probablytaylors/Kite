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
    return path;
}

struct SemVer {
    int major = 0;
    int minor = 0;
    int patch = 0;
    bool operator>(const SemVer& other) const {
        if (major != other.major) return major > other.major;
        if (minor != other.minor) return minor > other.minor;
        return patch > other.patch;
    }
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

std::string latest_tag() {
    const std::string url = std::string(kRepoUrl) + "/releases/latest";
    const std::string effective = trim(spinner("Checking for updates",
        "curl -s -L -o NUL -w \"%{url_effective}\" " + url));
    const std::size_t marker = effective.find("/tag/");
    if (marker == std::string::npos) return "";
    return effective.substr(marker + 5);
}

std::vector<std::string> release_highlights(const std::string& tag) {
    const std::string text = run_capture(
        "curl -s -L -f " + std::string(kRawUrl) + "/" + tag + "/CHANGELOG.md");
    std::vector<std::string> lines;
    std::string current;
    for (char character : text) {
        if (character == '\n') { lines.push_back(current); current.clear(); }
        else if (character != '\r') { current.push_back(character); }
    }
    if (!current.empty()) lines.push_back(current);

    std::vector<std::string> highlights;
    std::string pending;
    const auto flush = [&] {
        std::string entry = trim(pending);
        pending.clear();
        if (entry.empty() || highlights.size() >= 6) return;
        entry.erase(std::remove(entry.begin(), entry.end(), '`'), entry.end());
        if (entry.size() > 108) entry = entry.substr(0, 105) + "...";
        highlights.push_back(entry);
    };

    bool inside = false;
    for (const std::string& line : lines) {
        if (line.rfind("## ", 0) == 0) {
            std::string title = line.substr(3);
            for (auto& c : title) c = static_cast<char>(std::tolower(c));
            if (inside) break;
            if (title.find("unreleased") != std::string::npos) continue;
            inside = true;
            continue;
        }
        if (!inside) continue;
        const std::string entry = trim(line);
        if (entry.rfind("- ", 0) == 0) { flush(); pending = entry.substr(2); }
        else if (entry.empty() || entry.rfind("### ", 0) == 0) flush();
        else if (!pending.empty()) pending += ' ' + entry;
        if (highlights.size() >= 6) break;
    }
    flush();
    return highlights;
}

std::string file_sha256(const std::string& path) {
    const std::string output = run_capture("certutil -hashfile \"" + path + "\" SHA256");
    std::size_t line_start = output.find('\n');
    if (line_start == std::string::npos) return "";
    std::size_t line_end = output.find('\n', line_start + 1);
    std::string hash = output.substr(line_start + 1, line_end - line_start - 1);
    hash.erase(std::remove_if(hash.begin(), hash.end(), [](unsigned char c) { return std::isspace(c); }),
        hash.end());
    for (auto& character : hash) character = static_cast<char>(std::tolower(character));
    return hash;
}

bool prompt_yes(const std::string& question) {
    std::cout << "  " << question << ' ' << dim("[Y/n]") << ' ' << std::flush;
    std::string answer;
    if (!std::getline(std::cin, answer)) return false;
    answer = trim(answer);
    for (auto& character : answer) character = static_cast<char>(std::tolower(character));
    return answer.empty() || answer == "y" || answer == "yes";
}

int perform_update(const std::string& tag) {
    const std::string base = std::string(kRepoUrl) + "/releases/latest/download/";
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
    if (checksum_file) {
        std::string expected;
        checksum_file >> expected;
        for (auto& character : expected) character = static_cast<char>(std::tolower(character));
        if (!expected.empty() && expected != file_sha256(installer)) {
            std::cerr << "  " << red(std::string(cross()) + " Checksum mismatch")
                      << " - the download may be corrupt. Aborting.\n";
            return 1;
        }
        std::cout << "  " << green(tick()) << " Checksum verified\n";
    } else {
        std::cout << "  " << yellow(warn_mark()) << " No published checksum for this release\n";
    }

    std::cout << "  " << cyan(std::string(arrow()) + " Launching the installer") << " ...\n";
    std::system(("cmd /c start \"\" \"" + installer +
        "\" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART").c_str());
    std::cout << "  " << green(tick()) << " kite.exe is being replaced in place. "
              << dim("Open a new terminal to pick it up.") << "\n\n";
    return 0;
}

int update_windows(bool check_only, bool force, bool assume_yes) {
    enable_console_style();
    std::cout << '\n' << "  " << bold("Kite update") << "\n\n";

    const std::string tag = latest_tag();
    if (tag.empty()) {
        std::cerr << "  " << red(std::string(cross()) + " Could not reach the release feed.") << '\n';
        return 1;
    }

    SemVer latest;
    const SemVer current{kVersionMajor, kVersionMinor, kVersionPatch};
    if (!parse_semver(tag, latest)) {
        std::cerr << "  " << red(std::string(cross()) + " Unexpected release tag: ") << tag << '\n';
        return 1;
    }

    const bool newer = latest > current;
    const std::string shown = without_v(tag);
    std::cout << "  installed   " << bold(kVersion) << '\n';
    std::cout << "  latest      " << (newer ? green(shown) : dim(shown)) << '\n';

    if (!newer && !force) {
        std::cout << "\n  " << green(tick()) << ' ' << "You are on the latest version.\n\n";
        return 0;
    }

    const std::vector<std::string> highlights = release_highlights(tag);
    if (!highlights.empty()) {
        std::cout << "\n  " << bold("What's new") << '\n';
        for (const std::string& entry : highlights) {
            std::cout << "    " << dim("-") << ' ' << entry << '\n';
        }
        std::cout << "  " << dim(std::string(kRepoUrl) + "/blob/" + tag + "/CHANGELOG.md") << '\n';
    }
    std::cout << '\n';

    if (check_only) {
        std::cout << "  Run " << cyan("kite update") << " to install it.\n\n";
        return 0;
    }

    if (!assume_yes && !prompt_yes(newer ? "Install " + shown + " now?" : "Reinstall " + shown + "?")) {
        std::cout << "  Cancelled.\n\n";
        return 0;
    }

    return perform_update(tag);
}

#endif

} // namespace

int run_update(bool check_only, bool force, bool assume_yes) {
#if defined(_WIN32)
    return update_windows(check_only, force, assume_yes);
#else
    (void)check_only;
    (void)force;
    (void)assume_yes;
    std::cerr << "kite update is only available on Windows. Use your package manager elsewhere.\n";
    return 1;
#endif
}

} // namespace kite
