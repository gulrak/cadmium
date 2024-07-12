//---------------------------------------------------------------------------------------
// src/emulation/coreregistry.hpp
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

#include <emulation/chip8emulatorhost.hpp>
#include <emulation/ichip8.hpp>
#include <emulation/properties.hpp>

#include <iterator>
#include <memory>
#include <unordered_map>
#include <utility>

namespace emu {

enum PropertySelector {PropertiesFromVariant, PropertiesAsGiven};
class CoreRegistry {
public:
    using EmulatorInstance = std::unique_ptr<IChip8Emulator>;
    using FactoryMethod = std::pair<std::string, EmulatorInstance> (*)(const std::string&, Chip8EmulatorHost&, Properties&, PropertySelector);
    struct FactoryInfo
    {
        FactoryInfo(FactoryMethod factoryMethod, std::string  coreDescription)
            : factory(factoryMethod)
            , description(std::move(coreDescription))
        {}
        virtual ~FactoryInfo() = default;
        virtual std::string prefix() const = 0;
        virtual Properties propertiesPrototype() const = 0;
        virtual size_t numberOfVariants() const = 0;
        virtual std::string variantName(size_t index) const = 0;
        virtual const char* variantDescription(size_t index) const = 0;
        virtual Properties variantProperties(size_t index) const = 0;
        Properties variantProperties(const std::string& variant) const;
        virtual EmulatorInstance createCore(const std::string& variant, Chip8EmulatorHost& host, Properties& props, PropertySelector propSel) const = 0;
        FactoryMethod factory;
        std::string description;
    };
    using FactoryMap = std::unordered_map<std::string, std::unique_ptr<FactoryInfo>>;

    struct Iterator
    {
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = std::pair<std::string_view,const FactoryInfo*>;
        using pointer           = value_type*;
        using reference         = value_type&;
        explicit Iterator(FactoryMap::iterator iter) : _iter(iter) {}
        reference operator*() const { _val = {_iter->first, _iter->second.get()}; return _val; }
        pointer operator->() const { return &(operator*()); }
        Iterator& operator++() { _iter++; return *this; }
        Iterator operator++(int) { Iterator tmp = *this; ++(*this); return tmp; }
        friend bool operator== (const Iterator& a, const Iterator& b) { return a._iter == b._iter; };
        friend bool operator!= (const Iterator& a, const Iterator& b) { return a._iter != b._iter; };
    private:
        FactoryMap::iterator _iter;
        mutable value_type _val;
    };

    static bool registerFactory(const std::string& name, std::unique_ptr<FactoryInfo>&& factoryInfo)
    {
        if(auto iter = factoryMap().find(name); iter == factoryMap().end()) {
            factoryMap()[name] = std::move(factoryInfo);
            return true;
        }
        return false;
    }

    static std::pair<std::string, EmulatorInstance> create(const std::string& name, const std::string& variant, Chip8EmulatorHost& host, Properties& properties, PropertySelector propSel)
    {
        if (auto iter = factoryMap().find(name); iter != factoryMap().end()) {
            return iter->second->factory(variant, host, properties, propSel);
        }
        for(const auto& [coreName, info] : factoryMap()) {
            if(fuzzyCompare(info->prefix(), name) || fuzzyCompare(coreName, name))
                return info->factory(variant, host, properties, propSel);
        }
        return {};
    }

    static Properties propertiesForPreset(const std::string& name)
    {
        return {};
    }

    Iterator begin() const {
        return Iterator(factoryMap().begin());
    }
    Iterator end() const {
        return Iterator(factoryMap().end());
    }

private:
    static FactoryMap& factoryMap();
};

} // emu

