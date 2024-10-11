#pragma once
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

#define SHA1_HEX_SIZE (40 + 1)
#define SHA1_BASE64_SIZE (28 + 1)

#define SHA1(h1, h2, l) Sha1::Value(0x## h1, 0x## h2, 0x## l)

class Sha1
{
public:
    class Value
    {
    private:
        uint64_t high1;
        uint64_t high2;
        uint32_t low;

    public:
        constexpr Value()
            : high1(0)
            , high2(0)
            , low(0)
        {
        }
        constexpr Value(uint64_t h1, uint64_t h2, uint32_t l)
            : high1(h1)
            , high2(h2)
            , low(l)
        {
        }
        explicit Value(const std::string& hex_digest)
        {
            if (hex_digest.length() != 40)
                throw std::invalid_argument("Invalid SHA-1 hex digest length");
            high1 = std::stoull(hex_digest.substr(0, 16), nullptr, 16);
            high2 = std::stoull(hex_digest.substr(16, 16), nullptr, 16);
            low = static_cast<uint32_t>(std::stoul(hex_digest.substr(32, 8), nullptr, 16));
        }
        constexpr Value(const char* str, std::size_t len)
            : high1(0)
            , high2(0)
            , low(0)
        {
            if (len != 40)
                throw std::invalid_argument("Invalid SHA-1 hex digest length");
            high1 = parse_hex_uint64(str, 0, 16);
            high2 = parse_hex_uint64(str, 16, 16);
            low = parse_hex_uint32(str, 32, 8);
        }
        std::string to_hex() const
        {
            std::ostringstream oss;
            oss << std::hex << std::setfill('0');
            oss << std::setw(16) << high1;
            oss << std::setw(16) << high2;
            oss << std::setw(8) << low;
            return oss.str();
        }
        friend std::ostream& operator<<(std::ostream& os, const Value& sha1)
        {
            os << sha1.to_hex();
            return os;
        }
        constexpr uint64_t getHigh1() const { return high1; }
        constexpr uint64_t getHigh2() const { return high2; }
        constexpr uint32_t getLow() const { return low; }
        bool operator<(const Value& other) const {
            if (high1 != other.high1)
                return high1 < other.high1;
            if (high2 != other.high2)
                return high2 < other.high2;
            return low < other.low;
        }
        bool operator==(const Value& other) const {
            return high1 == other.high1 && high2 == other.high2 && low == other.low;
        }
        bool operator!=(const Value& other) const {
            return !(*this == other);
        }

