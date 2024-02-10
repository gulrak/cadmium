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

#include <algorithm>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <tuple>
#include <chiplet/utility.hpp>

#include <nlohmann/json_fwd.hpp>

namespace emu {

template<class... Ts> struct visitor : Ts... { using Ts::operator()...;  };
template<class... Ts> visitor(Ts...) -> visitor<Ts...>;

class Property
{
public:
    struct Combo {
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
            for(size_t i = 0; i < options.size(); ++i) {
                if (options[i] == text)
                    index = i;
            }
        }
        int index{};
        std::vector<std::string> options;
        std::string rgCombo;
    };
    struct Integer {
        int intValue{};
        int minValue{};
        int maxValue{};
    };
    using Value = std::variant<nullptr_t,bool,Integer,std::string,Combo>;

    Property(std::string name, Value val, std::string additionalInfo, bool isReadOnly = true)
    : _name(name)
    , _value(val)
    , _additionalInfo(additionalInfo)
    , _isReadonly(isReadOnly)
    {}
    Property(std::string name, Value val, bool isReadOnly = true)
        : _name(name)
        , _value(val)
        , _isReadonly(isReadOnly)
    {}
    Property(const Property& other)
        : _name(other._name)
        , _value(other._value)
        , _additionalInfo(other._additionalInfo)
        , _isReadonly(other._isReadonly)
    {
    }
    const std::string& getName() const { return _name; }
    const std::string& getAdditionalInfo() const { return _additionalInfo; }
    void setAdditionalInfo(std::string info) { _additionalInfo = std::move(info); }
    bool isReadonly() const { return _isReadonly; }
    Value& getValue() { return _value; }
    const Value& getValue() const { return _value; }
    bool getBool() const { return std::get<bool>(_value); }
    void setBool(bool val) { std::get<bool>(_value) = val; }
    int getInt() const { return std::get<Integer>(_value).intValue; }
    int getIntMin() const { return std::get<Integer>(_value).minValue; }
    int getIntMax() const { return std::get<Integer>(_value).maxValue; }
    void setInt(int val) { std::get<Integer>(_value).intValue = val; }
    std::string& getStringRef() { return std::get<std::string>(_value); }
    const std::string& getString() const { return std::get<std::string>(_value); }
    void setString(const std::string& val) { std::get<std::string>(_value) = val; }
    const std::string& getSelectedText() const { return std::get<Combo>(_value).selectedText(); }
    void setSelectedIndex(size_t idx) { std::get<Combo>(_value).index = idx; }
    void setSelectedText(const std::string& val) { std::get<Combo>(_value).setSelectedToText(val); }
    Property& operator=(const Property& other)
    {
        if(_value.index() != other._value.index())
            return *this;
        std::visit(emu::visitor{
                  [&](nullptr_t) { },
                  [&](bool& val) { val = std::get<bool>(other._value); },
                  [&](Integer& val) { val.intValue = std::get<Integer>(other._value).intValue; },
                  [&](std::string& val) { val = std::get<std::string>(other._value); },
                  [&](emu::Property::Combo& val) { val.index = std::get<Combo>(other._value).index; }
              }, _value);
        return *this;
    }
    bool operator==(const Property& other) const
    {
        if(_value.index() != other._value.index())
            return false;
        return std::visit(emu::visitor{
                       [&](nullptr_t) { return true; },
                       [&](bool val) { return val == std::get<bool>(other._value); },
                       [&](const Integer& val) { return val.intValue == std::get<Integer>(other._value).intValue; },
                       [&](const std::string& val) { return val == std::get<std::string>(other._value); },
                       [&](const emu::Property::Combo& val) { return val.index == std::get<Combo>(other._value).index; }
                   }, _value);
    }
private:
    std::string _name;
    Value _value;
    std::string _additionalInfo;
    bool _isReadonly{true};
};

class Properties {
public:
    Properties() = default;
    Properties(const Properties&) = delete;
    void registerProperty(const Property& prop)
    {
        auto iter = _valueMap.find(prop.getName());
        if(iter == _valueMap.end()) {
            registerKey(prop.getName());
            _valueList.push_back(prop.getName());
            _valueMap.emplace(prop.getName(), prop);
        }
    }
    size_t numProperties() const { return _valueList.size(); }
    void copyInto(Properties& props) const
    {
        if(_valueList.empty()) {
            if(props._valueList.empty()) {
                // from a passive copy into another passive copy
                props._valueMap = _valueMap;
            }
            else {
                // from a passive copy into an active property set
                for(auto iterDest = props._valueMap.begin(); iterDest != props._valueMap.cend(); ++iterDest) {
                    auto iterSrc = _valueMap.find(iterDest->first);
                    if(iterSrc != _valueMap.end()) {
                        iterDest->second = iterSrc->second;
                    }
                }
            }
        }
        else {
            // from an active property set to a passive copy
            if(!props._valueList.empty())
                throw std::runtime_error("Tried to overwrite an active property set with another active one!");
            props._valueMap = _valueMap;
        }
        props._valueList = _valueList;
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
        for(auto iter = _valueMap.begin(); iter != _valueMap.cend(); ++iter, ++iterM) {
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
    bool isReadonly(size_t index) const
    {
        auto iter = _valueMap.find(_valueList[index]);
        return iter == _valueMap.end() ? true : iter->second.isReadonly();
    }
    Property& operator[](const std::string& key)
    {
        auto iter = _valueMap.find(key);
        if(iter == _valueMap.end()) {
            throw std::runtime_error("No property named " + key);
        }
        return iter->second;
    }
    inline static Properties& getProperties(std::string_view key)
    {
        auto iter = propertyRegistry.find(key);
        if(iter == propertyRegistry.end()) {
            iter = propertyRegistry.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple()).first;
        }
        return iter->second;
    }
    inline static std::string makeJsonKey(std::string name)
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
    inline static void registerKey(const std::string& key)
    {
        auto jsonKey = makeJsonKey(key);
        registeredKeys[key] = jsonKey;
        registeredJsonKeys[jsonKey] = key;
    }
private:
    std::vector<std::string> _valueList;
    std::map<std::string, Property> _valueMap;
    static std::map<std::string_view,Properties> propertyRegistry;
    static std::unordered_map<std::string,std::string> registeredKeys;
    static std::unordered_map<std::string,std::string> registeredJsonKeys;
};

void to_json(nlohmann::json& j, const Properties& cc);
void from_json(const nlohmann::json& j, Properties& cc);

}
