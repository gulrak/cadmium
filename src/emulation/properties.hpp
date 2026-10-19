//---------------------------------------------------------------------------------------
// src/emulation/properties.hpp
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

#include <emulation/palette.hpp>
#include <chiplet/utility.hpp>

#include <algorithm>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <variant>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "ghc/bitenum.hpp"

namespace emu {

template<class... Ts>

struct visitor : Ts... { using Ts::operator()...;  };
template<class... Ts>

visitor(Ts...) -> visitor<Ts...>;

enum class PropertyFlags { eNone = 0, eWritable = 1, eInvisible = 2, eNeedsRelaunch = 4 };

// Whether a property matters, decided by the rules of its core (see CoreRegistry::applyRules). It is derived from
// the values, so it is neither saved nor compared.
enum class PropertyState {
    ACTIVE,          // has an effect
    INACTIVE,        // has no effect with the current values of other properties, keeps its value
    NOT_APPLICABLE   // makes no sense for the variant, is hidden and kept at the preset default
};

// Ids of properties several cores share and that the host and the UI use directly.
namespace propid {
inline constexpr std::string_view TRACE_LOG{"traceLog"};
inline constexpr std::string_view INSTRUCTIONS_PER_FRAME{"instructionsPerFrame"};
inline constexpr std::string_view FRAME_RATE{"frameRate"};
inline constexpr std::string_view START_ADDRESS{"startAddress"};
}
GHC_ENUM_ENABLE_BIT_OPERATIONS(PropertyFlags)

class Property
{
public:
    struct Combo
    {
        Combo(std::initializer_list<std::string> opts)
        : options{opts}
        {
            rgCombo = join(options.begin(), options.end(), ";");
        }
        const std::string& selectedText() const
        {
            if(index < options.size())
                return options[index];
            static std::string emptyString{};
            return emptyString;
        }
        // Selects the first option matching the text, keeps the current selection if there is none.
        void setSelectedToText(const std::string& text)
        {
            for(size_t i = 0; i < options.size(); ++i) {
                if (fuzzyCompare(options[i], text)) {
                    index = static_cast<int>(i);
                    return;
                }
            }
        }
        int index{};
        std::vector<std::string> options;
        std::string rgCombo;
    };

    struct Integer
    {

        int intValue{};

        int minValue{std::numeric_limits<int>::min()};

        int maxValue{std::numeric_limits<int>::max()};
    };
    using Value = std::variant<std::nullptr_t,bool,Integer,std::string,Combo>;

