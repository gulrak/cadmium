//
// Created by schuemann on 10.05.23.
//
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <fmt/format.h>

inline std::vector<uint8_t> loadFile(const std::string& file)
{
    std::ifstream is(file, std::ios::binary | std::ios::ate);
    auto size = is.tellg();
    is.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (is.read((char*)buffer.data(), size)) {
        return buffer;
    }
    return {};
}

int main(int argc, char* argv[])
{
    std::string original = argv[1];
    std::string derived = argv[2];
    std::vector<uint8_t> memory;
    auto odata = loadFile(original);
    auto ddata = loadFile(derived);
    memory.resize(std::max(4096ul,ddata.size()));
    std::memcpy(memory.data(), odata.data(), odata.size());
    auto iter = ddata.begin();
    auto miter = memory.begin();
    while(iter != ddata.end()) {
        while (iter != ddata.end() && *miter == *iter) {
            miter++; iter++;
        }
        auto start = iter;
        while (iter != ddata.end() && *miter != *iter) {
            miter++; iter++;
        }
        if(start != iter) {
            std::cout << fmt::format("    {{0x{:03x}, {{", (start - ddata.begin()));
            for(auto i = start; i < iter; ++i) {
                if(i != start)
                    std::cout << ", ";
                std::cout << fmt::format("0x{:02x}", (unsigned)*i);
            }
            std::cout << "}},"  << std::endl;
        }
    }
    return 0;
}
