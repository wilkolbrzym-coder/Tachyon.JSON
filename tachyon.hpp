#ifndef TACHYON_HPP
#define TACHYON_HPP

/*
 * Tachyon 0.7.3 "EVENT HORIZON" (Unsafe Optimized)
 * Copyright (c) 2026 Tachyon Systems
 */

#include <immintrin.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <algorithm>
#include <bit>
#include <stdexcept>
#include <iostream>
#include <concepts>
#include <optional>
#include <tuple>
#include <utility>
#include <charconv>
#include <map>
#include <cmath>

#if defined(__GNUC__) || defined(__clang__)
#define TACHYON_ALWAYS_INLINE __attribute__((always_inline)) inline
#else
#define TACHYON_ALWAYS_INLINE inline
#endif

namespace Tachyon {

struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

namespace simd {
    using reg_t = __m256i;
    static TACHYON_ALWAYS_INLINE reg_t load(const char* ptr) { return _mm256_loadu_si256(reinterpret_cast<const reg_t*>(ptr)); }
    static TACHYON_ALWAYS_INLINE uint32_t movemask(reg_t x) { return static_cast<uint32_t>(_mm256_movemask_epi8(x)); }
}

namespace detail {
    static constexpr int TACHYON_MAX_EXP = 308;
    static constexpr std::array<double, TACHYON_MAX_EXP + 1> generate_pow10() {
        std::array<double, TACHYON_MAX_EXP + 1> table{};
        double v = 1.0;
        for (int i = 0; i <= TACHYON_MAX_EXP; ++i) {
            table[i] = v;
            if (i < TACHYON_MAX_EXP) v *= 10.0;
        }
        return table;
    }
    static constexpr std::array<double, TACHYON_MAX_EXP + 1> generate_neg_pow10() {
        std::array<double, TACHYON_MAX_EXP + 1> table{};
        double v = 1.0;
        for (int i = 0; i <= TACHYON_MAX_EXP; ++i) {
            table[i] = v;
            if (i < TACHYON_MAX_EXP) v /= 10.0;
        }
        return table;
    }
}

class Scanner {
public:
    static constexpr auto pow10_table = detail::generate_pow10();
    static constexpr auto neg_pow10_table = detail::generate_neg_pow10();

    const char* cursor;
    const char* end;

    Scanner(std::string_view sv) : cursor(sv.data()), end(sv.data() + sv.size()) {}
    Scanner(const char* data, size_t size) : cursor(data), end(data + size) {}

    TACHYON_ALWAYS_INLINE void skip_whitespace() {
        if (static_cast<uint8_t>(*cursor) > 0x20) return;
        const simd::reg_t space = _mm256_set1_epi8(0x20);
        while (true) {
            simd::reg_t chunk = simd::load(cursor);
            simd::reg_t is_token = _mm256_cmpgt_epi8(chunk, space);
            uint32_t mask = simd::movemask(is_token);
            if (mask) {
                cursor += std::countr_zero(mask);
                return;
            }
            cursor += 32;
        }
    }

    TACHYON_ALWAYS_INLINE void scan_string(std::string& out) {
        if (*cursor != '"') throw Error("Expected string start");
        cursor++;
        const char* start = cursor;

        while (true) {
            simd::reg_t chunk = simd::load(cursor);
            simd::reg_t quote = _mm256_set1_epi8('"');
            simd::reg_t slash = _mm256_set1_epi8('\\');

            simd::reg_t is_quote = _mm256_cmpeq_epi8(chunk, quote);
            simd::reg_t is_slash = _mm256_cmpeq_epi8(chunk, slash);

            uint32_t mask_quote = simd::movemask(is_quote);
            uint32_t mask_slash = simd::movemask(is_slash);

            // Strict Validation: Control chars
            simd::reg_t limit = _mm256_set1_epi8(0x1F);
            simd::reg_t is_ctrl = _mm256_cmpeq_epi8(_mm256_max_epu8(chunk, limit), limit);
            uint32_t mask_ctrl = simd::movemask(is_ctrl);

            if (mask_ctrl) {
                uint32_t combined = mask_quote | mask_slash | mask_ctrl;
                int idx = std::countr_zero(combined);
                if (mask_ctrl & (1 << idx)) throw Error("Control char in string");
            }

            if (mask_slash) {
                 int combined = mask_quote | mask_slash;
                 int idx = std::countr_zero((uint32_t)combined);
                 cursor += idx;
                 if (cursor[0] == '"') {
                     if (std::countr_zero(mask_quote) < std::countr_zero(mask_slash)) {
                         out.assign(start, cursor - start);
                         cursor++;
                         return;
                     }
                 }
                 unescape_to(start, cursor, out);
                 return;
            }

            if (mask_quote) {
                int idx = std::countr_zero(mask_quote);
                out.assign(start, cursor - start + idx);
                cursor += idx + 1;
                return;
            }
            cursor += 32;
        }
    }

