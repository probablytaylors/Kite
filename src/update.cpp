#include "kite/update.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include "kite/version.hpp"

#if defined(_WIN32)
#include <windows.h>
#endif

namespace kite {

namespace {

constexpr const char* kRepoUrl = "https://github.com/probablytaylors/Kite";

#if defined(_WIN32)

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
    bool operator==(const SemVer& other) const {
        return major == other.major && minor == other.minor && patch == other.patch;
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
    const std::string effective = trim(run_capture(
        "curl -s -L -o NUL -w \"%{url_effective}\" " + url));
    const std::size_t marker = effective.find("/tag/");
    if (marker == std::string::npos) return "";
    return effective.substr(marker + 5);
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

int perform_update(const std::string& tag) {
    const std::string base = std::string(kRepoUrl) + "/releases/latest/download/";
    const std::string directory = temp_dir();
    const std::string installer = directory + "\\kite-setup.exe";
    const std::string checksum = directory + "\\kite-setup.exe.sha256";

    std::cout << "Downloading " << tag << "...\n";
    if (std::system(("curl -sL -f -o \"" + installer + "\" " + base + "kite-setup.exe").c_str()) != 0) {
        std::cerr << "Download failed.\n";
        return 1;
    }
    std::system(("curl -sL -f -o \"" + checksum + "\" " + base + "kite-setup.exe.sha256").c_str());

    std::ifstream checksum_file(checksum);
    if (checksum_file) {
        std::string expected;
        checksum_file >> expected;
        for (auto& character : expected) character = static_cast<char>(std::tolower(character));
        if (!expected.empty() && expected != file_sha256(installer)) {
            std::cerr << "Checksum mismatch; aborting.\n";
            return 1;
        }
    } else {
        std::cerr << "Warning: no published checksum for this release.\n";
    }

    std::cout << "Launching the installer. Approve the prompt to finish updating.\n";
    std::system(("cmd /c start \"\" \"" + installer + "\" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART").c_str());
    return 0;
}

int update_windows(bool check_only, bool force) {
    const std::string tag = latest_tag();
    if (tag.empty()) {
        std::cerr << "Could not reach the release feed.\n";
        return 1;
    }

    SemVer latest;
    SemVer current{kVersionMajor, kVersionMinor, kVersionPatch};
    if (!parse_semver(tag, latest)) {
        std::cerr << "Unexpected release tag: " << tag << '\n';
        return 1;
    }

    if (!force && !(latest > current)) {
        std::cout << "kite " << kVersion << " is the latest version.\n";
        return 0;
    }

    std::cout << "Update available: " << tag << " (installed: " << kVersion << ")\n";
    if (check_only) return 0;
    return perform_update(tag);
}

#endif

} // namespace

int run_update(bool check_only, bool force) {
#if defined(_WIN32)
    return update_windows(check_only, force);
#else
    (void)check_only;
    (void)force;
    std::cerr << "kite update is only available on Windows. Use your package manager elsewhere.\n";
    return 1;
#endif
}

} // namespace kite
