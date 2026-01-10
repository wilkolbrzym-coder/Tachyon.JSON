#ifndef TACHYON_HPP
#define TACHYON_HPP

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

// MIT License
// Copyright (c) 2026 Tachyon Systems
// Version 0.7.3 "EVENT HORIZON"

namespace Tachyon {

struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// ==============================================================================================
//  GOD-MODE SIMD ENGINE (AVX2)
// ==============================================================================================

namespace simd {
    using reg_t = __m256i;

    static inline reg_t load(const char* ptr) {
        return _mm256_loadu_si256(reinterpret_cast<const reg_t*>(ptr));
    }

    static inline void store(char* ptr, reg_t val) {
        _mm256_storeu_si256(reinterpret_cast<reg_t*>(ptr), val);
    }

    static inline uint32_t movemask(reg_t x) {
        return static_cast<uint32_t>(_mm256_movemask_epi8(x));
    }
}

class Scanner {
public:
    const char* cursor;
    const char* end;

    Scanner(std::string_view sv) : cursor(sv.data()), end(sv.data() + sv.size()) {}
    Scanner(const char* data, size_t size) : cursor(data), end(data + size) {}

    // --- SIMD WHITESPACE SKIP ---
    void skip_whitespace() {
        while (cursor + 32 <= end) {
            simd::reg_t chunk = simd::load(cursor);
            simd::reg_t space = _mm256_set1_epi8(0x20);
            simd::reg_t nl = _mm256_set1_epi8(0x0A);
            simd::reg_t cr = _mm256_set1_epi8(0x0D);
            simd::reg_t tab = _mm256_set1_epi8(0x09);

            simd::reg_t is_space = _mm256_cmpeq_epi8(chunk, space);
            simd::reg_t is_tab = _mm256_cmpeq_epi8(chunk, tab);
            simd::reg_t is_nl = _mm256_cmpeq_epi8(chunk, nl);
            simd::reg_t is_cr = _mm256_cmpeq_epi8(chunk, cr);

            simd::reg_t is_ws = _mm256_or_si256(_mm256_or_si256(is_space, is_tab), _mm256_or_si256(is_nl, is_cr));

            uint32_t mask = simd::movemask(is_ws);

            if (mask == 0xFFFFFFFF) {
                cursor += 32;
            } else {
                uint32_t not_ws = ~mask;
                cursor += std::countr_zero(not_ws);
                return;
            }
        }

        while (cursor < end) {
             uint8_t c = static_cast<uint8_t>(*cursor);
             if (c == 0x20 || c == 0x0A || c == 0x0D || c == 0x09) cursor++;
             else break;
        }
    }

