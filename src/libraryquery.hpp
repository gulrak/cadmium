//---------------------------------------------------------------------------------------
// src/libraryquery.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2026, Steffen Schümann <s.schuemann@pobox.com>
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

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// What the library search knows about a config of a ROM: its variant and the effective values of its properties
// (the preset's with the config's changes), by lowercase property name, as text (true/false for flags).
struct ConfigFacts
{
    std::string preset;
    std::map<std::string, std::string> properties;
};

// What the library search knows about a ROM.
struct RomFacts
{
    std::string sha1;
    int size{0};
    std::vector<std::string> files;
    std::vector<std::string> tags;  // its own, not the ones of its program
    bool present{false};  // found in the library folders, not only known from metadata
    bool analyzed{false};  // its opcodes are known, from its data or from the starter metadata
    std::vector<uint16_t> opcodes;  // the distinct opcodes on its execution paths, sorted
    std::vector<ConfigFacts> configs;
};

// What the library search knows about a program.
struct ProgramFacts
{
    std::string name;
    std::string description;
    std::vector<std::string> authors;
    std::string release;  // a date like 1978, 1978-05 or 1978-05-12, unknown digits of the year as x or ?
    std::string origin;
    std::string originReference;
    std::string license;
    std::string copyright;
    std::string url;
    std::vector<std::string> tags;  // its own, not the ones of its ROMs
    bool present{false};  // at least one of its ROMs was found in the library folders, used without ROMs
    std::vector<RomFacts> roms;
};

// The search of the library, a small expression language in the spirit of an SQL WHERE clause:
//
//   space invaders              words are searched in name, description and authors, adjacent words as one phrase
//   "space invaders"            quotes keep words together, a quoted word is never a keyword or field
//   release < 1980              a field, an operator (= != < <= > >= ~) and a value, ~ means "contains"
//   release 1990..1999          a range of dates, open ranges like ..1979 or 1990.. work too
//   author = "John Earnest"     quotes for values with spaces, comparisons ignore case
//   tag = none                  none is the empty value: no tag, no release...
//   missing, present            ROMs not found/found in the library folders
//   uses F?75, uses 8xy6        ROMs using an opcode, hex digits have to match, other characters are wildcards
//   analyzed                    ROMs whose opcodes are known, "-uses" alone also finds ROMs never analyzed
//   a b, a and b, a or b        juxtaposition means and, "and" binds stronger than "or"
//   not a, -a, ( ... )          negation and grouping
//
// Fields of a program: name (title), description (desc), author (by), release (year, date), origin, jam (ref,
// reference), license, copyright, url. Fields of a ROM: size (with k for 1024 bytes), sha1 (a prefix with =), file,
// tag (the tags of the ROM together with the ones of its program). Fields of a config of a ROM: variant (preset) and the properties of the cores by their name (like
// wrapSprites or instructionsPerFrame), which the caller passes to parse(), with the quirk names of the
// chip-8-database as aliases where it adds them.
//
// A query is evaluated for each ROM of a program and each config of the ROM, a program matches if one of these
// combinations does. So "variant = xo-chip and wrapSprites = false" needs one config with both, and a negation
// like "-variant = xo-chip" matches a program that has another variant too, and "-tag = bad" one with another ROM
// that isn't bad. matchingRoms() tells which ROMs match.
//
// Partial dates are ranges, 197x is 1970 to 1979. A comparison holds only if it holds for the whole range of
// the release, so "release < 1980" doesn't match 198x. Programs without a release match none of them.
class LibraryQuery
{
public:
    // A query that can't be parsed, position is the offset of the problem in the text.
    class Error : public std::runtime_error
    {
    public:
        Error(const std::string& message, size_t position)
            : std::runtime_error(message)
            , _position(position)
        {
        }
        size_t position() const { return _position; }
    private:
        size_t _position;
    };

    LibraryQuery() = default;
    // Throws LibraryQuery::Error for a query that can't be parsed, an empty text matches everything. The
    // properties are the lowercase names usable as config fields.
    static LibraryQuery parse(std::string_view text, const std::set<std::string>& properties = {});
    // A value in quotes for a query, so it is never taken as a keyword or as none. Throws LibraryQuery::Error for
    // a value with a quote, as quotes can't be escaped.
    static std::string quoted(std::string_view value);
    // The queries as one query that matches where all of them match, empty ones are left out.
    static std::string combine(const std::vector<std::string>& queries);
    bool empty() const { return !_root; }
    bool matches(const ProgramFacts& program) const;
    // the indices of the ROMs of the program that match, all of them for a query without ROM or config fields
    std::vector<size_t> matchingRoms(const ProgramFacts& program) const;
    // whether the query asks for present or missing ROMs itself
    bool usesPresence() const { return _usesPresence; }
    // whether the query has fields of ROMs or configs, so not all ROMs of a matching program match
    bool usesRomFields() const { return _usesRomFields; }

    struct Node;

private:
    bool matchesRom(const ProgramFacts& program, const RomFacts& rom) const;

    std::shared_ptr<const Node> _root;
    bool _usesPresence{false};
    bool _usesRomFields{false};
};
