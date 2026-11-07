//
// Created by Steffen Schümann on 05.08.25.
//
#pragma once

#include <filesystem>
#include <random>
#include <string>

namespace fs = std::filesystem;

class TemporaryDirectory {
public:
    explicit TemporaryDirectory(std::string prefix = "tmp")
      : _prefix(std::move(prefix))
    {
        namespace fs = std::filesystem;
        fs::path base = fs::temp_directory_path();
        std::error_code ec;

        for (int attempt = 0; attempt < 100; ++attempt) {
            if (fs::path candidate = base / make_name(); fs::create_directory(candidate, ec)) {
                _path = std::move(candidate);
                return;
            }
            // if it already exists, ec will be non-zero but create_directory returns false
            ec.clear();
        }

        throw std::runtime_error("TempDir: unable to create a unique temporary directory");
    }

    // non‐copyable
    TemporaryDirectory(TemporaryDirectory const&) = delete;
    TemporaryDirectory& operator=(TemporaryDirectory const&) = delete;

    // movable
    TemporaryDirectory(TemporaryDirectory&& o) noexcept : _prefix(), _path(std::exchange(o._path, {})) {}
    TemporaryDirectory& operator=(TemporaryDirectory&& o) noexcept {
        if (this != &o) {
            cleanup();
            _path = std::exchange(o._path, {});
        }
        return *this;
    }

    ~TemporaryDirectory() noexcept {
        cleanup();
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return _path; }
    operator std::filesystem::path() const noexcept { return _path; } // NOLINT(google-explicit-constructor)

private:
    std::string _prefix;
    std::filesystem::path _path;

    void cleanup() const noexcept {
        if (!_path.empty()) {
            std::error_code ec;
            std::filesystem::remove_all(_path, ec);
        }
    }

    [[nodiscard]] std::string make_name() const {
        // e.g. "tmp-5F3A7D"
        thread_local std::mt19937_64 eng{ std::random_device{}() };
        thread_local std::uniform_int_distribution<uint64_t> dist;

        uint64_t r = dist(eng);
        std::string hex;
        hex.reserve(16);
        for (int i = 0; i < 6; ++i) {
            uint8_t nib = (r >> (i*4)) & 0xF;
            hex.push_back(static_cast<char>(nib < 10 ? '0' + nib : 'A' + (nib - 10)));
        }
        return _prefix + "-" + hex;
    }
};
