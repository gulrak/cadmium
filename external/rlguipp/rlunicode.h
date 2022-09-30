//-----------------------------------------------------------------------------
//   Raylib complementing Unicode conversion helpers
//-----------------------------------------------------------------------------
//   LICENSE: zlib/libpng
//
//   Copyright (c) 2022 Steffen Schümann (@gulrak)
//
//   This software is provided "as-is", without any express or implied
//   warranty. In no event will the authors be held liable for any damages
//   arising from the use of this software.
//
//   Permission is granted to anyone to use this software for any purpose,
//   including commercial applications, and to alter it and redistribute it
//   freely, subject to the following restrictions:
//
//     1. The origin of this software must not be misrepresented; you must not
//     claim that you wrote the original software. If you use this software in
//     a product, an acknowledgment in the product documentation would be
//     appreciated but is not required.
//
//     2. Altered source versions must be plainly marked as such, and must not
//     be misrepresented as being the original software.
//
//     3. This notice may not be removed or altered from any source
//     distribution.
//-----------------------------------------------------------------------------
//
// Note: This file needs to be included with RLUNICODE_IMPLEMENTATION set in exactly one c/cpp.
//
//-----------------------------------------------------------------------------
#ifndef RLUNICODE_H
#define RLUNICODE_H

#include <raylib.h>
#include <stdint.h>
#include <string.h>

#if defined(_WIN32) && defined(BUILD_LIBTYPE_SHARED)
#  define RLUC_API __declspec(dllexport) extern inline
#elif defined(_WIN32) && defined(USE_LIBTYPE_SHARED)
#  define RLUC_API __declspec(dllimport)
#endif
#ifndef RLUC_API
# define RLUC_API
#endif

RLUC_API int GetCodepointFromUTF8(const char *text, int *bytesProcessed); // Fetch one unicode codepoint from zero terminated utf8 string (forwards to GetCodepoint() from raylib)
RLUC_API int GetCodepointCountUTF8(const char *text); // Count codepoints in a zero terminated UTF8 c-string (just forwards to GetCodepointCount() from raylib)
RLUC_API int GetCodepointFromUTF16(const uint16_t *text, int *wordsProcessed); // Fetch one unicode codepoint from zero terminated UTF16 string
RLUC_API int GetCodepointCountUTF16(const uint16_t *text);  // Count codepoints in a zero terminated UTF16 c-string
RLUC_API int GetCodepointFromUTF32(const uint32_t *text); // Fetch one unicode codepoint from zero terminated UTF16 string
RLUC_API int GetCodepointCountUTF32(const uint32_t *text); // Count codepoints in a zero terminated UTF32 c-string
RLUC_API int ConvertUTF8ToUTF16(const char* text, uint16_t* out, int inputLength); // Convert string from UTF8 to UTF16, or calculate buffer needed if out is NULL, looks for zero-termination if inputLength is 0
RLUC_API int ConvertUTF16ToUTF8(const uint16_t* text, char* out, int inputLength); // Convert string from UTF16 to UTF8, or calculate buffer needed if out is NULL, looks for zero-termination if inputLength is 0
RLUC_API int ConvertUTF8ToUTF32(const char* text, uint32_t* out, int inputLength); // Convert string from UTF8 to UTF32, or calculate buffer needed if out is NULL, looks for zero-termination if inputLength is 0
RLUC_API int ConvertUTF32ToUTF8(const uint32_t* text, char* out, int inputLength); // Convert string from UTF32 to UTF8, or calculate buffer needed if out is NULL, looks for zero-termination if inputLength is 0
RLUC_API int ConvertUTF16ToUTF32(const uint16_t* text, uint32_t* out, int inputLength); // Convert string from UTF16 to UTF32, or calculate buffer needed if out is NULL, looks for zero-termination if inputLength is 0
RLUC_API int ConvertUTF32ToUTF16(const uint32_t* text, uint16_t* out, int inputLength); // Convert string from UTF32 to UTF16, or calculate buffer needed if out is NULL, looks for zero-termination if inputLength is 0

#endif // RLUNICODE_H

#ifdef RLUNICODE_IMPLEMENTATION
bool RlucInRange(uint32_t c, uint32_t lo, uint32_t hi)
{
    return ((uint32_t)(c - lo) < (hi - lo + 1));
}

bool RlucIsSurrogate(uint32_t c)
{
    return RlucInRange(c, 0xd800, 0xdfff);
}

bool RlucIsHighSurrogate(uint32_t c)
{
    return (c & 0xfffffc00) == 0xd800;
}

bool RlucIsLowSurrogate(uint32_t c)
{
    return (c & 0xfffffc00) == 0xdc00;
}

// This exists just for symmetry reasons
int GetCodepointFromUTF8(const char *text, int *bytesProcessed)
{
    return GetCodepoint(text, bytesProcessed);
}

// This exists just for symmetry reasons
int GetCodepointCountUTF8(const char *text)
{
    return GetCodepointCount(text);
}