    // The id is the stable technical name: key in Properties, key in JSON (configs, ROM database) and base of the
    // CLI option. It must never change. The label is only shown to the user and can be changed at any time.
    Property(std::string id, std::string label, Value val, std::string description, PropertyFlags flags_ = PropertyFlags::eNone);
    Property(std::string id, std::string label, Value val, PropertyFlags flags_);  // description is the label
    Property(const Property& other);
    // An entry without a value that separates groups of properties in the UI.
    static Property separator() { return {"", "", nullptr, ""}; }
    const std::string& getId() const { return _id; }
    const std::string& getLabel() const { return _label; }
    const std::string& getOptionName() const { return _optionName; }
    // Only for CLI options that already existed with a name toOptionName(id) doesn't produce.
    Property& setOptionName(std::string optionName) { _optionName = std::move(optionName); return *this; }
    const std::string& getDescription() const { return _description; }
    const std::string& getAdditionalInfo() const { return _additionalInfo; }
    void setAdditionalInfo(std::string info) { _additionalInfo = std::move(info); }
    PropertyFlags getFlags() const { return _flags; }
    bool hasFlags(const PropertyFlags flags) const { return (_flags & flags) == flags; }
    PropertyState getState() const { return _state; }
    void setState(PropertyState state) { _state = state; }
    bool isActive() const { return _state == PropertyState::ACTIVE; }
    bool isApplicable() const { return _state != PropertyState::NOT_APPLICABLE; }
    bool isValid() const { return !std::holds_alternative<std::nullptr_t>(_value); }
    explicit operator bool() const { return isValid(); }
    Value& getValue() { return _value; }
    Value& getValueNC() { return _value; }
    const Value& getValue() const { return _value; }
    bool getBool() const { return std::get<bool>(_value); }
    void setBool(bool val) { std::get<bool>(_value) = val; }
    int getInt() const { return std::get<Integer>(_value).intValue; }
    int& getIntRef() { return std::get<Integer>(_value).intValue; }
    int getIntMin() const { return std::get<Integer>(_value).minValue; }
    int getIntMax() const { return std::get<Integer>(_value).maxValue; }
    void setInt(int val) { std::get<Integer>(_value).intValue = std::clamp(val, std::get<Integer>(_value).minValue, std::get<Integer>(_value).maxValue); }
    std::string& getStringRef() { return std::get<std::string>(_value); }
    const std::string& getString() const { return std::get<std::string>(_value); }
    void setString(const std::string& val) { std::get<std::string>(_value) = val; }
    const std::string& getSelectedText() const { return std::get<Combo>(_value).selectedText(); }
    size_t getSelectedIndex() const { return std::get<Combo>(_value).index; }
    int& getSelectedIndexRef() { return std::get<Combo>(_value).index; }
    void setSelectedIndex(size_t idx)
    {
        auto& combo = std::get<Combo>(_value);
        if (!combo.options.empty())
            combo.index = static_cast<int>(std::min(idx, combo.options.size() - 1));
    }
    void setSelectedText(const std::string& val) { std::get<Combo>(_value).setSelectedToText(val); }
    bool operator==(const Property& other) const
    {
        if(_value.index() != other._value.index())
            return false;
        return std::visit(emu::visitor{
                       [&](std::nullptr_t) { return true; },
                       [&](bool val) { return val == std::get<bool>(other._value); },
                       [&](const Integer& val) { return val.intValue == std::get<Integer>(other._value).intValue; },
                       [&](const std::string& val) { return val == std::get<std::string>(other._value); },
                       [&](const emu::Property::Combo& val) { return val.index == std::get<Combo>(other._value).index; }
                   }, _value);
    }

private:
    std::string _id;
    std::string _label;
    std::string _optionName;
    Value _value;
    std::string _description;
    std::string _additionalInfo;
    PropertyFlags _flags{PropertyFlags::eNone};
    PropertyState _state{PropertyState::ACTIVE};
};

class Properties
{
    struct RegistryMaps
    {
        static std::map<std::string_view,Properties> propertyRegistry;
        static std::unordered_map<std::string,std::string> registeredKeys;
        static std::unordered_map<std::string,std::string> registeredJsonKeys;
    };
public:
    Properties() = default;
    explicit operator bool() const { return !_valueList.empty(); }
    const std::string& propertyClass() const { return _class; }
    void registerProperty(const Property& prop)
    {
        auto iter = _valueMap.find(prop.getId());
        if(iter == _valueMap.end()) {
            _valueList.push_back(prop.getId());
            _valueMap.emplace(prop.getId(), prop);
        }
    }
    size_t numProperties() const { return _valueList.size(); }

