//---------------------------------------------------------------------------------------
// src/database.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2024, Steffen Schümann <s.schuemann@pobox.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//---------------------------------------------------------------------------------------
#pragma once

#include <string>

#include <configuration.hpp>
#include <emulation/coreregistry.hpp>
#include <threadpool.hpp>

#include <chiplet/sha1.hpp>
#include <raylib.h>

class Database
{
public:
    struct FileInfo
    {
        std::string filePath;
        Sha1::Digest digest;
    };
    explicit Database(const emu::CoreRegistry& registry, CadmiumConfiguration& configuration, ThreadPool& threadPool, const std::string& path, const std::unordered_map<std::string, std::string>& badges);
    ~Database();
    int scanLibrary();
    FileInfo scanFile(const std::string& filePath, std::vector<uint8_t>* outData = nullptr);
    void render(Font& font);
    bool fetchC8PDB();
private:
    void fetchProgramInfo();
    const std::string& getBadge(std::string badgeText) const;
    const emu::CoreRegistry& _registry;
    ThreadPool& _threadPool;
    CadmiumConfiguration& _configuration;
    const std::unordered_map<std::string, std::string>& _badges;
    std::future<int> _scanResult;
    std::set<Sha1::Digest> _digests;
    std::chrono::steady_clock::duration durationOfLastJob{};
    struct Private;
    std::unique_ptr<Private> _pimpl;
};