    private:
        static constexpr uint8_t hex_char_to_value(char c)
        {
            return (c >= '0' && c <= '9') ? static_cast<uint8_t>(c - '0') : (c >= 'a' && c <= 'f') ? static_cast<uint8_t>(c - 'a' + 10) : (c >= 'A' && c <= 'F') ? static_cast<uint8_t>(c - 'A' + 10) : throw std::invalid_argument("Invalid hex character");
        }
        static constexpr uint64_t parse_hex_uint64(const char* str, std::size_t start, std::size_t len)
        {
            uint64_t result = 0;
            for (std::size_t i = 0; i < len; ++i) {
                result = (result << 4) | hex_char_to_value(str[start + i]);
            }
            return result;
        }
        static constexpr uint32_t parse_hex_uint32(const char* str, std::size_t start, std::size_t len)
        {
            uint32_t result = 0;
            for (std::size_t i = 0; i < len; ++i) {
                result = (result << 4) | hex_char_to_value(str[start + i]);
            }
            return result;
        }
    };

private:
    void add_byte_dont_count_bits(uint8_t x)
    {
        buf[i++] = x;

        if (i >= sizeof(buf)) {
            i = 0;
            process_block(buf);
        }
    }
    static uint32_t rol32(uint32_t x, uint32_t n) { return (x << n) | (x >> (32 - n)); }
    static uint32_t make_word(const uint8_t* p) { return ((uint32_t)p[0] << 3 * 8) | ((uint32_t)p[1] << 2 * 8) | ((uint32_t)p[2] << 1 * 8) | ((uint32_t)p[3] << 0 * 8); }
    void process_block(const uint8_t* ptr)
    {
        const uint32_t c0 = 0x5a827999;
        const uint32_t c1 = 0x6ed9eba1;
        const uint32_t c2 = 0x8f1bbcdc;
        const uint32_t c3 = 0xca62c1d6;
        uint32_t a = state[0];
        uint32_t b = state[1];
        uint32_t c = state[2];
        uint32_t d = state[3];
        uint32_t e = state[4];
        uint32_t w[16];
        for (int i = 0; i < 16; i++)
            w[i] = make_word(ptr + i * 4);
#define SHA1_LOAD(i) w[i & 15] = rol32(w[(i + 13) & 15] ^ w[(i + 8) & 15] ^ w[(i + 2) & 15] ^ w[i & 15], 1);
#define SHA1_ROUND_0(v, u, x, y, z, i)                       \
    z += ((u & (x ^ y)) ^ y) + w[i & 15] + c0 + rol32(v, 5); \
    u = rol32(u, 30);
#define SHA1_ROUND_1(v, u, x, y, z, i)                                    \
    SHA1_LOAD(i) z += ((u & (x ^ y)) ^ y) + w[i & 15] + c0 + rol32(v, 5); \
    u = rol32(u, 30);
#define SHA1_ROUND_2(v, u, x, y, z, i)                            \
    SHA1_LOAD(i) z += (u ^ x ^ y) + w[i & 15] + c1 + rol32(v, 5); \
    u = rol32(u, 30);
#define SHA1_ROUND_3(v, u, x, y, z, i)                                          \
    SHA1_LOAD(i) z += (((u | x) & y) | (u & x)) + w[i & 15] + c2 + rol32(v, 5); \
    u = rol32(u, 30);
#define SHA1_ROUND_4(v, u, x, y, z, i)                            \
    SHA1_LOAD(i) z += (u ^ x ^ y) + w[i & 15] + c3 + rol32(v, 5); \
    u = rol32(u, 30);
        SHA1_ROUND_0(a, b, c, d, e, 0);
        SHA1_ROUND_0(e, a, b, c, d, 1);
        SHA1_ROUND_0(d, e, a, b, c, 2);
        SHA1_ROUND_0(c, d, e, a, b, 3);
        SHA1_ROUND_0(b, c, d, e, a, 4);
        SHA1_ROUND_0(a, b, c, d, e, 5);
        SHA1_ROUND_0(e, a, b, c, d, 6);
        SHA1_ROUND_0(d, e, a, b, c, 7);
        SHA1_ROUND_0(c, d, e, a, b, 8);
        SHA1_ROUND_0(b, c, d, e, a, 9);
        SHA1_ROUND_0(a, b, c, d, e, 10);
        SHA1_ROUND_0(e, a, b, c, d, 11);
        SHA1_ROUND_0(d, e, a, b, c, 12);
        SHA1_ROUND_0(c, d, e, a, b, 13);
        SHA1_ROUND_0(b, c, d, e, a, 14);
        SHA1_ROUND_0(a, b, c, d, e, 15);
        SHA1_ROUND_1(e, a, b, c, d, 16);
        SHA1_ROUND_1(d, e, a, b, c, 17);
        SHA1_ROUND_1(c, d, e, a, b, 18);
        SHA1_ROUND_1(b, c, d, e, a, 19);
        SHA1_ROUND_2(a, b, c, d, e, 20);
        SHA1_ROUND_2(e, a, b, c, d, 21);
        SHA1_ROUND_2(d, e, a, b, c, 22);
        SHA1_ROUND_2(c, d, e, a, b, 23);
        SHA1_ROUND_2(b, c, d, e, a, 24);
        SHA1_ROUND_2(a, b, c, d, e, 25);
        SHA1_ROUND_2(e, a, b, c, d, 26);
        SHA1_ROUND_2(d, e, a, b, c, 27);
        SHA1_ROUND_2(c, d, e, a, b, 28);
        SHA1_ROUND_2(b, c, d, e, a, 29);
        SHA1_ROUND_2(a, b, c, d, e, 30);
        SHA1_ROUND_2(e, a, b, c, d, 31);
        SHA1_ROUND_2(d, e, a, b, c, 32);
        SHA1_ROUND_2(c, d, e, a, b, 33);
        SHA1_ROUND_2(b, c, d, e, a, 34);
        SHA1_ROUND_2(a, b, c, d, e, 35);
        SHA1_ROUND_2(e, a, b, c, d, 36);
        SHA1_ROUND_2(d, e, a, b, c, 37);
        SHA1_ROUND_2(c, d, e, a, b, 38);
        SHA1_ROUND_2(b, c, d, e, a, 39);
        SHA1_ROUND_3(a, b, c, d, e, 40);
        SHA1_ROUND_3(e, a, b, c, d, 41);
        SHA1_ROUND_3(d, e, a, b, c, 42);
        SHA1_ROUND_3(c, d, e, a, b, 43);
        SHA1_ROUND_3(b, c, d, e, a, 44);
        SHA1_ROUND_3(a, b, c, d, e, 45);
        SHA1_ROUND_3(e, a, b, c, d, 46);
        SHA1_ROUND_3(d, e, a, b, c, 47);
        SHA1_ROUND_3(c, d, e, a, b, 48);
        SHA1_ROUND_3(b, c, d, e, a, 49);
        SHA1_ROUND_3(a, b, c, d, e, 50);
        SHA1_ROUND_3(e, a, b, c, d, 51);
        SHA1_ROUND_3(d, e, a, b, c, 52);
        SHA1_ROUND_3(c, d, e, a, b, 53);
        SHA1_ROUND_3(b, c, d, e, a, 54);
        SHA1_ROUND_3(a, b, c, d, e, 55);
        SHA1_ROUND_3(e, a, b, c, d, 56);
        SHA1_ROUND_3(d, e, a, b, c, 57);
        SHA1_ROUND_3(c, d, e, a, b, 58);
        SHA1_ROUND_3(b, c, d, e, a, 59);
        SHA1_ROUND_4(a, b, c, d, e, 60);
        SHA1_ROUND_4(e, a, b, c, d, 61);
        SHA1_ROUND_4(d, e, a, b, c, 62);
        SHA1_ROUND_4(c, d, e, a, b, 63);
        SHA1_ROUND_4(b, c, d, e, a, 64);
        SHA1_ROUND_4(a, b, c, d, e, 65);
        SHA1_ROUND_4(e, a, b, c, d, 66);
        SHA1_ROUND_4(d, e, a, b, c, 67);
        SHA1_ROUND_4(c, d, e, a, b, 68);
        SHA1_ROUND_4(b, c, d, e, a, 69);
        SHA1_ROUND_4(a, b, c, d, e, 70);
        SHA1_ROUND_4(e, a, b, c, d, 71);
        SHA1_ROUND_4(d, e, a, b, c, 72);
        SHA1_ROUND_4(c, d, e, a, b, 73);
        SHA1_ROUND_4(b, c, d, e, a, 74);
        SHA1_ROUND_4(a, b, c, d, e, 75);
        SHA1_ROUND_4(e, a, b, c, d, 76);
        SHA1_ROUND_4(d, e, a, b, c, 77);
        SHA1_ROUND_4(c, d, e, a, b, 78);
        SHA1_ROUND_4(b, c, d, e, a, 79);
#undef SHA1_LOAD
#undef SHA1_ROUND_0
#undef SHA1_ROUND_1
#undef SHA1_ROUND_2
#undef SHA1_ROUND_3
#undef SHA1_ROUND_4
        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
    }

public:
    uint32_t state[5];
    uint8_t buf[64];
    uint32_t i;
    uint64_t n_bits;
    Sha1(const char* text = nullptr)
        : i(0)
        , n_bits(0)
    {
        state[0] = 0x67452301;
        state[1] = 0xEFCDAB89;
        state[2] = 0x98BADCFE;
        state[3] = 0x10325476;
        state[4] = 0xC3D2E1F0;
        if (text)
            add(text);
    }
    Sha1& add(uint8_t x)
    {
        add_byte_dont_count_bits(x);
        n_bits += 8;
        return *this;
    }
    Sha1& add(char c) { return add(*reinterpret_cast<uint8_t*>(&c)); }
    Sha1& add(const void* data, uint32_t n)
    {
        if (!data)
            return *this;
        const auto* ptr = static_cast<const uint8_t*>(data);
        for (; n && i % sizeof(buf); n--)
            add(*ptr++);
        for (; n >= sizeof(buf); n -= sizeof(buf)) {
            process_block(ptr);
            ptr += sizeof(buf);
            n_bits += sizeof(buf) * 8;
        }
        for (; n; n--)
            add(*ptr++);

        return *this;
    }
    Sha1& add(const char* text)
    {
        if (!text)
            return *this;
        return add(text, strlen(text));
    }
    Sha1& finalize()
    {
        add_byte_dont_count_bits(0x80);
        while (i % 64 != 56)
            add_byte_dont_count_bits(0x00);
        for (int j = 7; j >= 0; j--)
            add_byte_dont_count_bits(n_bits >> j * 8);
        return *this;
    }
    const Sha1& print_hex(char* hex, bool zero_terminate = true, const char* alphabet = "0123456789abcdef") const
    {
        int k = 0;
        for (const unsigned int i : state) {
            for (int j = 7; j >= 0; j--) {
                hex[k++] = alphabet[(i >> j * 4) & 0xf];
            }
        }
        if (zero_terminate)
            hex[k] = '\0';
        return *this;
    }

