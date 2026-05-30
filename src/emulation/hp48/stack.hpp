//
// Created by Steffen Schümann on 18.02.26.
//
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <variant>
#include <vector>


namespace hp48 {

// If you later add bignum: boost::multiprecision::cpp_int is a common choice.
using Int   = std::int64_t;   // start simple; upgrade later
using Real  = double;

struct Symbol { std::string name; };
struct Program { std::vector<std::string> tokens; }; // placeholder
struct List;

enum class BlobKind { Raw, RplObject, SaturnCode, Unknown };
struct Blob {
    BlobKind kind{BlobKind::Unknown};
    std::shared_ptr<const uint8_t[]> data;
    std::size_t size = 0;

    std::span<const uint8_t> view() const { return {data.get(), size}; }

    static Blob from_vector(std::vector<uint8_t> v, BlobKind kind = BlobKind::Raw) {
        auto p = std::shared_ptr<uint8_t[]>(new uint8_t[v.size()], std::default_delete<uint8_t[]>());
        std::memcpy(p.get(), v.data(), v.size());
        return Blob{kind, std::shared_ptr<const uint8_t[]>(p), v.size()};
    }
};

struct ObjectWrapper;
using ListData = std::vector<ObjectWrapper>;

struct List { ListData items; };

using Object = std::variant<
    Int,
    Real,
    std::string,
    Symbol,
    Blob,
    List
>;

struct ObjectWrapper { Object obj; };

template <class T>
class Stack
{
public:
    // level 1 = top of stack = back()
    void push(T v) { _data.push_back(std::move(v)); }

    T pop()
    {
        auto v = std::move(_data.back());
        _data.pop_back();
        return v;
    }

    // 1-based: level(1) is top, level(n) is deeper
    T& level(std::size_t n) { return _data.at(_data.size() - n); }

    const T& level(std::size_t n) const { return _data.at(_data.size() - n); }

    std::size_t size() const { return _data.size(); }

    // "PICK n": copy level(n) onto top
    void pick(std::size_t n) { push(level(n)); }

    // "ROLL n": bring level(n) to top, shifting others down
    void roll(std::size_t n)
    {
        auto it = _data.end() - static_cast<std::ptrdiff_t>(n);
        std::rotate(it, it + 1, _data.end());  // move *it to end
    }

    // "ROLLD n": move top to level(n)
    void rolld(std::size_t n)
    {
        auto it = _data.end() - static_cast<std::ptrdiff_t>(n);
        std::rotate(it, _data.end() - 1, _data.end());
    }

private:
    std::vector<T> _data;
};

using ObjectStack = Stack<Object>;

}  // namespace hp48
