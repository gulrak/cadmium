#include <resourcemanager.hpp>
#include <raylib.h>

#include <cassert>
#include <cstring>

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

unsigned char *LoadFileDataFromResource(const char *fileName, unsigned int *bytesRead)
{
    auto resource = ResourceManager::instance().resourceForName(fileName);
    if(bytesRead) {
        *bytesRead = resource.size();
    }
    if(resource.size()) {
        auto data = (unsigned char *)RL_MALLOC(resource.size()*sizeof(unsigned char));
        std::memcpy(data, resource.data(), resource.size());
        return data;
    }
    return nullptr;
}

//-------------------------------------------------------------------------

ResourceManager::ResourceManager()
{
    registerResources(g_resourceData, g_resourceDataSize);
    SetLoadFileDataCallback(&LoadFileDataFromResource);
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
        //size_t filesize = readInteger(dataPtr + offset);
        size_t namelen  = readInteger(dataPtr + offset + 4);
        std::string fileName((const char*)dataPtr + offset + 8, namelen);
        _resources[fileName] = dataPtr + offset;
        //std::clog << "Application: Found resource '" << fileName << "' with " << filesize << " bytes." << std::endl;
    }
}

bool ResourceManager::recourceAvailable(const std::string& name) const
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
    size = readInteger(dataPtr);
    size_t fileNameSize = readInteger(dataPtr + 4);
    data = dataPtr + 8 + fileNameSize;

    return Resource(name, data, size);
}