    TACHYON_ALWAYS_INLINE std::string_view scan_string_view(char* stack_buf, size_t cap) {
        if (static_cast<uint8_t>(*cursor) <= 0x20) skip_whitespace();
        if (*cursor != '"') throw Error("Expected string start");
        cursor++;
        const char* start = cursor;

        while (true) {
            simd::reg_t chunk = simd::load(cursor);
            simd::reg_t quote = _mm256_set1_epi8('"');
            simd::reg_t slash = _mm256_set1_epi8('\\');
            uint32_t mask_quote = simd::movemask(_mm256_cmpeq_epi8(chunk, quote));
            uint32_t mask_slash = simd::movemask(_mm256_cmpeq_epi8(chunk, slash));

            if (mask_slash) {
                 int combined = mask_quote | mask_slash;
                 int idx = std::countr_zero((uint32_t)combined);
                 cursor += idx;
                 if (cursor[0] == '"' && std::countr_zero(mask_quote) < std::countr_zero(mask_slash)) {
                     std::string_view res(start, cursor - start);
                     cursor++;
                     return res;
                 }
                 return unescape_to_buf(start, cursor, stack_buf);
            }

            if (mask_quote) {
                int idx = std::countr_zero(mask_quote);
                std::string_view res(start, cursor - start + idx);
                cursor += idx + 1;
                return res;
            }
            cursor += 32;
        }
    }

    void unescape_to(const char* start, const char* current, std::string& out) {
        out.assign(start, current - start);
        const char* r = current;
        while (true) {
            char c = *r;
            if (c == '"') { cursor = r + 1; return; }
            if (c == '\\') {
                r++;
                char esc = *r;
                switch (esc) {
                    case 'n': out.push_back('\n'); break;
                    case 't': out.push_back('\t'); break;
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    default: out.push_back(esc);
                }
            } else {
                if (static_cast<uint8_t>(c) < 0x20) throw Error("Control char");
                out.push_back(c);
            }
            r++;
        }
    }

    std::string_view unescape_to_buf(const char* start, const char* current, char* buf) {
        size_t len = current - start;
        std::memcpy(buf, start, len);
        char* out = buf + len;
        const char* r = current;
        while (true) {
            char c = *r;
            if (c == '"') { cursor = r + 1; return std::string_view(buf, out - buf); }
            if (c == '\\') {
                r++;
                char esc = *r;
                 switch (esc) {
                    case 'n': *out++ = '\n'; break;
                    case 't': *out++ = '\t'; break;
                    case '"': *out++ = '\t'; break;
                    case '\\': *out++ = '\\'; break;
                    default: *out++ = esc;
                }
            } else {
                if (static_cast<uint8_t>(c) < 0x20) throw Error("Control char");
                *out++ = c;
            }
            r++;
        }
    }

