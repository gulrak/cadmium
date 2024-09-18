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

#include "coreregistry.hpp"

#include <chiplet/utility.hpp>

#include <set>

namespace emu {

void CoreRegistry::IFactoryInfo::cacheVariantMappings() const
{
    if(presetMappings.empty()) {
        for (size_t i = 0; i < numberOfVariants(); ++i) {
            if(prefix().empty())
                presetMappings.emplace(toOptionName(variantName(i)), i);
            else
                presetMappings.emplace(toOptionName(prefix() + '-' + variantName(i)), i);
        }
    }
}

bool CoreRegistry::IFactoryInfo::hasVariant(const std::string& variant) const
{
    cacheVariantMappings();
    return presetMappings.find(toOptionName(variant)) != presetMappings.end();
}

Properties CoreRegistry::IFactoryInfo::variantProperties(const std::string& variant) const
{
    cacheVariantMappings();
    if(const auto iter = presetMappings.find(toOptionName(variant)); iter != presetMappings.end())
        return variantProperties(iter->second);
    return {};
}

CoreRegistry::CoreRegistry()
{
    for(auto& [key, info] : factoryMap()) {
        if(!coresCombo.empty())
            coresCombo += ";";
        coresCombo += key;
        if(info->variantsCombo.empty()) {
            for(size_t i = 0; i < info->numberOfVariants(); ++i) {
                if(!info->variantsCombo.empty()) {
                    info->variantsCombo += ";";
                }
                info->variantsCombo += info->variantName(i);
                auto defaultExtensions = split(info->variantExtensions(i), ';');
                supportedExtensions.insert(defaultExtensions.begin(), defaultExtensions.end());
            }
        }
        orderedFactories.emplace_back(key, info.get());
    }
    std::sort(orderedFactories.begin(), orderedFactories.end(), [](const auto& lhs, const auto& rhs) { return lhs.second->score < rhs.second->score; });
}

CoreRegistry::FactoryMap& CoreRegistry::factoryMap()
{
    static FactoryMap factories{};
    return factories;
}

} // emu
