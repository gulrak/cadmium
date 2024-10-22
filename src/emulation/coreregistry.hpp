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

#include <emulation/emulatorhost.hpp>
#include <emulation/ichip8.hpp>
#include <emulation/iemulationcore.hpp>
#include <emulation/properties.hpp>
#include <chiplet/chip8variants.hpp>

#include <iterator>
#include <memory>
#include <unordered_map>
#include <set>
#include <utility>

#include <ghc/span.hpp>

namespace emu {

enum PropertySelector {PropertiesFromVariant, PropertiesAsGiven};
class CoreRegistry {
public:
    using EmulatorInstance = std::unique_ptr<IEmulationCore>;
    //using FactoryMethod = std::pair<std::string, EmulatorInstance> (*)(const std::string&, Chip8EmulatorHost&, Properties&, PropertySelector);
    template<typename T>
    struct SetupInfo
    {
        const char* presetName{};
        const char* description{};
        const char* defaultExtensions{};
        chip8::VariantSet supportedChip8Variants;
        T options;
    };
    struct IFactoryInfo
    {
        struct VariantIndex
        {
            size_t index{0};
            bool isCustom{true};
        };
        IFactoryInfo(int orderScore, std::string  coreDescription)
            : description(std::move(coreDescription))
            , score(orderScore)
        {}
        virtual ~IFactoryInfo() = default;
        virtual std::string prefix() const = 0;
        virtual Properties propertiesPrototype() const = 0;
        virtual size_t numberOfVariants() const = 0;
        virtual std::string variantName(size_t index) const = 0;
        virtual const char* variantDescription(size_t index) const = 0;
        virtual const char* variantExtensions(size_t index) const = 0;
        virtual Properties variantProperties(size_t index) const = 0;
        virtual VariantIndex variantIndex(const Properties& props) const = 0;
        virtual chip8::VariantSet variantSupportedChip8(size_t index) const = 0;
        void cacheVariantMappings() const;
        bool hasVariant(const std::string& variant) const;
        Properties variantProperties(const std::string& variant) const;
        virtual std::pair<std::string, EmulatorInstance> createCore(const std::string& variant, EmulatorHost& host, Properties& props, PropertySelector propSel) const = 0;
        virtual std::pair<std::string, EmulatorInstance> createCore(EmulatorHost& host, Properties& props) const = 0;
        std::string description;
        std::string variantsCombo{};
        int score{0};
        mutable std::map<std::string,size_t> presetMappings{};
    };
    template<typename CoreType, typename PresetType, typename OptionsType>
    struct FactoryInfo : public IFactoryInfo
    {
        FactoryInfo(int orderScore, ghc::span<const PresetType> presetSet, std::string coreDescription) : IFactoryInfo(orderScore, std::move(coreDescription)), presets(presetSet) {}
        Properties propertiesPrototype() const override
        {
            return presets[0].options.asProperties();
        }
        size_t numberOfVariants() const override
        {
            return presets.size();
        }
        std::string variantName(size_t index) const override
        {
            return index < numberOfVariants() ? presets[index].presetName : presets[0].presetName;
        }
        const char* variantDescription(size_t index) const override
        {
            return index < numberOfVariants() ? presets[index].description : presets[0].description;
        }
        const char* variantExtensions(size_t index) const override
        {
            return index < numberOfVariants() ? presets[index].defaultExtensions : presets[0].defaultExtensions;
        }
        Properties variantProperties(size_t index) const override
        {
            return index < numberOfVariants() ? presets[index].options.asProperties() : presets[0].options.asProperties();
        }
        chip8::VariantSet variantSupportedChip8(size_t index) const override
        {
            return index < numberOfVariants() ? presets[index].supportedChip8Variants : presets[0].supportedChip8Variants;
        }
        std::pair<std::string, EmulatorInstance> createCore(const std::string& variant, EmulatorHost& host, Properties& props, PropertySelector propSel) const override
        {
            Properties defaultProps;
            auto newVariant = variant;
            for(const auto& setupInfo : presets) {
                if(setupInfo.presetName == variant) {
                    defaultProps = setupInfo.options.asProperties();
                    break;
                }
            }
            if(propSel == PropertiesFromVariant) {
                props = defaultProps ? defaultProps : presets[0].options.asProperties();
            }
            auto options = OptionsType::fromProperties(props);
            return {newVariant, std::make_unique<CoreType>(host, props)};
        }
        std::pair<std::string, EmulatorInstance> createCore(EmulatorHost& host, Properties& props) const override
        {
            std::string variant;
            auto idx = variantIndex(props);
            const auto& info = presets[idx.index];
            if(prefix().empty())
                variant = info.presetName;
            else
                variant = !std::strcmp(info.presetName, "NONE") ? prefix() : prefix() + "-" + info.presetName;
            if(props != info.options.asProperties()) {
                variant += "*";
            }
            auto options = OptionsType::fromProperties(props);
            return {variant, std::make_unique<CoreType>(host, props)};
        }
    private:
        ghc::span<const PresetType> presets{nullptr};
    };
    using FactoryMap = std::unordered_map<std::string, std::unique_ptr<IFactoryInfo>>;

