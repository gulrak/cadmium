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
        virtual size_t numberOfVariants() const = 0;
        virtual std::string variantName(size_t index) const = 0;
        virtual const char* variantDescription(size_t index) const = 0;
        virtual Properties variantProperties(size_t index) const = 0;
        FactoryMethod factory;
        std::string description;
    };
    static bool registerFactory(const std::string& name, std::unique_ptr<FactoryInfo>&& factoryInfo)
    {
        if(auto iter = _factories.find(name); iter == _factories.end()) {
            _factories[name] = std::move(factoryInfo);
            return true;
        }
        return false;
    }

    static std::pair<std::string, EmulatorInstance> create(const std::string& name, const std::string& variant, Chip8EmulatorHost& host, Properties& properties, PropertySelector propSel)
    {
        if (auto iter = _factories.find(name); iter != _factories.end()) {
            return iter->second->factory(variant, host, properties, propSel);
        }
        return {};
    }



private:
    static std::unordered_map<std::string, std::unique_ptr<FactoryInfo>> _factories;
};

} // emu

