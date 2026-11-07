#include <resourcemanager.hpp>
#include <raylib.h>

#include <cassert>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string_view>

#include <zstd.h>

extern "C" {
extern const unsigned char g_resourceData[];
extern const int g_resourceDataSize;
}

static size_t readInteger(const unsigned char* data)
{
    uint32_t val = data[0];
    val |= uint32_t(data[1])<<8u;
    val |= uint32_t(data[2])<<16u;
    val |= uint32_t(data[3])<<24u;
    return size_t(val);
}

namespace {

constexpr std::uint32_t RESOURCE_FLAG_ZSTD_COMPRESSED = 0x01;

// An entry of the resource data: stored size, original size, flags and name length, followed by the name and data.
struct ResourceEntry {
    size_t storedSize = 0;
    size_t originalSize = 0;
    std::uint32_t flags = 0;
    size_t nameSize = 0;
    const unsigned char* name = nullptr;
    const unsigned char* data = nullptr;
};

ResourceEntry readEntry(const unsigned char* dataPtr)
{
    ResourceEntry entry;
    entry.storedSize = readInteger(dataPtr);
    entry.originalSize = readInteger(dataPtr + 4);
    entry.flags = static_cast<std::uint32_t>(readInteger(dataPtr + 8));
    entry.nameSize = readInteger(dataPtr + 12);
    entry.name = dataPtr + 16;
    entry.data = entry.name + entry.nameSize;
    return entry;
}

std::vector<unsigned char> decompressResource(std::string_view name, const ResourceEntry& entry)
{
    const unsigned long long contentSize = ZSTD_getFrameContentSize(entry.data, entry.storedSize);
    if (contentSize == ZSTD_CONTENTSIZE_ERROR) {
        throw std::runtime_error("embedded resource is not a valid zstd frame: " + std::string(name));
    }
    if (contentSize == ZSTD_CONTENTSIZE_UNKNOWN) {
        throw std::runtime_error("embedded resource zstd frame does not declare decompressed size: " + std::string(name));
    }
    if (contentSize != entry.originalSize) {
        throw std::runtime_error("embedded resource zstd size mismatch: " + std::string(name));
    }
    if (contentSize > static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error("embedded resource is too large for this host: " + std::string(name));
    }
    std::vector<unsigned char> decoded(static_cast<std::size_t>(contentSize));
    const std::size_t result = ZSTD_decompress(decoded.data(), decoded.size(), entry.data, entry.storedSize);
    if (ZSTD_isError(result)) {
        throw std::runtime_error("failed to decompress embedded resource " + std::string(name) + ": " + ZSTD_getErrorName(result));
    }
    decoded.resize(result);
    return decoded;
}

}  // namespace

unsigned char *LoadFileDataFromResource(const char *fileName, int *bytesRead)
{
    const auto resource = ResourceManager::instance().resourceForName(fileName);
    if(bytesRead) {
        *bytesRead = static_cast<int>(resource.size());
    }
    if(!resource.empty()) {
        auto* data = static_cast<unsigned char*>(RL_MALLOC(resource.size() * sizeof(unsigned char)));
        std::memcpy(data, resource.data(), resource.size());
        return data;
    }
    return nullptr;
}

char *LoadFileTextFromResource(const char *fileName)
{
    const auto resource = ResourceManager::instance().resourceForName(fileName);
    if(!resource.empty()) {
        auto* data = static_cast<char*>(RL_MALLOC((resource.size() + 1) * sizeof(char)));
        std::memcpy(data, resource.data(), resource.size());
        data[resource.size()] = 0;
        return data;
    }
    return nullptr;
}

//-------------------------------------------------------------------------

ResourceManager::ResourceManager()
{
    registerResources(g_resourceData, g_resourceDataSize);
    SetLoadFileDataCallback(&LoadFileDataFromResource);
    SetLoadFileTextCallback(&LoadFileTextFromResource);
}

ResourceManager& ResourceManager::instance()
{
    static ResourceManager manager;
    return manager;
}

void ResourceManager::registerResources(const void* data, long size)
{
    const auto* startPtr = (const unsigned char*)data;
    const auto* dataPtr = startPtr;
    size_t numFiles = readInteger(dataPtr);
    for(size_t i = 0; i < numFiles && (dataPtr - startPtr) < size; ++i)
    {
        size_t offset   = readInteger(dataPtr + 4 + i*4);
        const ResourceEntry entry = readEntry(dataPtr + offset);
        std::string fileName((const char*)entry.name, entry.nameSize);
        _resources[fileName] = dataPtr + offset;
        //std::clog << "Application: Found resource '" << fileName << "' with " << filesize << " bytes." << std::endl;
    }
}

bool ResourceManager::resourceAvailable(const std::string& name) const
{
    return _resources.find(name) != _resources.end();
}

ResourceManager::Resource ResourceManager::resourceForName(const std::string& name) const
{
    const unsigned char* data = 0;
    size_t size = 0;
    auto iter = _resources.find(name);
    assert(iter != _resources.end());

    if(iter == _resources.end())
        return Resource(name, nullptr, 0);

    const auto* dataPtr = (const unsigned char*)iter->second;
    const ResourceEntry entry = readEntry(dataPtr);
    if ((entry.flags & RESOURCE_FLAG_ZSTD_COMPRESSED) != 0) {
        // the cache only grows, so the data of an entry stays where it is when others are added
        std::lock_guard lock(_decompressedMutex);
        auto cacheIter = _decompressedResources.find(name);
        if (cacheIter == _decompressedResources.end()) {
            cacheIter = _decompressedResources.emplace(name, decompressResource(name, entry)).first;
        }
        data = cacheIter->second.data();
        size = cacheIter->second.size();
    }
    else {
        data = entry.data;
        size = entry.originalSize;
    }

    return Resource(name, data, size);
}