    const Sha1& print_base64(char* base64, bool zero_terminate = true) const
    {
        static const auto *table = reinterpret_cast<const uint8_t*>(
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789"
            "+/");
        uint32_t triples[7] = {
            ((state[0] & 0xffffff00) >> 1 * 8),
            ((state[0] & 0x000000ff) << 2 * 8) | ((state[1] & 0xffff0000) >> 2 * 8),
            ((state[1] & 0x0000ffff) << 1 * 8) | ((state[2] & 0xff000000) >> 3 * 8),
            ((state[2] & 0x00ffffff) << 0 * 8),
            ((state[3] & 0xffffff00) >> 1 * 8),
            ((state[3] & 0x000000ff) << 2 * 8) | ((state[4] & 0xffff0000) >> 2 * 8),
            ((state[4] & 0x0000ffff) << 1 * 8),
        };
        for (int i = 0; i < 7; i++) {
            const uint32_t x = triples[i];
            base64[i * 4 + 0] = table[(x >> 3 * 6) % 64];
            base64[i * 4 + 1] = table[(x >> 2 * 6) % 64];
            base64[i * 4 + 2] = table[(x >> 1 * 6) % 64];
            base64[i * 4 + 3] = table[(x >> 0 * 6) % 64];
        }
        base64[SHA1_BASE64_SIZE - 2] = '=';
        if (zero_terminate)
            base64[SHA1_BASE64_SIZE - 1] = '\0';
        return *this;
    }
};


constexpr Sha1::Value operator"" _sha1(const char* str, std::size_t len) { return {str, len}; }

template <>
struct std::hash<Sha1::Value>
{
    std::size_t operator()(const Sha1::Value& sha1) const noexcept
    {
        const std::size_t h1 = std::hash<uint64_t>{}(sha1.getHigh1());
        const std::size_t h2 = std::hash<uint64_t>{}(sha1.getHigh2());
        const std::size_t h3 = std::hash<uint32_t>{}(sha1.getLow());
        std::size_t seed = h1;
        seed ^= h2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= h3 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }
};