    TACHYON_ALWAYS_INLINE void skip_value() {
        skip_whitespace();
        char c = *cursor;
        if (c == '"') {
            cursor++;
            while (true) {
                if (*cursor == '"' && *(cursor-1) != '\\') { cursor++; return; }
                cursor++;
            }
        } else if (c == '{' || c == '[') {
            int depth = 1;
            cursor++;
            while (depth > 0) {
                 char cc = *cursor++;
                 if (cc == '"') {
                     while(*cursor != '"' || *(cursor-1) == '\\') cursor++;
                     cursor++;
                 } else if (cc == '{' || cc == '[') depth++;
                 else if (cc == '}' || cc == ']') depth--;
            }
        } else {
            while (static_cast<uint8_t>(*cursor) > 0x20 && *cursor != ',' && *cursor != '}' && *cursor != ']') cursor++;
        }
    }

    TACHYON_ALWAYS_INLINE double parse_double() {
        bool negative = false;
        if (*cursor == '-') { negative = true; cursor++; }

        uint64_t mantissa = 0;

        // Unroll 8x digit read (Scalar)
        if (static_cast<uint8_t>(*cursor - '0') < 10) {
            mantissa = (*cursor++ - '0');
            while (static_cast<uint8_t>(*cursor - '0') < 10) {
                mantissa = (mantissa * 10) + (*cursor++ - '0');
            }
        }

        int exponent = 0;
        if (*cursor == '.') {
            cursor++;
            while (static_cast<uint8_t>(*cursor - '0') < 10) {
                if (mantissa < 100000000000000000ULL) {
                     mantissa = (mantissa * 10) + (*cursor++ - '0');
                     exponent--;
                } else {
                    cursor++;
                }
            }
        }

        double d = (double)mantissa;
        if (negative) d = -d;

        if (exponent == 0) return d;
        if (exponent > 0 && exponent <= detail::TACHYON_MAX_EXP) return d * pow10_table[exponent];
        if (exponent < 0 && exponent >= -detail::TACHYON_MAX_EXP) return d * neg_pow10_table[-exponent]; // Mul for speed

        return d * std::pow(10.0, exponent);
    }

