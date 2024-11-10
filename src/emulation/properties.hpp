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

namespace emu {

template<class... Ts>

struct visitor : Ts... { using Ts::operator()...;  };
template<class... Ts>

visitor(Ts...) -> visitor<Ts...>;

enum PropertyAccess { eReadOnly, eWritable, eInvisible };


class Palette
{
public:
    struct Color {
        Color(const uint8_t rval, const uint8_t gval, const uint8_t bval) : r(rval), g(gval), b(bval) {}
        explicit Color(uint32_t val) : r(val >> 16), g(val >> 8), b(val) {}
        explicit Color(std::string_view hex)
        {
            if(hex.length()>1 && hex[0] == '#') {
                uint32_t colInt = std::strtoul(hex.data() + 1, nullptr, 16);
                r = colInt>>16;
                g = colInt>>8;
                b = colInt;
            }
            else {
                r = 0, g = 0, b = 0;
            }
        }
        uint32_t toRGBInt() const
        {
            return (r << 16u) | (g << 8u) | b;
        }
        uint32_t toRGBAInt(const uint8_t alpha = 255) const
        {
            return (r << 24u) | (g << 16u) | (b << 8) | alpha;
        }
        bool operator==(const Color& other) const
        {
            return r == other.r && g == other.g && b == other.b;
        }
        std::string toString() const;
        static Color fromRGBA(const uint32_t val) { return Color( val >> 24, val >> 16, val >> 8); }
        uint8_t r, g, b;
    };
    Palette() = default;
    Palette(const std::initializer_list<Color> cols)
        : colors(cols)
    {}
    Palette(const std::initializer_list<std::string> cols)
    {
        colors.reserve(cols.size());
        for(const auto& col : cols) {
            colors.emplace_back(col);
        }
    }

    bool empty() const { return colors.empty(); }
    size_t size() const { return colors.size(); }
    bool operator==(const Palette& other) const
    {
        return colors == other.colors && borderColor == other.borderColor && signalColor == other.signalColor;
    }
    std::vector<Color> colors;
    std::optional<Color> borderColor{};
    std::optional<Color> signalColor{};
};

class Property
{
public:
    struct NameAndKeyName
    {
        std::string name;
        std::string keyName;
    };
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
        void setSelectedToText(const std::string& text)
        {
            index = 0;
            for(int i = 0; i < options.size(); ++i) {
                if (fuzzyCompare(options[i],text))
                    index = i;
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

    Property(const std::string& name, Value val, std::string description, std::string additionalInfo, PropertyAccess access_ = eReadOnly);
    Property(const std::string& name, Value val, std::string description, PropertyAccess access_ = eReadOnly);
    Property(const NameAndKeyName& nameAndKey, Value val, std::string description, PropertyAccess access_ = eReadOnly);
    Property(const NameAndKeyName& nameAndKey, Value val, PropertyAccess access_ = eReadOnly);
    Property(const Property& other);
    const std::string& getName() const { return _name; }
    const std::string& getJsonKey() const { return _jsonKey; }
    const std::string& getOptionName() const { return _optionName; }
    void setJsonKey(const std::string& jsonKey) { _jsonKey = jsonKey; }
    const std::string& getDescription() const { return _description; }
    const std::string& getAdditionalInfo() const { return _additionalInfo; }
    void setAdditionalInfo(std::string info) { _additionalInfo = std::move(info); }
    PropertyAccess access() const { return _access; }
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
    void setSelectedIndex(size_t idx) { std::get<Combo>(_value).index = static_cast<int>(idx < std::get<Combo>(_value).options.size() ? idx : std::get<Combo>(_value).options.size() - 1); }
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
    std::string _name;
    std::string _jsonKey;
    std::string _optionName;
    Value _value;
    std::string _description;
    std::string _additionalInfo;
    PropertyAccess _access{eReadOnly};
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
        auto iter = _valueMap.find(prop.getName());
        if(iter == _valueMap.end()) {
            _valueList.push_back(prop.getName());
            _valueMap.emplace(prop.getName(), prop);
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
    Property* changedProperty(const Properties& memento)
    {
        auto iterM = memento._valueMap.cbegin();
        bool error = false;
        for(auto iter = _valueMap.begin(); iter != _valueMap.cend(); ++iter, ++iterM) {
            if(iter->first != iterM->first)
                error = true;
            if(!(iter->second == iterM->second)) {
                return &iter->second;
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
    PropertyAccess access(size_t index) const
    {
        auto iter = _valueMap.find(_valueList[index]);
        return iter == _valueMap.end() ? eInvisible : iter->second.access();
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
    Property& at(std::string_view key)
    {
        for(auto& [mapKey, val] : _valueMap) {
            if(fuzzyCompare(mapKey, key)) {
                return val;
            }
        }
        throw std::runtime_error(fmt::format("No property named {}", key));
    }
    const Property& at(std::string_view key) const
    {
        for(auto& [mapKey, val] : _valueMap) {
            if(fuzzyCompare(mapKey, key)) {
                return val;
            }
        }
        throw std::runtime_error(fmt::format("No property named {}", key));
    }
    template<typename T>
    std::optional<T> get(std::string_view key) const
    {
        for(auto& [mapKey, val] : _valueMap) {
            if(fuzzyCompare(mapKey, key)) {
                if(std::holds_alternative<T>(val.getValue())) {
                    return std::get<T>(val.getValue());
                }
                return {};
            }
        }
        return {};
    }
    bool contains(const std::string& key)
    {
        return _valueMap.find(key) != _valueMap.end();
    }
    bool containsFuzzy(std::string_view key)
    {
        return std::any_of(_valueMap.begin(), _valueMap.end(),
                       [&](const auto& pair) { return fuzzyCompare(pair.first, key); });
    }
    static Properties& getProperties(std::string_view key)
    {
        auto iter = propertyRegistry.find(key);
        if(iter == propertyRegistry.end()) {
            iter = propertyRegistry.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple()).first;
            iter->second._class = key;
        }
        return iter->second;
    }
    static std::string makeJsonKey(const std::string& name)
    {
        std::string key;
        bool nonChar = false;
        for(char c : name) {
            if(isalnum(c)) {
                key.push_back(nonChar && key.size() ? std::toupper(int(c)) : std::tolower(int(c)));
                nonChar = false;
            }
            else {
                nonChar = true;
            }
        }
        return key;
    }

    Palette& palette() { return _palette; }
    const Palette& palette() const { return _palette; }

    static RegistryMaps& getRegistryMaps();

    nlohmann::json createDiff(const Properties& other) const;
    void applyDiff(const nlohmann::json& diff);

private:
    std::string _class;
    std::vector<std::string> _valueList;
    std::map<std::string, Property> _valueMap;
    Palette _palette;
    static std::map<std::string_view,Properties> propertyRegistry;
    static std::unordered_map<std::string,std::string> registeredKeys;
    static std::unordered_map<std::string,std::string> registeredJsonKeys;
};

void to_json(nlohmann::json& j, const Properties& props);
void from_json(const nlohmann::json& j, Properties& props);
void to_json(nlohmann::json& j, const Palette& pal);
void from_json(const nlohmann::json& j, Palette& pal);

}
