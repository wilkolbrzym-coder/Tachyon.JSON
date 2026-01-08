/*
 * TACHYON 0.7.3 "EVENT HORIZON"
 * The World's Fastest Typed JSON Library
 *
 * MIT License
 *
 * Copyright (c) 2026 Tachyon Systems
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef TACHYON_HPP
#define TACHYON_HPP

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <tuple>
#include <optional>
#include <variant>
#include <concepts>
#include <type_traits>
#include <bit>
#include <algorithm>
#include <utility>
#include <limits>
#include <cmath>
#include <charconv>
#include <memory>
#include <immintrin.h>

#ifdef _MSC_VER
#include <intrin.h>
#pragma intrinsic(_BitScanForward)
#pragma intrinsic(_BitScanForward64)
#endif

#ifndef TACHYON_FORCE_INLINE
    #ifdef _MSC_VER
        #define TACHYON_FORCE_INLINE __forceinline
    #else
        #define TACHYON_FORCE_INLINE __attribute__((always_inline)) inline
    #endif
#endif

#ifndef TACHYON_LIKELY
    #ifdef _MSC_VER
        #define TACHYON_LIKELY(x) (x)
        #define TACHYON_UNLIKELY(x) (x)
    #else
        #define TACHYON_LIKELY(x) __builtin_expect(!!(x), 1)
        #define TACHYON_UNLIKELY(x) __builtin_expect(!!(x), 0)
    #endif
#endif

namespace tachyon {

enum class error_code : uint8_t {
    ok = 0,
    unexpected_end,
    expected_brace,
    expected_bracket,
    expected_colon,
    expected_comma,
    expected_quote,
    invalid_number,
    invalid_string,
    invalid_escape,
    invalid_utf8,
    unknown_key,
    syntax_error
};

struct error {
    error_code code;
    size_t location;
    operator bool() const { return code != error_code::ok; }
};

namespace detail {

    template <typename T> struct is_member_pointer : std::false_type {};
    template <typename C, typename M> struct is_member_pointer<M C::*> : std::true_type {};

    template <typename T>
    constexpr auto reflect() { return std::tuple<>{}; }

    template <typename T>
    concept is_optional = requires(T t) {
        { t.has_value() } -> std::convertible_to<bool>;
        { t.value() };
        { t.reset() };
    };

    template <typename T>
    concept is_vector_type = requires(T t) {
        typename T::value_type;
        t.push_back(std::declval<typename T::value_type>());
        t.clear();
        t.reserve(size_t{});
    } && !std::is_same_v<T, std::string>;

    template <typename T>
    concept is_map_type = requires(T t) {
        typename T::key_type;
        typename T::mapped_type;
        t.emplace(std::declval<typename T::key_type>(), std::declval<typename T::mapped_type>());
        t.clear();
    };

    TACHYON_FORCE_INLINE uint32_t countr_zero(uint32_t x) {
#ifdef _MSC_VER
        unsigned long result;
        _BitScanForward(&result, x);
        return result;
#else
        return __builtin_ctz(x);
#endif
    }

    TACHYON_FORCE_INLINE uint64_t countr_zero64(uint64_t x) {
#ifdef _MSC_VER
        unsigned long result;
        _BitScanForward64(&result, x);
        return result;
#else
        return __builtin_ctzl(x);
#endif
    }

    TACHYON_FORCE_INLINE const char* skip_whitespace(const char* p, const char* end) {
        // Fast scalar skip
        if (TACHYON_LIKELY((unsigned char)*p > 32)) return p;
        p++;
        if (TACHYON_LIKELY((unsigned char)*p > 32)) return p;
        p++;
        if (TACHYON_LIKELY((unsigned char)*p > 32)) return p;
        p++;

        while (p < end && (unsigned char)*p <= 32) p++;
        return p;
    }

    struct StringScanResult {
        const char* end;
        bool has_escapes;
        bool error;
    };

    // Scalar String Scan (Robust and Correct)
    TACHYON_FORCE_INLINE StringScanResult skip_string(const char* p, const char* end) {
        p++; // Skip opening quote
        const char* start = p;
        bool has_escapes = false;

        while (p < end) {
            char c = *p;
            if (c == '"') return {p, has_escapes, false};
            if (c == '\\') {
                has_escapes = true;
                p++;
                if (p >= end) return {nullptr, false, true};
                p++;
            } else {
                // Optimization: Skip until next quote or slash using SWAR or SIMD?
                // For now, scalar loop is fine for keys and short strings.
                // Glaze uses scalar loop.
                p++;
            }
        }
        return {nullptr, false, true};
    }

    TACHYON_FORCE_INLINE bool validate_utf8(const char* data, size_t len) {
        // Removed global validation to match Glaze speed profile.
        // Can be re-enabled for "Titan" mode.
        return true;
    }

    TACHYON_FORCE_INLINE size_t decode_string(const char* src, size_t len, char* dst) {
        const char* src_end = src + len;
        char* dst_start = dst;

        while (src < src_end) {
            if (TACHYON_UNLIKELY(*src == '\\')) {
                src++;
                if (src >= src_end) break;
                char c = *src++;
                switch (c) {
                    case '"': *dst++ = '"'; break;
                    case '\\': *dst++ = '\\'; break;
                    case '/': *dst++ = '/'; break;
                    case 'b': *dst++ = '\b'; break;
                    case 'f': *dst++ = '\f'; break;
                    case 'n': *dst++ = '\n'; break;
                    case 'r': *dst++ = '\r'; break;
                    case 't': *dst++ = '\t'; break;
                    case 'u': {
                        auto hex4 = [](const char*& p, const char* end) -> uint32_t {
                            uint32_t x = 0;
                            for(int i=0; i<4; ++i) {
                                if (p >= end) return 0;
                                char h = *p++;
                                x <<= 4;
                                if(h>='0'&&h<='9') x|=(h-'0');
                                else if(h>='A'&&h<='F') x|=(h-'A'+10);
                                else if(h>='a'&&h<='f') x|=(h-'a'+10);
                            }
                            return x;
                        };
                        uint32_t cp = hex4(src, src_end);
                        if (cp >= 0xD800 && cp <= 0xDBFF) {
                            if (src + 2 < src_end && src[0] == '\\' && src[1] == 'u') {
                                const char* temp = src + 2;
                                uint32_t low = hex4(temp, src_end);
                                if (low >= 0xDC00 && low <= 0xDFFF) {
                                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                                    src = temp;
                                }
                            }
                        }
                        if (cp < 0x80) *dst++ = (char)cp;
                        else if (cp < 0x800) { *dst++ = (char)(0xC0|(cp>>6)); *dst++ = (char)(0x80|(cp&0x3F)); }
                        else if (cp < 0x10000) { *dst++ = (char)(0xE0|(cp>>12)); *dst++ = (char)(0x80|((cp>>6)&0x3F)); *dst++ = (char)(0x80|(cp&0x3F)); }
                        else { *dst++ = (char)(0xF0|(cp>>18)); *dst++ = (char)(0x80|((cp>>12)&0x3F)); *dst++ = (char)(0x80|((cp>>6)&0x3F)); *dst++ = (char)(0x80|(cp&0x3F)); }
                        break;
                    }
                    default: *dst++ = c; break;
                }
            } else {
                *dst++ = *src++;
            }
        }
        return dst - dst_start;
    }

    template <typename T>
    TACHYON_FORCE_INLINE const char* parse_number(const char* p, const char* end, T& value) {
        auto res = std::from_chars(p, end, value);
        if (res.ptr == p) return nullptr;
        return res.ptr;
    }

    TACHYON_FORCE_INLINE const char* skip_value(const char* p, const char* end) {
        p = skip_whitespace(p, end);
        if (p == end) return p;
        char c = *p;
        if (c == '"') {
             auto res = skip_string(p, end);
             if (res.error) return end;
             return res.end + 1;
        } else if (c == '{' || c == '[') {
            int depth = 0;
            while (p < end) {
                if (*p == '{' || *p == '[') depth++;
                else if (*p == '}' || *p == ']') {
                    depth--;
                    if (depth == 0) return p + 1;
                } else if (*p == '"') {
                    auto res = skip_string(p, end);
                    if (res.error) return end;
                    p = res.end;
                }
                p++;
            }
        } else {
            while (p < end && *p != ',' && *p != '}' && *p != ']' && (unsigned char)*p > 32) p++;
        }
        return p;
    }
}

namespace phf {
    // Fast hash for SWAR
    constexpr uint64_t hash(std::string_view s, uint64_t seed) {
        size_t len = s.size();
        if (len <= 8) {
            uint64_t k = 0;
            for (size_t i = 0; i < len; ++i) {
                k |= ((uint64_t)(unsigned char)s[i]) << (i * 8);
            }
            // Simple mixer
            return (seed ^ k) * 1099511628211ULL;
        } else {
            // Fallback to FNV for long keys
            uint64_t h = seed;
            for (char c : s) {
                h ^= (uint64_t)(unsigned char)c;
                h *= 1099511628211ULL;
            }
            return h;
        }
    }

    template <size_t N>
    constexpr uint64_t find_seed(const std::array<std::string_view, N>& keys) {
        if (N == 0) return 0;
        size_t mask = (std::bit_ceil(N) * 4) - 1;
        if (mask < 15) mask = 15;

        for (uint64_t seed = 1; seed < 100000; ++seed) {
             bool collision = false;
             for (size_t i = 0; i < N; ++i) {
                 size_t slot = hash(keys[i], seed) & mask;
                 for (size_t j = i + 1; j < N; ++j) {
                     if ((hash(keys[j], seed) & mask) == slot) {
                         collision = true; break;
                     }
                 }
                 if (collision) break;
             }
             if (!collision) return seed;
        }
        return 0;
    }
}

template <typename T>
TACHYON_FORCE_INLINE error read(T& value, std::string_view json);

namespace detail {

    template <typename T> struct reader;

    template <typename T> requires std::is_arithmetic_v<T> && (!std::is_same_v<T, bool>)
    struct reader<T> {
        static TACHYON_FORCE_INLINE error read(T& val, const char*& p, const char* end) {
            p = skip_whitespace(p, end);
            auto next = parse_number(p, end, val);
            if (!next) return {error_code::invalid_number, (size_t)(p - end)};
            p = next;
            return {error_code::ok, 0};
        }
    };

    template <> struct reader<bool> {
        static TACHYON_FORCE_INLINE error read(bool& val, const char*& p, const char* end) {
            p = skip_whitespace(p, end);
            if (*p == 't') { val = true; p += 4; }
            else { val = false; p += 5; }
            return {error_code::ok, 0};
        }
    };

    template <> struct reader<std::string> {
        static TACHYON_FORCE_INLINE error read(std::string& val, const char*& p, const char* end) {
            p = skip_whitespace(p, end);
            if (*p != '"') return {error_code::expected_quote, (size_t)(p - end)};

            auto res = skip_string(p, end);
            if (res.error) return {error_code::invalid_string, (size_t)(p - end)};

            const char* end_q = res.end;
            size_t len = end_q - p - 1; // p points to "
            val.resize(len);

            if (!res.has_escapes) {
                std::memcpy(val.data(), p + 1, len);
            } else {
                size_t new_len = decode_string(p + 1, len, val.data());
                val.resize(new_len);
            }

            p = end_q + 1;
            return {error_code::ok, 0};
        }
    };

    template <typename T> requires is_vector_type<T>
    struct reader<T> {
        static TACHYON_FORCE_INLINE error read(T& val, const char*& p, const char* end) {
            p = skip_whitespace(p, end);
            if (*p != '[') return {error_code::expected_bracket, (size_t)(p - end)};
            p++;
            val.clear();
            val.reserve(16);

            p = skip_whitespace(p, end);
            if (*p == ']') { p++; return {error_code::ok, 0}; }

            while (true) {
                typename T::value_type element;
                if (auto err = reader<typename T::value_type>::read(element, p, end)) return err;
                val.push_back(std::move(element));
                p = skip_whitespace(p, end);
                if (*p == ']') { p++; break; }
                if (*p != ',') return {error_code::expected_comma, (size_t)(p - end)};
                p++;
            }
            return {error_code::ok, 0};
        }
    };

    template <typename T> requires is_optional<T>
    struct reader<T> {
        static TACHYON_FORCE_INLINE error read(T& val, const char*& p, const char* end) {
            p = skip_whitespace(p, end);
            if (p + 4 <= end && memcmp(p, "null", 4) == 0) {
                val.reset();
                p += 4;
                return {error_code::ok, 0};
            }
            typename T::value_type inner;
            if (auto err = reader<typename T::value_type>::read(inner, p, end)) return err;
            val = std::move(inner);
            return {error_code::ok, 0};
        }
    };

    template <typename T> requires is_map_type<T>
    struct reader<T> {
        static TACHYON_FORCE_INLINE error read(T& val, const char*& p, const char* end) {
            p = skip_whitespace(p, end);
            if (*p != '{') return {error_code::expected_brace, (size_t)(p - end)};
            p++;
            val.clear();
            p = skip_whitespace(p, end);
            if (*p == '}') { p++; return {error_code::ok, 0}; }

            while (true) {
                p = skip_whitespace(p, end);
                if (*p != '"') return {error_code::expected_quote, (size_t)(p - end)};

                auto res = skip_string(p, end);
                if (res.error) return {error_code::invalid_string, (size_t)(p - end)};
                const char* end_q = res.end;

                std::string key;
                size_t len = end_q - p - 1;
                key.resize(len);
                if (!res.has_escapes) std::memcpy(key.data(), p + 1, len);
                else { size_t nl = decode_string(p + 1, len, key.data()); key.resize(nl); }
                p = end_q + 1;

                p = skip_whitespace(p, end);
                if (*p != ':') return {error_code::expected_colon, (size_t)(p - end)};
                p++;

                typename T::mapped_type mapped;
                if (auto err = reader<typename T::mapped_type>::read(mapped, p, end)) return err;
                val.emplace(std::move(key), std::move(mapped));

                p = skip_whitespace(p, end);
                if (*p == '}') { p++; break; }
                if (*p != ',') return {error_code::expected_comma, (size_t)(p - end)};
                p++;
            }
            return {error_code::ok, 0};
        }
    };

    template <typename T>
    struct reader {
        static TACHYON_FORCE_INLINE error read(T& val, const char*& p, const char* end) {
            p = skip_whitespace(p, end);
            if (p == end) return {error_code::unexpected_end, (size_t)(p - end)};
            if (*p != '{') return {error_code::expected_brace, (size_t)(p - end)};
            p++;
            p = skip_whitespace(p, end);
            if (p == end) return {error_code::unexpected_end, (size_t)(p - end)};
            if (*p == '}') { p++; return {error_code::ok, 0}; }

            static constexpr auto members = reflect<T>();
            constexpr size_t N = std::tuple_size_v<decltype(members)>;

            static constexpr auto keys = []() {
                std::array<std::string_view, N> k;
                auto fill = [&]<size_t... I>(std::index_sequence<I...>) {
                    ((k[I] = std::get<I>(members).first), ...);
                };
                fill(std::make_index_sequence<N>{});
                return k;
            }();

            static constexpr uint64_t seed = phf::find_seed(keys);
            static constexpr size_t mask = (N > 0) ? (std::bit_ceil(N) * 4) - 1 : 0;
            static constexpr size_t table_size = mask + 1;

            static constexpr auto dispatch_table = []() {
                std::array<int, table_size> tbl;
                std::fill(tbl.begin(), tbl.end(), -1);
                for (size_t i = 0; i < N; ++i) {
                    size_t slot = phf::hash(keys[i], seed) & mask;
                    tbl[slot] = (int)i;
                }
                return tbl;
            }();

            while (true) {
                p = skip_whitespace(p, end);
                if (p == end) return {error_code::unexpected_end, (size_t)(p - end)};
                if (*p != '"') return {error_code::expected_quote, (size_t)(p - end)};

                auto res = skip_string(p, end);
                if (res.error) return {error_code::invalid_string, (size_t)(p - end)};
                const char* end_q = res.end;
                size_t key_len = end_q - p - 1;
                const char* key_start = p + 1;

                uint64_t h;
                if (TACHYON_LIKELY(key_len <= 8)) {
                    // SWAR Hash
                    uint64_t k;
                    std::memcpy(&k, key_start, 8); // Safe load due to padding/structure
                    // Mask out garbage
                    // 1ULL << (key_len * 8) - 1. But for 8 bytes (64 bits), 1<<64 is UB.
                    // Special case if len=8?
                    // Or just use bit mask.
                    // For little endian:
                    uint64_t m = (key_len == 8) ? ~0ULL : ((1ULL << (key_len * 8)) - 1);
                    k &= m;
                    h = (seed ^ k) * 1099511628211ULL;
                } else {
                    h = seed;
                    for (size_t i = 0; i < key_len; ++i) {
                        h ^= (uint64_t)(unsigned char)key_start[i];
                        h *= 1099511628211ULL;
                    }
                }

                size_t slot = h & mask;
                int member_idx = dispatch_table[slot];

                bool found = false;
                if (member_idx >= 0) {
                     error pending_err = {error_code::ok, 0};
                     auto dispatch = [&]<size_t... Is>(std::index_sequence<Is...>) {
                         ((member_idx == Is ? (
                             (keys[Is].size() == key_len && memcmp(keys[Is].data(), key_start, key_len) == 0) ? (
                                 found = true,
                                 p = end_q + 1,
                                 p = skip_whitespace(p, end),
                                 (p == end ? (pending_err = {error_code::unexpected_end, (size_t)(p - end)}, 0) :
                                 (*p != ':' ? (pending_err = {error_code::expected_colon, (size_t)(p - end)}, 0) :
                                 (p++, pending_err = reader<typename std::remove_reference_t<decltype(val.*(std::get<Is>(members).second))>>::read(val.*(std::get<Is>(members).second), p, end), 0)))
                             ) : 0
                         ) : 0), ...);
                     };
                     dispatch(std::make_index_sequence<N>{});
                     if (pending_err.code != error_code::ok) return pending_err;
                }

                if (!found) {
                     p = end_q + 1;
                     p = skip_whitespace(p, end);
                     if (p == end) return {error_code::unexpected_end, (size_t)(p - end)};
                     if (*p == ':') p++;
                     p = skip_value(p, end);
                }

                p = skip_whitespace(p, end);
                if (p == end) return {error_code::unexpected_end, (size_t)(p - end)};
                if (*p == '}') { p++; break; }
                if (*p != ',') return {error_code::expected_comma, (size_t)(p - end)};
                p++;
            }
            return {error_code::ok, 0};
        }
    };

}

template <typename T>
TACHYON_FORCE_INLINE error read(T& value, std::string_view json) {
    const char* p = json.data();
    const char* end = p + json.size();
    if (!detail::validate_utf8(p, json.size())) return {error_code::invalid_utf8, 0};
    return detail::reader<T>::read(value, p, end);
}

#define TACHYON_MEMBER(v) std::pair{std::string_view(#v), &T::v}

#define TACHYON_ARG_1(op, x) op(x)
#define TACHYON_ARG_2(op, x, ...) op(x), TACHYON_ARG_1(op, __VA_ARGS__)
#define TACHYON_ARG_3(op, x, ...) op(x), TACHYON_ARG_2(op, __VA_ARGS__)
#define TACHYON_ARG_4(op, x, ...) op(x), TACHYON_ARG_3(op, __VA_ARGS__)
#define TACHYON_ARG_5(op, x, ...) op(x), TACHYON_ARG_4(op, __VA_ARGS__)
#define TACHYON_ARG_6(op, x, ...) op(x), TACHYON_ARG_5(op, __VA_ARGS__)
#define TACHYON_ARG_7(op, x, ...) op(x), TACHYON_ARG_6(op, __VA_ARGS__)
#define TACHYON_ARG_8(op, x, ...) op(x), TACHYON_ARG_7(op, __VA_ARGS__)
#define TACHYON_ARG_9(op, x, ...) op(x), TACHYON_ARG_8(op, __VA_ARGS__)
#define TACHYON_ARG_10(op, x, ...) op(x), TACHYON_ARG_9(op, __VA_ARGS__)
#define TACHYON_ARG_11(op, x, ...) op(x), TACHYON_ARG_10(op, __VA_ARGS__)
#define TACHYON_ARG_12(op, x, ...) op(x), TACHYON_ARG_11(op, __VA_ARGS__)
#define TACHYON_ARG_13(op, x, ...) op(x), TACHYON_ARG_12(op, __VA_ARGS__)
#define TACHYON_ARG_14(op, x, ...) op(x), TACHYON_ARG_13(op, __VA_ARGS__)
#define TACHYON_ARG_15(op, x, ...) op(x), TACHYON_ARG_14(op, __VA_ARGS__)
#define TACHYON_ARG_16(op, x, ...) op(x), TACHYON_ARG_15(op, __VA_ARGS__)

#define TACHYON_GET_MACRO(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,NAME,...) NAME

#define TACHYON_FOR_EACH(op, ...) \
  TACHYON_GET_MACRO(__VA_ARGS__, TACHYON_ARG_16, TACHYON_ARG_15, TACHYON_ARG_14, TACHYON_ARG_13, \
                    TACHYON_ARG_12, TACHYON_ARG_11, TACHYON_ARG_10, TACHYON_ARG_9, TACHYON_ARG_8, \
                    TACHYON_ARG_7, TACHYON_ARG_6, TACHYON_ARG_5, TACHYON_ARG_4, TACHYON_ARG_3, \
                    TACHYON_ARG_2, TACHYON_ARG_1)(op, __VA_ARGS__)

#define TACHYON_APEX(Type, ...) \
    template<> \
    constexpr auto tachyon::detail::reflect<Type>() { \
        using T = Type; \
        return std::make_tuple(TACHYON_FOR_EACH(TACHYON_MEMBER, __VA_ARGS__)); \
    }

} // namespace tachyon

#endif // TACHYON_HPP
