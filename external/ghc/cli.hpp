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
#include <map>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace ghc
{

template<class... Ts> struct visitor : Ts... { using Ts::operator()...;  };
template<class... Ts> visitor(Ts...) -> visitor<Ts...>;

class CLI
{
    using ValuePtr = std::variant<bool*, std::int64_t*, std::string*, std::vector<std::string>*>;
    template<class T, class TL> struct Contains;
    template<typename T, template<class...> class Container, class... Ts>
    struct Contains<T, Container<Ts...>> : std::disjunction<std::is_same<T, Ts>...> {};
    struct Info
    {
        ValuePtr valPtr;
        std::function<void(std::string, ValuePtr)> converter;
        std::string help;
        std::function<void()> triggerCallback;
    };
public:
    CLI(int argc, char* argv[])
    {
        for(size_t i = 0; i < argc; ++i) {
            argList.emplace_back(argv[i]);
        }
    }
    template<typename T>
    void option(const std::vector<std::string>& names, T& destVal, std::string description = std::string(), std::function<void()> trigger = {})
    {
        static_assert(Contains<T*, ValuePtr>::value, "CLI: supported option types are only bool, std::int64_t, std::string or std::vector<std::string>");
        handler[names] = {&destVal,
                          [](const std::string& arg, ValuePtr valp) {
                              std::visit(visitor{[arg](bool* val) { *val = !*val; },
                                                 [arg](std::int64_t* val) { *val = std::strtoll(arg.c_str(), nullptr, 0); },
                                                 [arg](std::string* val) { *val = arg; },
                                                 [arg](std::vector<std::string>* val) { val->push_back(arg); }}, valp);
                          },
                          description, trigger};
    }
    void positional(std::vector<std::string>& dest, std::string description = std::string())
    {
        positionalArgs = &dest;
        positionalHelp = description;
    }
    bool parse()
    {
        auto iter = ++argList.begin();
        while(iter != argList.end()) {
            if(!handleOption(iter)) {
                if(positionalArgs && iter->at(0) != '-') {
                    positionalArgs->push_back(*iter++);
                }
                else
                    throw std::runtime_error("Unexpected argument " + *iter);
            }
        }
        return false;
    }
    void usage()
    {
        std::cout << "USAGE: " << argList[0] << " [options]\n" << std::endl;
        std::cout << "OPTIONS:\n" << std::endl;
        for(const auto& [names, info] : handler) {
            std::string delimiter = "";
            for(const auto& name : names) {
                std::cout << delimiter << name;
                if(info.valPtr.index()) {
                    std::cout << " <arg>";
                }
                delimiter = ", ";
            }
            std::cout << std::endl << "    " << info.help << "\n" << std::endl;
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
        for(const auto& [names, info] : handler) {
            for(const auto& name : names) {
                if(name == *iter) {
                    ++iter;
                    if(info.valPtr.index()) {
                        if(iter == argList.end()) {
                            throw std::runtime_error("Missing argument to option " + name);
                        }
                        errno = 0;
                        info.converter(*iter++, info.valPtr);
                        if(errno) {
                            throw std::runtime_error("Conversion error for option " + name);
                        }
                    }
                    else if(iter != argList.end() && boolKeys.count(*iter)) {
                        *std::get<bool*>(info.valPtr) = boolKeys.at(*iter++);
                    }
                    else {
                        std::cerr << "bool-arg: " << name << ", old value " << *std::get<bool*>(info.valPtr) << std::endl;
                        *std::get<bool*>(info.valPtr) = true;//(*std::get<bool*>(info.valPtr) == false);
                    }
                    if(info.triggerCallback)
                        info.triggerCallback();
                    return true;
                }
            }
        }
        return false;
    }
    std::vector<std::string> argList;
    std::map<std::vector<std::string>, Info> handler;
    std::vector<std::string>* positionalArgs{nullptr};
    std::string positionalHelp;
};

}
