//---------------------------------------------------------------------------------------
// src/cli.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2020, Steffen Schümann <s.schuemann@pobox.com>
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

#include <cstdlib>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace ghc
{

template<class... Ts> struct visitor : Ts... { using Ts::operator()...;  };
template<class... Ts> visitor(Ts...) -> visitor<Ts...>;

class CLI
{
public:
    struct Combo
    {
        int index;
        std::vector<std::string> combinations;
    };

private:
    using Value = std::variant<bool, int, std::int64_t, std::string, std::vector<std::string>, Combo>;
    using ValuePtr = std::variant<bool*, int*, std::int64_t*, std::string*, std::vector<std::string>*, Combo*>;
    template <class T, class TL>
    struct Contains;
    template <typename T, template <class...> class Container, class... Ts>
    struct Contains<T, Container<Ts...>> : std::disjunction<std::is_same<T, Ts>...>
    {
    };

public:
    struct Info
    {
        ValuePtr valPtr;
        std::function<void(std::string, std::string, ValuePtr)> converter;
        std::string help;
        std::string category;
        std::function<void(std::string)> triggerCallback;
        std::function<bool()> condition;
        Info& dependsOn(std::function<bool()> dependCondition)
        {
            condition = std::move(dependCondition);
            return *this;
        }
    };
    CLI(int argc, char* argv[])
    {
        for (size_t i = 0; i < argc; ++i) {
            argList.emplace_back(argv[i]);
        }
    }
    std::string category(std::string cat)
    {
        auto prevCat = currentCategory;
        currentCategory = std::move(cat);
        if (std::find(categories.begin(), categories.end(), currentCategory) == categories.end()) {
            categories.push_back(currentCategory);
        }
        return prevCat;
    }
    template <typename T>
    Info& option(const std::vector<std::string>& names, T& destVal, std::string description = {}, std::function<void(std::string)> trigger = {})
    {
        static_assert(Contains<T*, ValuePtr>::value, "CLI: supported option types are only bool, int, std::int64_t, std::string or std::vector<std::string>");
        auto iter = handler.insert({names,
                                    {&destVal,
                                     [](const std::string& name, const std::string& arg, ValuePtr valp) {
                                         std::visit(visitor{[name, arg](bool* val) { *val = !*val; },
                                                            [name, arg](int* val) {
                                                                errno = 0;
                                                                char* endPtr{};
                                                                *val = static_cast<int>(std::strtol(arg.c_str(), &endPtr, 0));
                                                                if (errno > 0 || endPtr != arg.c_str() + arg.length()) {
                                                                    throw std::runtime_error("Conversion error for option " + name);
                                                                }
                                                            },
                                                            [name, arg](std::int64_t* val) {
                                                                errno = 0;
                                                                char* endPtr{};
                                                                *val = std::strtoll(arg.c_str(), &endPtr, 0);
                                                                if (errno > 0 || endPtr != arg.c_str() + arg.length()) {
                                                                    throw std::runtime_error("Conversion error for option " + name);
                                                                }
                                                            },
                                                            [name, arg](std::string* val) { *val = arg; }, [name, arg](std::vector<std::string>* val) { val->push_back(arg); },
                                                            [name, arg](Combo* val) {
                                                                int idx = 0;
                                                                for (const auto& alt : val->combinations) {
                                                                    if (compareFuzzy(alt, arg)) {
                                                                        val->index = idx;
                                                                        return;
                                                                    }
                                                                    ++idx;
                                                                }
                                                                throw std::runtime_error("Invalid alternative '" + arg + "' for option " + name);
                                                            }

                                                    },
                                                    valp);
                                     },
                                     description,
                                     currentCategory,
                                     trigger,
                                     {}}});
        return iter->second;
    }
    template<typename T>
    Info& option(const std::vector<std::string>& names, std::function<void(std::string, T&)> callback, std::string description = {})
    {
        callbackArgs.push_back(T{});
        auto& val = std::get<T>(callbackArgs.back());
        option(names, val, description, [callback,&val](std::string name){ callback(name, val); });
    }
    void positional(std::vector<std::string>& dest, std::string description = std::string())
    {
        positionalArgs = &dest;
        positionalHelp = std::move(description);
    }
    bool parse()
    {
        auto iter = ++argList.begin();
        while(iter != argList.end()) {
            if(!handleOption(iter)) {
                if(positionalArgs && iter->at(0) != '-') {
                    positionalArgs->push_back(*iter++);
                }
                else {
                    if(conditionFailed)
                        throw std::runtime_error("Unexpected argument " + *iter + ", a needed dependency was not met");
                    else
                        throw std::runtime_error("Unknown argument " + *iter);
                }
            }
        }
        return false;
    }
    void usage()
    {
        std::cout << "USAGE: " << argList[0] << " [options]" << (positionalArgs ? " ..." : "") << std::endl;
        std::cout << "OPTIONS:\n" << std::endl;
        //std::sort(categories.begin(), categories.end());
        for(const auto& category : categories) {
            if(categories.size() > 1) std::cout << category << ":" << std::endl;
            for(const auto& [names, info] : handler) {
                if(info.category == category) {
                    std::string delimiter = "  ";
                    for(const auto& name : names) {
                        std::cout << delimiter << name;
                        if(info.valPtr.index()) {
                            std::cout << " <arg>";
                        }
                        delimiter = ", ";
                    }
                    std::cout << std::endl << "    " << info.help << "\n" << std::endl;
                }
            }
        }
        if(positionalArgs)
            std::cout << "...\n    " << positionalHelp << "\n" << std::endl;
    }
private:
    bool handleOption(std::vector<std::string>::iterator& iter)
    {
        static const std::map<std::string,bool> boolKeys = {{"true", true}, {"false", false}, {"yes", true}, {"no", false}, {"on", true}, {"off", false}};
        if(*iter == "-?" || *iter == "-h" || *iter == "--help") {
            usage();
            exit(1);
        }
        conditionFailed = false;
        for(const auto& [names, info] : handler) {
            for(const auto& name : names) {
                if(name == *iter) {
                    if(info.condition && !info.condition()) {
                        conditionFailed = true;
                        continue;
                    }
                    ++iter;
                    if(info.valPtr.index()) {
                        if(iter == argList.end()) {
                            throw std::runtime_error("Missing argument to option " + name);
                        }
                        errno = 0;
                        info.converter(name, *iter++, info.valPtr);
                        if(errno) {
                            throw std::runtime_error("Conversion error for option " + name);
                        }
                    }
                    else if(iter != argList.end() && boolKeys.count(*iter)) {
                        *std::get<bool*>(info.valPtr) = boolKeys.at(*iter++);
                    }
                    else {
                        *std::get<bool*>(info.valPtr) = true;
                    }
                    if(info.triggerCallback)
                        info.triggerCallback(name);
                    return true;
                }
            }
        }
        return false;
    }
    static bool compareFuzzy(std::string_view s1, std::string_view s2)
    {
        auto iter1 = s1.begin();
        auto iter2 = s2.begin();
        while(iter1 != s1.end() && iter2 != s2.end()) {
            while(iter1 != s1.end() && !std::isalnum(*iter1)) ++iter1;
            while(iter2 != s2.end() && !std::isalnum(*iter2)) ++iter2;
            if(iter1 != s1.end() && iter2 == s2.end() && std::tolower(*iter1) != std::tolower(*iter2)) {
                return false;
            }
            if (iter1 != s1.end()) ++iter1;
            if (iter2 != s2.end()) ++iter2;
        }
        while (iter1 != s1.end() && !std::isalnum(*iter1)) ++iter1;
        while (iter2 != s2.end() && !std::isalnum(*iter2)) ++iter2;
        return iter1 == s1.end() && iter2 == s2.end();
    }
    std::vector<std::string> argList;
    std::multimap<std::vector<std::string>, Info> handler;
    std::list<Value> callbackArgs;
    std::vector<std::string>* positionalArgs{nullptr};
    std::vector<std::string> categories;
    std::string currentCategory;
    std::string positionalHelp;
    bool conditionFailed{false};
};

}