int GetCodepointFromUTF16(const uint16_t *text, int *wordsProcessed)
{
    uint32_t c = *text;
    if(RlucIsSurrogate(c)) {
        ++text;
        if(*text && RlucIsHighSurrogate(c) && RlucIsLowSurrogate(*text)) {
            *wordsProcessed = 2;
            return ((uint32_t)c << 10) + *text - 0x35fdc00;
        }
        else {
            // not a valid codepoint, return replacement character
            *wordsProcessed = 1;
            return 0xfffd;
        }
    }
    else {
        *wordsProcessed = c ? 1 : 0;
        return c;
    }
}

int GetCodepointCountUTF16(const uint16_t *text)
{
    int length = 0;
    while(*text) {
        int size = 0;
        GetCodepointFromUTF16(text, &size);
        text += size;
        ++length;
    }
    return length;
}

int GetCodepointFromUTF32(const uint32_t *text)
{
    int c = *text;
    if(c > 0x10ffff || (c > 0xd800 && c <= 0xdfff) || ((c & 0xfffe) == 0xfffe)) {
        // not a valid codepoint, return replacement character
        return 0xfffd;
    }
    return *text;
}

int GetCodepointCountUTF32(const uint32_t *text)
{
    int length = 0;
    if(text) {
        while (*text++) {
            ++length;
        }
    }
    return length;
}

int ConvertUTF8ToUTF16(const char* text, uint16_t* out, int inputLength)
{
    const char* src = text;
    int outputLength = 0;
    while(*src && (!inputLength || src - text < inputLength))
    {
        int size = 0;
        int codepoint = GetCodepointFromUTF8(src, &size);
        if (codepoint <= 0xffff) {
            ++outputLength;
            if(out) *out++ = (uint16_t)codepoint;
        }
        else {
            outputLength += 2;
            if(out) {
                codepoint -= 0x10000;
                *out++ = (uint16_t)((codepoint >> 10) + 0xd800);
                *out++ = (uint16_t)((codepoint & 0x3ff) + 0xdc00);
            }
        }
        src += size;
    }
    if(out) {
        *out = 0;
    }
    return outputLength+1; // including terminating 0 word
}

int ConvertUTF16ToUTF8(const uint16_t* text, char* out, int inputLength)
{
    const uint16_t *src = text;
    const char *utf8;
    int outputLength = 0;
    while(*src && (!inputLength || src - text < inputLength))
    {
        int size = 0;
        int bytes = 0;
        int codepoint = GetCodepointFromUTF16(src, &size);
        utf8 = CodepointToUTF8(codepoint, &bytes);
        if(out) memcpy(out + outputLength, utf8, bytes);
        outputLength += bytes;
        src += size;
    }
    if(out) {
        *(out + outputLength) = 0;
    }
    return outputLength+1; // including terminating 0 byte
}

int ConvertUTF32ToUTF8(const uint32_t* text, char* out, int inputLength)
{
    const uint32_t* src = text;
    const char* utf8;
    int outputLength = 0;
    while(*src && (!inputLength || src - text < inputLength))
    {
        int bytes = 0;
        int codepoint = GetCodepointFromUTF32(src);
        utf8 = CodepointToUTF8(codepoint, &bytes);
        if(out) memcpy(out + outputLength, utf8, bytes);
        outputLength += bytes;
        ++src;
    }
    if(out) {
        *(out + outputLength) = 0;
    }
    return outputLength+1; // including terminating 0 byte
}

int ConvertUTF8ToUTF32(const char* text, uint32_t* out, int inputLength)
{
    const char* src = text;
    int outputLength = 0;
    while(*src && (!inputLength || src - text < inputLength))
    {
        int size = 0;
        int codepoint = GetCodepointFromUTF8(src, &size);
        if(out) *out++ = codepoint;
        outputLength += 1;
        src += size;
    }
    if(out) {
        *out = 0;
    }
    return outputLength+1; // including terminating 0 byte
}

int ConvertUTF16ToUTF32(const uint16_t* text, uint32_t* out, int inputLength)
{
    const uint16_t* src = text;
    int outputLength = 0;
    while(*src && (!inputLength || src - text < inputLength))
    {
        int size = 0;
        int codepoint = GetCodepointFromUTF16(src, &size);
        if(out) *out++ = codepoint;
        outputLength += 1;
        src += size;
    }
    if(out) {
        *out = 0;
    }
    return outputLength+1; // including terminating 0 byte
}

int ConvertUTF32ToUTF16(const uint32_t* text, uint16_t* out, int inputLength)
{
    const uint32_t* src = text;
    int outputLength = 0;
    while(*src && (!inputLength || src - text < inputLength))
    {
        int codepoint = GetCodepointFromUTF32(src);
        if (codepoint <= 0xffff) {
            ++outputLength;
            if(out) *out++ = (uint16_t)codepoint;
        }
        else {
            outputLength += 2;
            if(out) {
                codepoint -= 0x10000;
                *out++ = (uint16_t)((codepoint >> 10) + 0xd800);
                *out++ = (uint16_t)((codepoint & 0x3ff) + 0xdc00);
            }
        }
        ++src;
    }
    if(out) {
        *out = 0;
    }
    return outputLength+1; // including terminating 0 word
}

#endif  // RLUNICODE_IMPLEMENTATION
