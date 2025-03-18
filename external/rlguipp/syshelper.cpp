//
// Created by Steffen Schümann on 05.03.25.
//
#include "syshelper.hpp"

#ifdef _WIN32
    #include <windows.h>
    #include <shlobj.h>
#elif defined(__APPLE__)
    #include <sysdir.h>
#elif defined(__linux__)
    #include <fstream>
#endif

#include <string>
#include <optional>
#include <cstdlib>
#include <ghc/filesystem.hpp>
#include <cctype>

#ifndef _WIN32
std::optional<std::string> expandPath(const std::string& path) {
    std::string result;
    size_t i = 0;
    if (!path.empty() && path[0] == '~' && (path.size() == 1 || path[1] == '/')) {
        if (const char* home = std::getenv("HOME")) {
            result.append(home);
        }
        i = 1;
    }
    while (i < path.size()) {
        if (path[i] == '$') {
            if (i + 1 < path.size() && path[i + 1] == '{') {
                size_t closing = path.find('}', i + 2);
                if (closing == std::string::npos) {
                    return std::nullopt;
                }
                std::string var_name = path.substr(i + 2, closing - (i + 2));
                if (var_name.empty() || !(std::isalpha(var_name[0]) || var_name[0] == '_')) {
                    return std::nullopt;
                }
                for (char ch : var_name) {
                    if (!(std::isalnum(ch) || ch == '_')) {
                        return std::nullopt;
                    }
                }
                if (const char* env = std::getenv(var_name.c_str())) {
                    result.append(env);
                }
                i = closing + 1;
            } else {
                size_t j = i + 1;
                if (j < path.size() && (std::isalpha(path[j]) || path[j] == '_')) {
                    size_t start = j;
                    while (j < path.size() && (std::isalnum(path[j]) || path[j] == '_')) {
                        ++j;
                    }
                    std::string var_name = path.substr(start, j - start);
                    if (const char* env = std::getenv(var_name.c_str())) {
                        result.append(env);
                    }
                    i = j;
                } else {
                    return std::nullopt;
                }
            }
        } else {
            result.push_back(path[i]);
            ++i;
        }
    }
    return result;
}
#endif

#ifdef __APPLE__
static std::string getSysDir(sysdir_search_path_directory_t dir)
{
    char path[PATH_MAX];
    sysdir_search_path_enumeration_state state = sysdir_start_search_path_enumeration(dir, SYSDIR_DOMAIN_MASK_USER);
    while ( (state = sysdir_get_next_search_path_enumeration(state, path)) != 0 ) {
        if (std::strlen(path) > 0) {
            auto expanded = expandPath(path);
            return expanded ? *expanded : path;
        }
    }
    return "";
}
#endif

UserDirectories getUserDirectories() {
    UserDirectories dirs;
#ifdef _WIN32
    // Windows: Use SHGetKnownFolderPath.
    auto getKnownFolderPath = [](REFKNOWNFOLDERID folderId) -> std::string {
        PWSTR path = nullptr;
        std::string result;
        if (SUCCEEDED(SHGetKnownFolderPath(folderId, 0, nullptr, &path))) {
            int size = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
            if (size > 0) {
                std::string utf8(size, '\0');
                WideCharToMultiByte(CP_UTF8, 0, path, -1, &utf8[0], size, nullptr, nullptr);
                if (!utf8.empty() && utf8.back() == '\0') {
                    utf8.pop_back();
                }
                result = utf8;
            }
            CoTaskMemFree(path);
        }
        return result;
    };

    dirs.home      = getKnownFolderPath(FOLDERID_Profile);
    dirs.documents = getKnownFolderPath(FOLDERID_Documents);
    dirs.downloads = getKnownFolderPath(FOLDERID_Downloads);

#else
    // For non-Windows: first, set home from the HOME environment variable.
    if (const char* homeEnv = std::getenv("HOME")) {
        dirs.home = homeEnv;
    }

    #ifdef __APPLE__
        if (!dirs.home.empty()) {
            dirs.documents = getSysDir(SYSDIR_DIRECTORY_DOCUMENT);
            dirs.downloads = getSysDir(SYSDIR_DIRECTORY_DOWNLOADS);
        }
    #elif defined(__linux__)
        // Linux: Read $HOME/.config/user-dirs.dirs
        if (!dirs.home.empty()) {
            std::filesystem::path configFile = std::filesystem::path(dirs.home) / ".config" / "user-dirs.dirs";
            if (std::filesystem::exists(configFile)) {
                std::ifstream in(configFile);
                std::string line;
                while (std::getline(in, line)) {
                    // Skip empty lines and comments.
                    if (line.empty() || line[0] == '#')
                        continue;
                    // Look for the Documents entry.
                    if (line.find("XDG_DOCUMENTS_DIR=") == 0) {
                        auto pos = line.find('=');
                        if (pos != std::string::npos) {
                            std::string value = line.substr(pos + 1);
                            // Remove any surrounding quotes.
                            if (!value.empty() && value.front() == '"')
                                value.erase(0, 1);
                            if (!value.empty() && value.back() == '"')
                                value.pop_back();
                            // Replace a leading "$HOME" with the actual home directory.
                            const std::string homePrefix = "$HOME";
                            if (value.rfind(homePrefix, 0) == 0) {
                                value.replace(0, homePrefix.size(), dirs.home);
                            }
                            dirs.documents = value;
                        }
                    }
                    // Look for the Downloads entry.
                    else if (line.find("XDG_DOWNLOAD_DIR=") == 0) {
                        auto pos = line.find('=');
                        if (pos != std::string::npos) {
                            std::string value = line.substr(pos + 1);
                            if (!value.empty() && value.front() == '"')
                                value.erase(0, 1);
                            if (!value.empty() && value.back() == '"')
                                value.pop_back();
                            const std::string homePrefix = "$HOME";
                            if (value.rfind(homePrefix, 0) == 0) {
                                value.replace(0, homePrefix.size(), dirs.home);
                            }
                            dirs.downloads = value;
                        }
                    }
                }
            }
        }
    #endif
#endif

    return dirs;
}