    struct Iterator
    {
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = std::pair<std::string_view,IFactoryInfo*>;
        using pointer           = value_type*;
        using reference         = value_type&;
        explicit Iterator(std::vector<value_type>::iterator iter) : _iter(iter) {}
        reference operator*() const { _val = {_iter->first, _iter->second}; return _val; }
        pointer operator->() const { return &(operator*()); }
        Iterator& operator++() { ++_iter; return *this; }
        Iterator operator++(int) { Iterator tmp = *this; ++(*this); return tmp; }
        friend bool operator== (const Iterator& a, const Iterator& b) { return a._iter == b._iter; };
        friend bool operator!= (const Iterator& a, const Iterator& b) { return a._iter != b._iter; };
    private:
        std::vector<value_type>::iterator _iter;
        mutable value_type _val;
    };

    static bool registerFactory(const std::string& name, std::unique_ptr<IFactoryInfo>&& factoryInfo)
    {
        if(auto iter = factoryMap().find(name); iter == factoryMap().end()) {
            factoryMap()[name] = std::move(factoryInfo);
            return true;
        }
        return false;
    }

    static std::pair<std::string, EmulatorInstance> create(const std::string& name, const std::string& variant, EmulatorHost& host, Properties& properties, PropertySelector propSel)
    {
        if (auto iter = factoryMap().find(name); iter != factoryMap().end()) {
            return iter->second->createCore(variant, host, properties, propSel);
        }
        for(const auto& [coreName, info] : factoryMap()) {
            if(fuzzyCompare(info->prefix(), name) || fuzzyCompare(coreName, name))
                return info->createCore(variant, host, properties, propSel);
        }
        return {};
    }

    static std::pair<std::string, EmulatorInstance> create(EmulatorHost& host, Properties& properties)
    {
        if (auto iter = factoryMap().find(properties.propertyClass()); iter != factoryMap().end()) {
            return iter->second->createCore(host, properties);
        }
        return {};
    }

    static Properties propertiesForPreset(std::string_view name)
    {
        for(const auto& [coreName, info] : factoryMap()) {
            if(fuzzyCompare(info->prefix(), name)) {
                return info->variantProperties(0);
            }
            for(size_t idx = 0; idx < info->numberOfVariants(); ++idx) {
                if(fuzzyCompare(info->prefix() + info->variantName(idx), name)) {
                    return info->variantProperties(idx);
                }
            }
        }
        return {};
    }

    int classIndex(const Properties& props) const
    {
        for(int idx = 0; idx < orderedFactories.size(); ++idx) {
            if(fuzzyCompare(orderedFactories[idx].first, props.propertyClass())) {
                return idx;
            }
        }
        return -1;
    }
    static IFactoryInfo::VariantIndex variantIndex(const Properties& props)
    {
        if (auto iter = factoryMap().find(props.propertyClass()); iter != factoryMap().end()) {
            return iter->second->variantIndex(props);
        }
        return {};
    }

    static Properties propertiesForExtension(std::string_view extension)
    {
        for(const auto& [name, factory] : factoryMap()) {
            for(size_t i = 0; i < factory->numberOfVariants(); ++i) {
                auto extensions = split(factory->variantExtensions(i), ';');
                if(auto iter = std::ranges::find(extensions, extension); iter != extensions.end()) {
                    return factory->variantProperties(i);
                }
            }
        }
        return {};
    }

    static std::string presetForExtension(std::string_view extension)
    {
        for(const auto& [name, factory] : factoryMap()) {
            for(size_t i = 0; i < factory->numberOfVariants(); ++i) {
                auto extensions = split(factory->variantExtensions(i), ';');
                if(auto iter = std::ranges::find(extensions, extension); iter != extensions.end()) {
                    if(factory->prefix().empty())
                        return factory->variantName(i);
                    else
                        return factory->variantName(i) == "NONE" ? factory->prefix() : factory->prefix() + "-" + factory->variantName(i);
                }
            }
        }
        return {};
    }

    Iterator begin() const
    {
        return Iterator(orderedFactories.begin());
    }
    Iterator end() const
    {
        return Iterator(orderedFactories.end());
    }

    const IFactoryInfo& operator[](size_t index) const
    {
        return index < orderedFactories.size() ? *orderedFactories[index].second : *orderedFactories.front().second;
    }

    CoreRegistry();
    const std::string& getCoresCombo() const { return coresCombo; }
    const std::set<std::string>& getSupportedExtensions() const { return supportedExtensions; }

private:
    std::string coresCombo{};
    std::set<std::string> supportedExtensions{};
    mutable std::vector<Iterator::value_type> orderedFactories;
    static FactoryMap& factoryMap();
};

} // emu