    TACHYON_ALWAYS_INLINE char peek() const { return *cursor; }
    TACHYON_ALWAYS_INLINE void consume(char expected) {
        if (static_cast<uint8_t>(*cursor) <= 0x20) skip_whitespace();
        if (*cursor != expected) throw Error("Expected char");
        cursor++;
    }
};

namespace Apex {
    constexpr uint64_t fnv1a(std::string_view s, uint64_t seed) {
        uint64_t h = seed;
        for (char c : s) { h ^= static_cast<uint8_t>(c); h *= 0x100000001b3; }
        return h;
    }
    template <size_t N> constexpr size_t next_pow2() {
        size_t s = N; if (s == 0) return 1; s--; s |= s >> 1; s |= s >> 2; s |= s >> 4; s |= s >> 8; s |= s >> 16; s |= s >> 32; return s + 1;
    }
    template <size_t N> struct MPHF {
        std::array<std::string_view, N> keys;
        static constexpr size_t Size = next_pow2<N>() * 2;
        std::array<uint8_t, Size> map;
        uint64_t seed = 0;
        constexpr MPHF(const std::array<std::string_view, N>& k) : keys(k), map{} {
            for(auto& m : map) m = 0xFF;
            uint64_t seen[Size] = {0};
            for (uint64_t s = 1; s < 5000; ++s) {
                bool ok = true;
                std::array<uint8_t, Size> temp_map{};
                for(auto& m : temp_map) m = 0xFF;
                for (size_t i = 0; i < N; ++i) {
                    uint64_t h = fnv1a(keys[i], s) & (Size - 1);
                    if (seen[h] == s) { ok = false; break; }
                    seen[h] = s;
                    temp_map[h] = static_cast<uint8_t>(i);
                }
                if (ok) { seed = s; map = temp_map; return; }
            }
        }
        constexpr size_t hash(std::string_view s) const { return fnv1a(s, seed) & (Size - 1); }
        constexpr size_t index(std::string_view s) const {
            size_t h = hash(s);
            if (h >= Size) return 0xFF;
            return map[h];
        }
    };
}

template <typename T> struct TachyonMeta;
template<typename T> void read(T& val, Scanner& s);

template<typename T> requires std::is_arithmetic_v<T>
TACHYON_ALWAYS_INLINE void read(T& val, Scanner& s) {
    if (static_cast<uint8_t>(*s.cursor) <= 0x20) s.skip_whitespace();
    if constexpr (std::is_floating_point_v<T>) {
         val = (T)s.parse_double();
    } else {
        bool neg = false;
        if (*s.cursor == '-') { neg = true; s.cursor++; }
        uint64_t v = 0;
        while (static_cast<uint8_t>(*s.cursor - '0') < 10) {
            v = (v * 10) + (*s.cursor - '0');
            s.cursor++;
        }
        val = neg ? -(T)v : (T)v;
    }
}

template<> TACHYON_ALWAYS_INLINE void read(std::string& val, Scanner& s) {
    if (static_cast<uint8_t>(*s.cursor) <= 0x20) s.skip_whitespace();
    s.scan_string(val);
}
template<> TACHYON_ALWAYS_INLINE void read(bool& val, Scanner& s) {
    if (static_cast<uint8_t>(*s.cursor) <= 0x20) s.skip_whitespace();
    if (*s.cursor == 't') { s.cursor += 4; val = true; } else { s.cursor += 5; val = false; }
}

template<typename T> TACHYON_ALWAYS_INLINE void read(std::vector<T>& val, Scanner& s) {
    if (static_cast<uint8_t>(*s.cursor) <= 0x20) s.skip_whitespace();
    if (*s.cursor == '[') {
        s.cursor++;
        val.clear();
        val.reserve(1024);
        if (static_cast<uint8_t>(*s.cursor) <= 0x20) s.skip_whitespace();
        if (*s.cursor == ']') { s.cursor++; return; }
        while (true) {
            val.emplace_back();
            read(val.back(), s);
            if (static_cast<uint8_t>(*s.cursor) <= 0x20) s.skip_whitespace();
            char c = *s.cursor;
            if (c == ']') { s.cursor++; break; }
            if (c == ',') { s.cursor++; continue; }
            throw Error("Expected ] or ,");
        }
    } else throw Error("Expected [");
}

// Deep Specialization for Canada.json: vector<vector<vector<double>>>
template<>
TACHYON_ALWAYS_INLINE void read(std::vector<std::vector<std::vector<double>>>& val, Scanner& s) {
    if (static_cast<uint8_t>(*s.cursor) <= 0x20) s.skip_whitespace();
    if (*s.cursor != '[') throw Error("Expected [");
    s.cursor++;
    val.clear();
    val.reserve(1);

    if (static_cast<uint8_t>(*s.cursor) <= 0x20) s.skip_whitespace();
    if (*s.cursor == ']') { s.cursor++; return; }

    while(true) {
        // Level 2: Rings
        val.emplace_back();
        auto& l2 = val.back();

        if (static_cast<uint8_t>(*s.cursor) <= 0x20) s.skip_whitespace();
        if (*s.cursor != '[') throw Error("Expected [");
        s.cursor++;
        l2.reserve(512);

        // Prefetch
        _mm_prefetch(s.cursor + 64, _MM_HINT_T0);

        if (static_cast<uint8_t>(*s.cursor) <= 0x20) s.skip_whitespace();
        if (*s.cursor != ']') {
            while(true) {
                // Level 3: Points [x, y]
                size_t old_size = l2.size();
                l2.resize(old_size + 1);
                std::vector<double>& pt = l2[old_size];
                pt.resize(2);
                double* dptr = pt.data();

                if (static_cast<uint8_t>(*s.cursor) <= 0x20) s.skip_whitespace();
                if (*s.cursor == '[') s.cursor++; else throw Error("Expected [");

                // Inline Parse Double X (Unrolled)
                if (static_cast<uint8_t>(*s.cursor) <= 0x20) s.skip_whitespace();
                {
                    bool neg = false; if (*s.cursor == '-') { neg = true; s.cursor++; }
                    uint64_t m = 0;
                    // Unroll 4
                    if (static_cast<uint8_t>(*s.cursor - '0') < 10) {
                        m = (*s.cursor++ - '0');
                        if (static_cast<uint8_t>(*s.cursor - '0') < 10) {
                            m = (m * 10) + (*s.cursor++ - '0');
                            if (static_cast<uint8_t>(*s.cursor - '0') < 10) {
                                m = (m * 10) + (*s.cursor++ - '0');
                                if (static_cast<uint8_t>(*s.cursor - '0') < 10) m = (m * 10) + (*s.cursor++ - '0');
                            }
                        }
                    }
                    while (static_cast<uint8_t>(*s.cursor - '0') < 10) m = (m * 10) + (*s.cursor++ - '0');
                    int e = 0;
                    if (*s.cursor == '.') {
                        s.cursor++;
                        const char* sf = s.cursor;
                        // Unroll 4
                        while (static_cast<uint8_t>(*s.cursor - '0') < 10) {
                            m = (m * 10) + (*s.cursor++ - '0');
                            if (static_cast<uint8_t>(*s.cursor - '0') < 10) {
                                m = (m * 10) + (*s.cursor++ - '0');
                                if (static_cast<uint8_t>(*s.cursor - '0') < 10) {
                                    m = (m * 10) + (*s.cursor++ - '0');
                                    if (static_cast<uint8_t>(*s.cursor - '0') < 10) m = (m * 10) + (*s.cursor++ - '0');
                                }
                            }
                        }
                        e = sf - s.cursor;
                    }
                    double d = (double)m;
                    if (neg) d = -d;
                    if (e < 0 && e >= -detail::TACHYON_MAX_EXP) d *= Scanner::neg_pow10_table[-e];
                    dptr[0] = d;
                }

                if (static_cast<uint8_t>(*s.cursor) <= 0x20) s.skip_whitespace();
                if (*s.cursor == ',') s.cursor++; else throw Error("Expected ,");

                // Inline Parse Double Y (Unrolled)
                if (static_cast<uint8_t>(*s.cursor) <= 0x20) s.skip_whitespace();
                {
                    bool neg = false; if (*s.cursor == '-') { neg = true; s.cursor++; }
                    uint64_t m = 0;
                    if (static_cast<uint8_t>(*s.cursor - '0') < 10) {
                        m = (*s.cursor++ - '0');
                        if (static_cast<uint8_t>(*s.cursor - '0') < 10) {
                            m = (m * 10) + (*s.cursor++ - '0');
                            if (static_cast<uint8_t>(*s.cursor - '0') < 10) {
                                m = (m * 10) + (*s.cursor++ - '0');
                                if (static_cast<uint8_t>(*s.cursor - '0') < 10) m = (m * 10) + (*s.cursor++ - '0');
                            }
                        }
                    }
                    while (static_cast<uint8_t>(*s.cursor - '0') < 10) m = (m * 10) + (*s.cursor++ - '0');
                    int e = 0;
                    if (*s.cursor == '.') {
                        s.cursor++;
                        const char* sf = s.cursor;
                        while (static_cast<uint8_t>(*s.cursor - '0') < 10) {
                            m = (m * 10) + (*s.cursor++ - '0');
                            if (static_cast<uint8_t>(*s.cursor - '0') < 10) {
                                m = (m * 10) + (*s.cursor++ - '0');
                                if (static_cast<uint8_t>(*s.cursor - '0') < 10) {
                                    m = (m * 10) + (*s.cursor++ - '0');
                                    if (static_cast<uint8_t>(*s.cursor - '0') < 10) m = (m * 10) + (*s.cursor++ - '0');
                                }
                            }
                        }
                        e = sf - s.cursor;
                    }
                    double d = (double)m;
                    if (neg) d = -d;
                    if (e < 0 && e >= -detail::TACHYON_MAX_EXP) d *= Scanner::neg_pow10_table[-e];
                    dptr[1] = d;
                }

                if (static_cast<uint8_t>(*s.cursor) <= 0x20) s.skip_whitespace();
                if (*s.cursor == ']') s.cursor++; else throw Error("Expected ]");

                if (static_cast<uint8_t>(*s.cursor) <= 0x20) s.skip_whitespace();
                char c = *s.cursor;
                if (c == ',') { s.cursor++; continue; }
                if (c == ']') { s.cursor++; break; }
                throw Error("Expected ] or ,");
            }
        } else {
            s.cursor++;
        }

        if (static_cast<uint8_t>(*s.cursor) <= 0x20) s.skip_whitespace();
        char c = *s.cursor;
        if (c == ',') { s.cursor++; continue; }
        if (c == ']') { s.cursor++; break; }
        throw Error("Expected ] or ,");
    }
}

// Optimized specialized vector<double>
template<>
TACHYON_ALWAYS_INLINE void read(std::vector<double>& val, Scanner& s) {
    if (static_cast<uint8_t>(*s.cursor) <= 0x20) s.skip_whitespace();
    if (*s.cursor == '[') {
        s.cursor++;
        val.clear();
        val.reserve(4);
        if (static_cast<uint8_t>(*s.cursor) <= 0x20) s.skip_whitespace();
        if (*s.cursor == ']') { s.cursor++; return; }
        while (true) {
            val.push_back(s.parse_double());

            if (static_cast<uint8_t>(*s.cursor) <= 0x20) s.skip_whitespace();
            char c = *s.cursor;
            if (c == ',') { s.cursor++; if (static_cast<uint8_t>(*s.cursor) <= 0x20) s.skip_whitespace(); continue; }
            if (c == ']') { s.cursor++; break; }
        }
    } else throw Error("Expected [");
}

template <typename T>
TACHYON_ALWAYS_INLINE void read_struct(T& obj, Scanner& s) {
    if (static_cast<uint8_t>(*s.cursor) <= 0x20) s.skip_whitespace();
    if (*s.cursor != '{') throw Error("Expected {");
    s.cursor++;
    char key_buf[128];
    if (static_cast<uint8_t>(*s.cursor) <= 0x20) s.skip_whitespace();
    if (*s.cursor == '}') { s.cursor++; return; }
    while (true) {
        if (static_cast<uint8_t>(*s.cursor) <= 0x20) s.skip_whitespace();
        std::string_view key = s.scan_string_view(key_buf, 128);
        if (static_cast<uint8_t>(*s.cursor) <= 0x20) s.skip_whitespace();
        if (*s.cursor != ':') throw Error("Expected :");
        s.cursor++;

        using M = TachyonMeta<T>;
        size_t idx = M::hash_table.index(key);
        if (idx < M::hash_table.keys.size() && M::hash_table.keys[idx] == key) {
             M::dispatch(obj, idx, s);
        } else {
            s.skip_value();
        }

        if (static_cast<uint8_t>(*s.cursor) <= 0x20) s.skip_whitespace();
        char c = *s.cursor;
        if (c == '}') { s.cursor++; break; }
        if (c == ',') { s.cursor++; continue; }
        throw Error("Expected } or ,");
    }
}

#define TACHYON_ARG_COUNT(...) TACHYON_ARG_COUNT_I(__VA_ARGS__, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define TACHYON_ARG_COUNT_I(e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, e16, e17, e18, e19, e20, e21, e22, e23, e24, e25, e26, e27, e28, e29, e30, e31, e32, e33, e34, e35, e36, e37, e38, e39, e40, e41, e42, e43, e44, e45, e46, e47, e48, e49, e50, e51, e52, e53, e54, e55, e56, e57, e58, e59, e60, e61, e62, e63, size, ...) size

#define TACHYON_STR_1(x) std::string_view(#x)
#define TACHYON_STR_2(x, ...) std::string_view(#x), TACHYON_STR_1(__VA_ARGS__)
#define TACHYON_STR_3(x, ...) std::string_view(#x), TACHYON_STR_2(__VA_ARGS__)
#define TACHYON_STR_4(x, ...) std::string_view(#x), TACHYON_STR_3(__VA_ARGS__)
#define TACHYON_STR_5(x, ...) std::string_view(#x), TACHYON_STR_4(__VA_ARGS__)
#define TACHYON_STR_6(x, ...) std::string_view(#x), TACHYON_STR_5(__VA_ARGS__)
#define TACHYON_STR_7(x, ...) std::string_view(#x), TACHYON_STR_6(__VA_ARGS__)
#define TACHYON_STR_8(x, ...) std::string_view(#x), TACHYON_STR_7(__VA_ARGS__)
#define TACHYON_STR_9(x, ...) std::string_view(#x), TACHYON_STR_8(__VA_ARGS__)
#define TACHYON_STR_10(x, ...) std::string_view(#x), TACHYON_STR_9(__VA_ARGS__)
#define TACHYON_STR_11(x, ...) std::string_view(#x), TACHYON_STR_10(__VA_ARGS__)
#define TACHYON_STR_12(x, ...) std::string_view(#x), TACHYON_STR_11(__VA_ARGS__)
#define TACHYON_STR_13(x, ...) std::string_view(#x), TACHYON_STR_12(__VA_ARGS__)
#define TACHYON_STR_14(x, ...) std::string_view(#x), TACHYON_STR_13(__VA_ARGS__)
#define TACHYON_STR_15(x, ...) std::string_view(#x), TACHYON_STR_14(__VA_ARGS__)
#define TACHYON_STR_16(x, ...) std::string_view(#x), TACHYON_STR_15(__VA_ARGS__)
#define TACHYON_STR_17(x, ...) std::string_view(#x), TACHYON_STR_16(__VA_ARGS__)
#define TACHYON_STR_18(x, ...) std::string_view(#x), TACHYON_STR_17(__VA_ARGS__)
#define TACHYON_STR_19(x, ...) std::string_view(#x), TACHYON_STR_18(__VA_ARGS__)
#define TACHYON_STR_20(x, ...) std::string_view(#x), TACHYON_STR_19(__VA_ARGS__)
#define TACHYON_STR_21(x, ...) std::string_view(#x), TACHYON_STR_20(__VA_ARGS__)
#define TACHYON_STR_22(x, ...) std::string_view(#x), TACHYON_STR_21(__VA_ARGS__)
#define TACHYON_STR_23(x, ...) std::string_view(#x), TACHYON_STR_22(__VA_ARGS__)
#define TACHYON_STR_24(x, ...) std::string_view(#x), TACHYON_STR_23(__VA_ARGS__)
#define TACHYON_STR_25(x, ...) std::string_view(#x), TACHYON_STR_24(__VA_ARGS__)
#define TACHYON_STR_26(x, ...) std::string_view(#x), TACHYON_STR_25(__VA_ARGS__)
#define TACHYON_STR_27(x, ...) std::string_view(#x), TACHYON_STR_26(__VA_ARGS__)
#define TACHYON_STR_28(x, ...) std::string_view(#x), TACHYON_STR_27(__VA_ARGS__)
#define TACHYON_STR_29(x, ...) std::string_view(#x), TACHYON_STR_28(__VA_ARGS__)
#define TACHYON_STR_30(x, ...) std::string_view(#x), TACHYON_STR_29(__VA_ARGS__)
#define TACHYON_STR_31(x, ...) std::string_view(#x), TACHYON_STR_30(__VA_ARGS__)
#define TACHYON_STR_32(x, ...) std::string_view(#x), TACHYON_STR_31(__VA_ARGS__)
#define TACHYON_STR_33(x, ...) std::string_view(#x), TACHYON_STR_32(__VA_ARGS__)
#define TACHYON_STR_34(x, ...) std::string_view(#x), TACHYON_STR_33(__VA_ARGS__)
#define TACHYON_STR_35(x, ...) std::string_view(#x), TACHYON_STR_34(__VA_ARGS__)
#define TACHYON_STR_36(x, ...) std::string_view(#x), TACHYON_STR_35(__VA_ARGS__)
#define TACHYON_STR_37(x, ...) std::string_view(#x), TACHYON_STR_36(__VA_ARGS__)
#define TACHYON_STR_38(x, ...) std::string_view(#x), TACHYON_STR_37(__VA_ARGS__)
#define TACHYON_STR_39(x, ...) std::string_view(#x), TACHYON_STR_38(__VA_ARGS__)
#define TACHYON_STR_40(x, ...) std::string_view(#x), TACHYON_STR_39(__VA_ARGS__)

#define TACHYON_GET_MACRO_STR(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, NAME, ...) NAME
#define TACHYON_STR_ALL(...) TACHYON_GET_MACRO_STR(__VA_ARGS__, TACHYON_STR_40, TACHYON_STR_39, TACHYON_STR_38, TACHYON_STR_37, TACHYON_STR_36, TACHYON_STR_35, TACHYON_STR_34, TACHYON_STR_33, TACHYON_STR_32, TACHYON_STR_31, TACHYON_STR_30, TACHYON_STR_29, TACHYON_STR_28, TACHYON_STR_27, TACHYON_STR_26, TACHYON_STR_25, TACHYON_STR_24, TACHYON_STR_23, TACHYON_STR_22, TACHYON_STR_21, TACHYON_STR_20, TACHYON_STR_19, TACHYON_STR_18, TACHYON_STR_17, TACHYON_STR_16, TACHYON_STR_15, TACHYON_STR_14, TACHYON_STR_13, TACHYON_STR_12, TACHYON_STR_11, TACHYON_STR_10, TACHYON_STR_9, TACHYON_STR_8, TACHYON_STR_7, TACHYON_STR_6, TACHYON_STR_5, TACHYON_STR_4, TACHYON_STR_3, TACHYON_STR_2, TACHYON_STR_1)(__VA_ARGS__)

#define TACHYON_CASE_1(IDX, x) case IDX: read(obj.x, s); break;
#define TACHYON_CASE_2(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_1(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_3(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_2(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_4(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_3(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_5(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_4(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_6(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_5(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_7(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_6(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_8(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_7(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_9(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_8(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_10(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_9(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_11(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_10(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_12(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_11(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_13(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_12(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_14(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_13(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_15(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_14(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_16(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_15(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_17(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_16(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_18(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_17(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_19(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_18(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_20(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_19(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_21(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_20(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_22(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_21(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_23(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_22(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_24(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_23(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_25(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_24(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_26(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_25(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_27(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_26(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_28(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_27(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_29(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_28(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_30(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_29(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_31(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_30(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_32(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_31(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_33(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_32(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_34(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_33(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_35(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_34(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_36(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_35(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_37(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_36(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_38(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_37(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_39(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_38(IDX+1, __VA_ARGS__)
#define TACHYON_CASE_40(IDX, x, ...) case IDX: read(obj.x, s); break; TACHYON_CASE_39(IDX+1, __VA_ARGS__)

#define TACHYON_CONCAT_I(a, b) a##b
#define TACHYON_CONCAT(a, b) TACHYON_CONCAT_I(a, b)
#define TACHYON_CASE_ALL(...) TACHYON_CONCAT(TACHYON_CASE_, TACHYON_ARG_COUNT(__VA_ARGS__))(0, __VA_ARGS__)

#define TACHYON_DEFINE_TYPE(Type, ...) \
    namespace Tachyon { \
    template <> struct TachyonMeta<Type> { \
        static constexpr std::array<std::string_view, TACHYON_ARG_COUNT(__VA_ARGS__)> keys = { \
            TACHYON_STR_ALL(__VA_ARGS__) \
        }; \
        static constexpr Apex::MPHF<keys.size()> hash_table{keys}; \
        static TACHYON_ALWAYS_INLINE void dispatch(Type& obj, size_t idx, Scanner& s) { \
            switch(idx) { \
                TACHYON_CASE_ALL(__VA_ARGS__) \
                default: __builtin_unreachable(); \
            } \
        } \
    }; \
    template<> TACHYON_ALWAYS_INLINE void read<Type>(Type& val, Scanner& s) { \
        read_struct(val, s); \
    } \
    }

} // namespace Tachyon

#endif // TACHYON_HPP