    // --- ROBUST SCAN STRING ---
    std::string_view scan_string(char* out_buf, size_t cap) {
        if (*cursor != '"') throw Error("Expected string start");
        cursor++;
        const char* start = cursor;

        int slash_carry = 0;
        bool has_escapes = false;

        while (cursor + 32 <= end) {
            simd::reg_t chunk = simd::load(cursor);
            simd::reg_t quote = _mm256_set1_epi8('"');
            simd::reg_t slash = _mm256_set1_epi8('\\');

            simd::reg_t is_quote = _mm256_cmpeq_epi8(chunk, quote);
            simd::reg_t is_slash = _mm256_cmpeq_epi8(chunk, slash);

            uint32_t mask_slash = simd::movemask(is_slash);
            uint32_t mask_quote = simd::movemask(is_quote);

            // Control Check
            simd::reg_t limit = _mm256_set1_epi8(0x1F);
            simd::reg_t max_val = _mm256_max_epu8(chunk, limit);
            simd::reg_t is_control = _mm256_cmpeq_epi8(max_val, limit);
            uint32_t mask_control = simd::movemask(is_control);

            if (mask_slash) has_escapes = true;

            if (mask_quote) {
                uint32_t q = mask_quote;
                while (q) {
                    int idx = std::countr_zero(q);

                    int slashes = 0;
                    if (idx == 0) {
                        slashes = slash_carry;
                    } else {
                        int k = idx - 1;
                        while (k >= 0 && (mask_slash & (1 << k))) { slashes++; k--; }
                        if (k < 0) slashes += slash_carry;
                    }

                    if (slashes % 2 == 0) {
                        if (mask_control & ((1 << idx) - 1)) throw Error("Control character in string");

                        cursor += idx;
                        if (has_escapes) return unescape(start, cursor - 1, out_buf);
                        std::string_view res(start, cursor - start);
                        cursor++;
                        return res;
                    }

                    q &= ~(1 << idx);
                }
            } else {
                if (mask_control) throw Error("Control character in string");
            }

            if (mask_slash & (1U << 31)) {
                int trailing = std::countl_one(mask_slash);
                if (trailing == 32) slash_carry += 32;
                else slash_carry = trailing;
            } else {
                slash_carry = 0;
            }

            cursor += 32;
        }

        while (cursor < end) {
            char c = *cursor;
            if (c == '"') {
                int backslash_count = 0;
                const char* back = cursor - 1;
                while (back >= start && *back == '\\') {
                    backslash_count++;
                    back--;
                }

                if (backslash_count % 2 == 1) {
                    has_escapes = true;
                } else {
                    if (has_escapes) return unescape(start, cursor - 1, out_buf);
                    std::string_view res(start, cursor - start);
                    cursor++;
                    return res;
                }
            } else if (c == '\\') {
                has_escapes = true;
            } else if (static_cast<uint8_t>(c) < 0x20) {
                throw Error("Control character");
            }
            cursor++;
        }
        throw Error("Unterminated string");
    }

    std::string_view unescape(const char* start, const char* end_ptr, char* out_buf) {
        char* out = out_buf;
        const char* r = start;
        while (r <= end_ptr) {
            if (*r == '\\') {
                r++;
                if (r > end_ptr) throw Error("Incomplete escape");
                char esc = *r;
                switch (esc) {
                    case '"': *out++ = '"'; break;
                    case '\\': *out++ = '\\'; break;
                    case '/': *out++ = '/'; break;
                    case 'b': *out++ = '\b'; break;
                    case 'f': *out++ = '\f'; break;
                    case 'n': *out++ = '\n'; break;
                    case 'r': *out++ = '\r'; break;
                    case 't': *out++ = '\t'; break;
                    case 'u': {
                        if (r + 4 > end_ptr) throw Error("Incomplete unicode");
                        uint32_t cp = decode_hex4(r + 1);
                        r += 4;

                        if (cp >= 0xD800 && cp <= 0xDBFF) {
                            if (r + 2 <= end_ptr && r[1] == '\\' && r[2] == 'u') {
                                r += 2;
                                if (r + 4 > end_ptr) throw Error("Incomplete low");
                                uint32_t low = decode_hex4(r + 1);
                                r += 4;
                                if (low >= 0xDC00 && low <= 0xDFFF) {
                                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                                } else {
                                    throw Error("Invalid low surrogate");
                                }
                            } else {
                                throw Error("Missing low surrogate");
                            }
                        } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                             throw Error("Lone low surrogate");
                        }

                        if (cp < 0x80) {
                            *out++ = static_cast<char>(cp);
                        } else if (cp < 0x800) {
                            *out++ = static_cast<char>(0xC0 | (cp >> 6));
                            *out++ = static_cast<char>(0x80 | (cp & 0x3F));
                        } else if (cp < 0x10000) {
                            *out++ = static_cast<char>(0xE0 | (cp >> 12));
                            *out++ = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                            *out++ = static_cast<char>(0x80 | (cp & 0x3F));
                        } else {
                            *out++ = static_cast<char>(0xF0 | (cp >> 18));
                            *out++ = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                            *out++ = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                            *out++ = static_cast<char>(0x80 | (cp & 0x3F));
                        }
                        break;
                    }
                    default: throw Error("Invalid escape");
                }
            } else {
                *out++ = *r;
            }
            r++;
        }
        cursor++;
        return std::string_view(out_buf, out - out_buf);
    }

