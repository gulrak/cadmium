//---------------------------------------------------------------------------------------
// src/resourcemanager.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2022, Steffen Schümann <s.schuemann@pobox.com>
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

#include <cstdint>
#include <string>
#include <map>
#include <memory>
#include <utility>

class ResourceManager
{
public:
    class Resource
    {
    public:
        Resource(std::string name, const unsigned char* data, size_t size)
            : _name(std::move(name)), _data(data), _size(size)
        {}
        const std::string& name() const { return _name; }
        const unsigned char* data() const { return _data; }
        size_t size() const { return _size; }
        bool empty() const { return !_data || !_size; }
    private:
        std::string _name;
        const unsigned char* _data;
        size_t _size;
    };

    ResourceManager();
    void registerResources(const void* data, long size);
    bool recourceAvailable(const std::string& name) const;
    Resource resourceForName(const std::string& name) const;

    static ResourceManager& instance();

private:
    typedef std::map<std::string,const void*> ResourceMap;
    ResourceMap _resources;
};