    Properties& operator=(const Properties& other)
    {
        if(&other != this) {

            if(_class == other._class) {
                for (auto& [key, val] : _valueMap) {
                    auto it = other._valueMap.find(key);
                    if (it != other._valueMap.end()) {
                        val = it->second;
                    }
                }
            }
            else {
                _class = other._class;
                _valueList = other._valueList;
                _valueMap = other._valueMap;
            }
            _palette = other._palette;
        }
        return *this;
    }
    bool operator==(const Properties& other) const
    {
        return _valueMap == other._valueMap;
    }
    bool operator!=(const Properties& other) const
    {
        return _valueMap != other._valueMap;
    }
    // The first property that differs from the memento (or doesn't exist there).
    Property* changedProperty(const Properties& memento)
    {
        for(auto& [key, prop] : _valueMap) {
            auto iter = memento._valueMap.find(key);
            if(iter == memento._valueMap.end() || !(prop == iter->second)) {
                return &prop;
            }
        }
        return nullptr;
    }
    Property& operator[](size_t index) { return operator[](_valueList[index]); }
    const Property& operator[](size_t index) const
    {
        auto iter = _valueMap.find(_valueList[index]);
        if(iter == _valueMap.end()) {
            throw std::runtime_error("No property named " + _valueList[index]);
        }
        return iter->second;
    }
    PropertyFlags getFlags(size_t index) const
    {
        auto iter = _valueMap.find(_valueList[index]);
        return iter == _valueMap.end() ? PropertyFlags::eInvisible : iter->second.getFlags();
    }
    Property& operator[](const std::string& key)
    {
        auto iter = _valueMap.find(key);
        if(iter == _valueMap.end()) {
            throw std::runtime_error("No property named " + key);
        }
        return iter->second;
    }
    const Property& operator[](const std::string& key) const
    {
        auto iter = _valueMap.find(key);
        if(iter == _valueMap.end()) {
            throw std::runtime_error("No property named " + key);
        }
        return iter->second;
    }
    // Lookups by id are exact, code always uses ids.
    Property* find(std::string_view id)
    {
        auto iter = _valueMap.find(id);
        return iter == _valueMap.end() ? nullptr : &iter->second;
    }
    const Property* find(std::string_view id) const
    {
        auto iter = _valueMap.find(id);
        return iter == _valueMap.end() ? nullptr : &iter->second;
    }
    // Only for text typed by people (or written by hand): matches ids and labels, ignoring case and punctuation.
    Property* findFuzzy(std::string_view text)
    {
        if (auto* prop = find(text))
            return prop;
        for(auto& [id, prop] : _valueMap) {
            if(!id.empty() && (fuzzyCompare(id, text) || fuzzyCompare(prop.getLabel(), text))) {
                return &prop;
            }
        }
        return nullptr;
    }
    Property& at(std::string_view id)
    {
        if (auto* prop = find(id)) {
            return *prop;
        }
        throw std::runtime_error(fmt::format("No property with id '{}'", id));
    }
    const Property& at(std::string_view id) const
    {
        if (const auto* prop = find(id)) {
            return *prop;
        }
        throw std::runtime_error(fmt::format("No property with id '{}'", id));
    }
    template<typename T>
    std::optional<T> get(std::string_view id) const
    {
        if (const auto* prop = find(id); prop && std::holds_alternative<T>(prop->getValue())) {
            return std::get<T>(prop->getValue());
        }
        return {};
    }
    bool contains(std::string_view id) const
    {
        return find(id) != nullptr;
    }
    static Properties& getProperties(std::string_view key)
    {
        auto iter = propertyRegistry.find(key);
        if(iter == propertyRegistry.end()) {
            iter = propertyRegistry.emplace(std::piecewise_construct, std::forward_as_tuple(std::string(key)), std::forward_as_tuple()).first;
            iter->second._class = key;
        }
        return iter->second;
    }
    Palette& palette() { return _palette; }
    const Palette& palette() const { return _palette; }

    static RegistryMaps& getRegistryMaps();

    nlohmann::json createDiff(const Properties& other) const;
    void applyDiff(const nlohmann::json& diff);

private:
    std::string _class;
    std::vector<std::string> _valueList;
    std::map<std::string, Property, std::less<>> _valueMap;
    Palette _palette;
    // Owns its keys, callers may pass a temporary string (e.g. a class name read from JSON).
    static std::map<std::string,Properties,std::less<>> propertyRegistry;
    static std::unordered_map<std::string,std::string> registeredKeys;
    static std::unordered_map<std::string,std::string> registeredJsonKeys;
};

void to_json(nlohmann::json& j, const Properties& props);
void from_json(const nlohmann::json& j, Properties& props);
void to_json(nlohmann::json& j, const Palette& pal);
void from_json(const nlohmann::json& j, Palette& pal);

}