    uint32_t decode_hex4(const char* p) {
        uint32_t val = 0;
        for (int i = 0; i < 4; ++i) {
            char c = p[i];
            val <<= 4;
            if (c >= '0' && c <= '9') val |= (c - '0');
            else if (c >= 'a' && c <= 'f') val |= (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') val |= (c - 'A' + 10);
            else throw Error("Invalid hex");
        }
        return val;
    }

    // --- SIMD VALUE SKIPPER (FIXED) ---
    void skip_value() {
        skip_whitespace();
        char c = peek();
        if (c == '"') {
            cursor++;
            while (cursor < end) {
                if (*cursor == '"') {
                    int bs = 0;
                    const char* b = cursor - 1;
                    while(*b == '\\') { bs++; b--; }
                    if (bs % 2 == 0) { cursor++; return; }
                }
                cursor++;
            }
            throw Error("Unterminated string");
        } else if (c == '{' || c == '[') {
            int depth = 0;
            while (cursor + 32 <= end) {
                simd::reg_t chunk = simd::load(cursor);
                simd::reg_t quote = _mm256_set1_epi8('"');
                simd::reg_t lbrace = _mm256_set1_epi8('{');
                simd::reg_t rbrace = _mm256_set1_epi8('}');
                simd::reg_t lbracket = _mm256_set1_epi8('[');
                simd::reg_t rbracket = _mm256_set1_epi8(']');

                simd::reg_t is_q = _mm256_cmpeq_epi8(chunk, quote);
                simd::reg_t is_lb = _mm256_cmpeq_epi8(chunk, lbrace);
                simd::reg_t is_rb = _mm256_cmpeq_epi8(chunk, rbrace);
                simd::reg_t is_lbr = _mm256_cmpeq_epi8(chunk, lbracket);
                simd::reg_t is_rbr = _mm256_cmpeq_epi8(chunk, rbracket);

                simd::reg_t hit = _mm256_or_si256(_mm256_or_si256(is_lb, is_rb),
                                                  _mm256_or_si256(is_lbr, is_rbr));
                hit = _mm256_or_si256(hit, is_q);

                uint32_t mask = simd::movemask(hit);

                while (mask) {
                    int idx = std::countr_zero(mask);
                    char cc = cursor[idx];

                    if (cc == '"') {
                        cursor += idx;
                        cursor++;
                        while (cursor < end) {
                            if (*cursor == '"') {
                                int bs = 0;
                                const char* b = cursor - 1;
                                while(*b == '\\') { bs++; b--; }
                                if (bs % 2 == 0) { cursor++; break; }
                            }
                            cursor++;
                        }
                        goto next_chunk_container;
                    } else if (cc == '{' || cc == '[') {
                        depth++;
                    } else if (cc == '}' || cc == ']') {
                        depth--;
                    }

                    if (depth == 0) {
                        cursor += idx + 1;
                        return;
                    }
                    mask &= ~(1 << idx);
                }
                cursor += 32;
                next_chunk_container:;
            }
            while (cursor < end) {
                char cc = *cursor;
                if (cc == '"') {
                    cursor++;
                    while (cursor < end) {
                        if (*cursor == '"' && *(cursor-1) != '\\') { cursor++; break; }
                        cursor++;
                    }
                } else if (cc == '{' || cc == '[') depth++;
                else if (cc == '}' || cc == ']') {
                    depth--;
                    if (depth == 0) { cursor++; return; }
                } else {
                    cursor++;
                }
            }
        } else {
            // Scalar scan
            while (cursor + 32 <= end) {
                simd::reg_t chunk = simd::load(cursor);
                simd::reg_t comma = _mm256_set1_epi8(',');
                simd::reg_t rbrace = _mm256_set1_epi8('}');
                simd::reg_t rbracket = _mm256_set1_epi8(']');

                simd::reg_t space = _mm256_set1_epi8(0x20);
                simd::reg_t nl = _mm256_set1_epi8(0x0A);
                simd::reg_t cr = _mm256_set1_epi8(0x0D);
                simd::reg_t tab = _mm256_set1_epi8(0x09);

                simd::reg_t match = _mm256_or_si256(_mm256_or_si256(_mm256_cmpeq_epi8(chunk, comma), _mm256_cmpeq_epi8(chunk, rbrace)), _mm256_cmpeq_epi8(chunk, rbracket));
                simd::reg_t ws_match = _mm256_or_si256(_mm256_or_si256(_mm256_cmpeq_epi8(chunk, space), _mm256_cmpeq_epi8(chunk, tab)), _mm256_or_si256(_mm256_cmpeq_epi8(chunk, nl), _mm256_cmpeq_epi8(chunk, cr)));

                uint32_t mask = simd::movemask(_mm256_or_si256(match, ws_match));

                if (mask) {
                    cursor += std::countr_zero(mask);
                    return;
                }
                cursor += 32;
            }
            while (cursor < end) {
                char cc = *cursor;
                if (cc == ',' || cc == '}' || cc == ']' || cc <= 0x20) return;
                cursor++;
            }
        }
    }

    char peek() const { if (cursor >= end) throw Error("Unexpected EOF"); return *cursor; }
    void consume(char expected) { skip_whitespace(); if (cursor >= end || *cursor != expected) throw Error("Expected char"); cursor++; }
};

namespace Apex {
    constexpr uint64_t fnv1a(std::string_view s, uint64_t seed) {
        uint64_t h = seed;
        for (char c : s) { h ^= static_cast<uint8_t>(c); h *= 0x100000001b3; }
        return h;
    }

    template <size_t N>
    constexpr size_t next_pow2() {
        size_t s = N;
        if (s == 0) return 1;
        s--;
        s |= s >> 1; s |= s >> 2; s |= s >> 4; s |= s >> 8; s |= s >> 16; s |= s >> 32;
        return s + 1;
    }

    template <size_t N>
    struct MPHF {
        std::array<std::string_view, N> keys;
        static constexpr size_t Size = next_pow2<N>() * 2;
        std::array<uint8_t, Size> map;
        uint64_t seed = 0;

        constexpr MPHF(std::array<std::string_view, N> k) : keys(k), map{} {
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
                if (ok) {
                    seed = s;
                    map = temp_map;
                    return;
                }
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

template <typename T> struct Meta;
template<typename T> void read(T& val, Scanner& s);

template<typename T> requires std::is_arithmetic_v<T>
void read(T& val, Scanner& s) {
    s.skip_whitespace();
    if ((std::isdigit(*s.cursor) || *s.cursor == '-')) {
        auto res = std::from_chars(s.cursor, s.end, val);
        if (res.ec != std::errc()) throw Error("Number parse error");
        s.cursor = res.ptr;
    } else {
        throw Error("Expected number");
    }
}

template<> inline void read(std::string& val, Scanner& s) {
    s.skip_whitespace();
    char buf[1024];
    val = s.scan_string(buf, 1024);
}
template<> inline void read(bool& val, Scanner& s) {
    s.skip_whitespace();
    if (*s.cursor == 't') { s.cursor += 4; val = true; } else { s.cursor += 5; val = false; }
}

template<typename T> void read(std::vector<T>& val, Scanner& s) {
    s.skip_whitespace();
    if (s.peek() == '[') {
        s.consume('[');
        val.clear();
        if (val.capacity() < 32) val.reserve(32);
        s.skip_whitespace();
        if (s.peek() == ']') { s.consume(']'); return; }
        while (true) {
            val.emplace_back();
            read(val.back(), s);
            s.skip_whitespace();
            char c = s.peek();
            if (c == ']') { s.consume(']'); break; }
            if (c == ',') { s.consume(','); continue; }
            throw Error("Expected ] or ,");
        }
    } else throw Error("Expected [");
}

template<typename T> void read(std::optional<T>& val, Scanner& s) {
    s.skip_whitespace();
    if (s.peek() == 'n') { s.cursor += 4; val.reset(); }
    else { val.emplace(); read(*val, s); }
}

template<typename K, typename V> void read(std::map<K, V>& val, Scanner& s) {
    s.skip_whitespace();
    if (s.peek() == '{') {
        s.consume('{');
        val.clear();
        s.skip_whitespace();
        if (s.peek() == '}') { s.consume('}'); return; }
        while (true) {
            K key; read(key, s);
            s.skip_whitespace(); s.consume(':');
            read(val[key], s);
            s.skip_whitespace();
            char c = s.peek();
            if (c == '}') { s.consume('}'); break; }
            if (c == ',') { s.consume(','); continue; }
            throw Error("Expected } or ,");
        }
    } else throw Error("Expected {");
}

template <typename T>
void read_struct(T& obj, Scanner& s) {
    s.skip_whitespace();
    if (s.peek() != '{') throw Error("Expected {");
    s.consume('{');
    constexpr auto& meta = Meta<T>::info;
    char key_buf[128];
    s.skip_whitespace();
    if (s.peek() == '}') { s.consume('}'); return; }
    while (true) {
        s.skip_whitespace();
        std::string_view key = s.scan_string(key_buf, 128);
        s.skip_whitespace(); s.consume(':');
        size_t idx = meta.mphf.index(key);
        if (idx < meta.mphf.keys.size() && meta.mphf.keys[idx] == key) {
            Meta<T>::dispatch(obj, idx, s);
        } else {
            s.skip_value();
        }
        s.skip_whitespace();
        char c = s.peek();
        if (c == '}') { s.consume('}'); break; }
        if (c == ',') { s.consume(','); continue; }
        throw Error("Expected } or ,");
    }
}

struct struct_info_base {};
template<typename Hash> struct struct_info : struct_info_base {
    Hash mphf;
    constexpr struct_info(Hash h) : mphf(h) {}
};

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

#define TACHYON_GET_MACRO_HDL TACHYON_GET_MACRO_STR

#define TACHYON_HDL_1(T, x) +[](T& o, Scanner& s) { read(o.x, s); }
#define TACHYON_HDL_2(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_1(T, __VA_ARGS__)
#define TACHYON_HDL_3(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_2(T, __VA_ARGS__)
#define TACHYON_HDL_4(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_3(T, __VA_ARGS__)
#define TACHYON_HDL_5(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_4(T, __VA_ARGS__)
#define TACHYON_HDL_6(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_5(T, __VA_ARGS__)
#define TACHYON_HDL_7(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_6(T, __VA_ARGS__)
#define TACHYON_HDL_8(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_7(T, __VA_ARGS__)
#define TACHYON_HDL_9(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_8(T, __VA_ARGS__)
#define TACHYON_HDL_10(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_9(T, __VA_ARGS__)
#define TACHYON_HDL_11(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_10(T, __VA_ARGS__)
#define TACHYON_HDL_12(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_11(T, __VA_ARGS__)
#define TACHYON_HDL_13(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_12(T, __VA_ARGS__)
#define TACHYON_HDL_14(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_13(T, __VA_ARGS__)
#define TACHYON_HDL_15(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_14(T, __VA_ARGS__)
#define TACHYON_HDL_16(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_15(T, __VA_ARGS__)
#define TACHYON_HDL_17(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_16(T, __VA_ARGS__)
#define TACHYON_HDL_18(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_17(T, __VA_ARGS__)
#define TACHYON_HDL_19(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_18(T, __VA_ARGS__)
#define TACHYON_HDL_20(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_19(T, __VA_ARGS__)
#define TACHYON_HDL_21(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_20(T, __VA_ARGS__)
#define TACHYON_HDL_22(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_21(T, __VA_ARGS__)
#define TACHYON_HDL_23(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_22(T, __VA_ARGS__)
#define TACHYON_HDL_24(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_23(T, __VA_ARGS__)
#define TACHYON_HDL_25(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_24(T, __VA_ARGS__)
#define TACHYON_HDL_26(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_25(T, __VA_ARGS__)
#define TACHYON_HDL_27(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_26(T, __VA_ARGS__)
#define TACHYON_HDL_28(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_27(T, __VA_ARGS__)
#define TACHYON_HDL_29(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_28(T, __VA_ARGS__)
#define TACHYON_HDL_30(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_29(T, __VA_ARGS__)
#define TACHYON_HDL_31(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_30(T, __VA_ARGS__)
#define TACHYON_HDL_32(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_31(T, __VA_ARGS__)
#define TACHYON_HDL_33(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_32(T, __VA_ARGS__)
#define TACHYON_HDL_34(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_33(T, __VA_ARGS__)
#define TACHYON_HDL_35(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_34(T, __VA_ARGS__)
#define TACHYON_HDL_36(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_35(T, __VA_ARGS__)
#define TACHYON_HDL_37(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_36(T, __VA_ARGS__)
#define TACHYON_HDL_38(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_37(T, __VA_ARGS__)
#define TACHYON_HDL_39(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_38(T, __VA_ARGS__)
#define TACHYON_HDL_40(T, x, ...) +[](T& o, Scanner& s) { read(o.x, s); }, TACHYON_HDL_39(T, __VA_ARGS__)

#define TACHYON_HDL_ALL(T, ...) TACHYON_GET_MACRO_HDL(__VA_ARGS__, TACHYON_HDL_40, TACHYON_HDL_39, TACHYON_HDL_38, TACHYON_HDL_37, TACHYON_HDL_36, TACHYON_HDL_35, TACHYON_HDL_34, TACHYON_HDL_33, TACHYON_HDL_32, TACHYON_HDL_31, TACHYON_HDL_30, TACHYON_HDL_29, TACHYON_HDL_28, TACHYON_HDL_27, TACHYON_HDL_26, TACHYON_HDL_25, TACHYON_HDL_24, TACHYON_HDL_23, TACHYON_HDL_22, TACHYON_HDL_21, TACHYON_HDL_20, TACHYON_HDL_19, TACHYON_HDL_18, TACHYON_HDL_17, TACHYON_HDL_16, TACHYON_HDL_15, TACHYON_HDL_14, TACHYON_HDL_13, TACHYON_HDL_12, TACHYON_HDL_11, TACHYON_HDL_10, TACHYON_HDL_9, TACHYON_HDL_8, TACHYON_HDL_7, TACHYON_HDL_6, TACHYON_HDL_5, TACHYON_HDL_4, TACHYON_HDL_3, TACHYON_HDL_2, TACHYON_HDL_1)(T, __VA_ARGS__)

#define TACHYON_DEFINE_TYPE(Type, ...) \
    namespace Tachyon { \
    template <> struct Meta<Type> { \
        static constexpr auto names = std::array{ \
            TACHYON_STR_ALL(__VA_ARGS__) \
        }; \
        static constexpr Apex::MPHF<names.size()> mphf{names}; \
        static constexpr auto info = struct_info{mphf}; \
        using Handler = void(*)(Type&, Scanner&); \
        static void dispatch(Type& obj, size_t idx, Scanner& s) { \
            static constexpr std::array<Handler, names.size()> handlers = { \
                TACHYON_HDL_ALL(Type, __VA_ARGS__) \
            }; \
            handlers[idx](obj, s); \
        } \
    }; \
    template<> inline void read<Type>(Type& val, Scanner& s) { \
        read_struct(val, s); \
    } \
    }

} // namespace Tachyon

#endif // TACHYON_HPP
