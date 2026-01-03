#ifndef TACHYON_HPP
#define TACHYON_HPP

#include <iostream>
#include <vector>
#include <string>
#include <string_view>
#include <cstring>
#include <immintrin.h>
#include <bit>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <memory>
#include <map>
#include <variant>
#include <charconv>
#include <initializer_list>
#include <functional>

#ifdef __BMI2__
#include <immintrin.h>
#endif

namespace Tachyon {
    struct Value;
    class Document;
    class Parser;
    enum class Type : uint8_t { Null, False, True, Number, String, Array, Object };
}

namespace Tachyon::ASM {
    // Branchless whitespace skip for small gaps?
    // AVX2 skip?
    // For now, inline scalar is very fast for small gaps.
    inline const char* skip_whitespace(const char* p, const char* end) {
        // Fast skip 4 bytes?
        while (p + 4 <= end) {
            uint32_t v;
            std::memcpy(&v, p, 4);
            // Check if any byte > 32
            // If v has any byte > 32, we stop.
            // Simplified: loop.
            if ((unsigned char)*p > 32) return p; p++;
            if ((unsigned char)*p > 32) return p; p++;
            if ((unsigned char)*p > 32) return p; p++;
            if ((unsigned char)*p > 32) return p; p++;
        }
        while (p < end && (unsigned char)*p <= 32) p++;
        return p;
    }
}

namespace Tachyon::SIMD {
    // V12: Unrolled 64-byte loop
    inline size_t compute_structural_mask(const char* data, size_t len, uint32_t* mask_array) {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
        size_t i = 0;
        size_t block_idx = 0;

        __m256i v_quote = _mm256_set1_epi8('"');
        __m256i v_backslash = _mm256_set1_epi8('\\');
        __m256i v_lbrace = _mm256_set1_epi8('{');
        __m256i v_rbrace = _mm256_set1_epi8('}');
        __m256i v_lbracket = _mm256_set1_epi8('[');
        __m256i v_rbracket = _mm256_set1_epi8(']');
        __m256i v_colon = _mm256_set1_epi8(':');
        __m256i v_comma = _mm256_set1_epi8(',');

        uint32_t in_string_mask = 0;
        uint64_t prev_escapes = 0;

        // Unrolled Loop (64 bytes per step)
        for (; i + 64 <= len; i += 64) {
            // Prefetch ahead
            _mm_prefetch(reinterpret_cast<const char*>(p + i + 128), _MM_HINT_T0);

            // Block 1 (0-31)
            __m256i chunk1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p + i));

            // Block 2 (32-63)
            __m256i chunk2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p + i + 32));

            // Process Block 1
            {
                __m256i cmp_bs = _mm256_cmpeq_epi8(chunk1, v_backslash);
                uint32_t bs_mask = _mm256_movemask_epi8(cmp_bs);
                uint32_t quote_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk1, v_quote));

                if (__builtin_expect(bs_mask != 0 || prev_escapes > 0, 0)) {
                     uint32_t real_quote_mask = 0;
                     const char* c_ptr = reinterpret_cast<const char*>(&chunk1);
                     for(int j=0; j<32; ++j) {
                         char c = c_ptr[j];
                         if ((c == '"') && ((prev_escapes & 1) == 0)) real_quote_mask |= (1U << j);
                         if (c == '\\') prev_escapes++; else prev_escapes = 0;
                     }
                     quote_mask = real_quote_mask;
                } else {
                    prev_escapes = 0;
                }

                uint32_t prefix = quote_mask;
                prefix ^= (prefix << 1); prefix ^= (prefix << 2); prefix ^= (prefix << 4); prefix ^= (prefix << 8); prefix ^= (prefix << 16);
                if (in_string_mask) prefix = ~prefix;
                if (std::popcount(quote_mask) % 2 != 0) in_string_mask = !in_string_mask;

                __m256i s = _mm256_cmpeq_epi8(chunk1, v_lbrace);
                s = _mm256_or_si256(s, _mm256_cmpeq_epi8(chunk1, v_rbrace));
                s = _mm256_or_si256(s, _mm256_cmpeq_epi8(chunk1, v_lbracket));
                s = _mm256_or_si256(s, _mm256_cmpeq_epi8(chunk1, v_rbracket));
                s = _mm256_or_si256(s, _mm256_cmpeq_epi8(chunk1, v_colon));
                s = _mm256_or_si256(s, _mm256_cmpeq_epi8(chunk1, v_comma));

                uint32_t struct_mask = _mm256_movemask_epi8(s);
                uint32_t interior = prefix & ~quote_mask;
                mask_array[block_idx++] = (struct_mask | quote_mask) & ~interior;
            }

            // Process Block 2
            {
                __m256i cmp_bs = _mm256_cmpeq_epi8(chunk2, v_backslash);
                uint32_t bs_mask = _mm256_movemask_epi8(cmp_bs);
                uint32_t quote_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk2, v_quote));

                if (__builtin_expect(bs_mask != 0 || prev_escapes > 0, 0)) {
                     uint32_t real_quote_mask = 0;
                     const char* c_ptr = reinterpret_cast<const char*>(&chunk2);
                     for(int j=0; j<32; ++j) {
                         char c = c_ptr[j];
                         if ((c == '"') && ((prev_escapes & 1) == 0)) real_quote_mask |= (1U << j);
                         if (c == '\\') prev_escapes++; else prev_escapes = 0;
                     }
                     quote_mask = real_quote_mask;
                } else {
                    prev_escapes = 0;
                }

                uint32_t prefix = quote_mask;
                prefix ^= (prefix << 1); prefix ^= (prefix << 2); prefix ^= (prefix << 4); prefix ^= (prefix << 8); prefix ^= (prefix << 16);
                if (in_string_mask) prefix = ~prefix;
                if (std::popcount(quote_mask) % 2 != 0) in_string_mask = !in_string_mask;

                __m256i s = _mm256_cmpeq_epi8(chunk2, v_lbrace);
                s = _mm256_or_si256(s, _mm256_cmpeq_epi8(chunk2, v_rbrace));
                s = _mm256_or_si256(s, _mm256_cmpeq_epi8(chunk2, v_lbracket));
                s = _mm256_or_si256(s, _mm256_cmpeq_epi8(chunk2, v_rbracket));
                s = _mm256_or_si256(s, _mm256_cmpeq_epi8(chunk2, v_colon));
                s = _mm256_or_si256(s, _mm256_cmpeq_epi8(chunk2, v_comma));

                uint32_t struct_mask = _mm256_movemask_epi8(s);
                uint32_t interior = prefix & ~quote_mask;
                mask_array[block_idx++] = (struct_mask | quote_mask) & ~interior;
            }
        }

        // Scalar Tail
        if (i < len) {
            uint32_t final_mask = 0;
            for (size_t j = 0; i < len; ++i, ++j) {
                if (j == 32) { // Should not happen if chunks aligned, but reset bit index
                     mask_array[block_idx++] = final_mask;
                     final_mask = 0;
                     j = 0;
                }
                char c = data[i];
                bool is_quote = (c == '"') && ((prev_escapes & 1) == 0);

                if (c == '\\') prev_escapes++;
                else prev_escapes = 0;

                if (in_string_mask) {
                    if (is_quote) {
                        in_string_mask = 0;
                        final_mask |= (1U << j);
                    }
                } else {
                    if (is_quote) {
                        in_string_mask = 1;
                        final_mask |= (1U << j);
                    } else if (c == '{' || c == '}' || c == '[' || c == ']' || c == ':' || c == ',') {
                        final_mask |= (1U << j);
                    }
                }
            }
            mask_array[block_idx++] = final_mask;
        }
        return block_idx;
    }
}

namespace Tachyon {
    class Parser {
    public:
        std::unique_ptr<uint32_t[]> bitmask;
        size_t bitmask_cap = 0;
        size_t bitmask_len = 0;
        const char* base;
        size_t len;

        void parse(const char* data, size_t size) {
            base = data;
            len = size;
            size_t mask_len = (size + 31) / 32;
            if (mask_len > bitmask_cap) {
                bitmask.reset(new uint32_t[mask_len]);
                bitmask_cap = mask_len;
            }
            bitmask_len = SIMD::compute_structural_mask(base, len, bitmask.get());
        }
    };

    struct Cursor {
        uint32_t block_idx;
        uint32_t mask;
        const Parser* parser;

        Cursor(const Parser* p, uint32_t offset) : parser(p) {
            block_idx = offset / 32;
            int bit = offset % 32;
            if (block_idx < parser->bitmask_len) {
                mask = parser->bitmask[block_idx];
                if (bit > 0) mask &= ~((1U << bit) - 1);
            } else {
                mask = 0;
            }
        }

        uint32_t next() {
            while (true) {
                if (mask != 0) {
                    int bit = std::countr_zero(mask);
                    uint32_t offset = block_idx * 32 + bit;
                    mask &= (mask - 1);
                    return offset;
                }
                block_idx++;
                if (block_idx >= parser->bitmask_len) return (uint32_t)-1;
                mask = parser->bitmask[block_idx];
            }
        }
    };

    struct Value {
        const Parser* parser;
        uint32_t offset;

        Value() : parser(nullptr), offset(0) {}
        Value(const Parser* p, uint32_t o) : parser(p), offset(o) {}

        bool is_valid() const { return parser != nullptr && offset < parser->len; }

        char current_char() const {
             const char* s = ASM::skip_whitespace(parser->base + offset, parser->base + parser->len);
             if (s >= parser->base + parser->len) return 0;
             return *s;
        }

        Type type() const {
            char c = current_char();
            if (c == '{') return Type::Object;
            if (c == '[') return Type::Array;
            if (c == '"') return Type::String;
            if (c == 't') return Type::True;
            if (c == 'f') return Type::False;
            if (c == 'n') return Type::Null;
            if ((c >= '0' && c <= '9') || c == '-') return Type::Number;
            return Type::Null;
        }

        bool is_object() const { return type() == Type::Object; }
        bool is_array() const { return type() == Type::Array; }
        bool is_string() const { return type() == Type::String; }
        bool is_number() const { return type() == Type::Number; }
        bool is_bool() const { Type t = type(); return t == Type::True || t == Type::False; }
        bool is_true() const { return type() == Type::True; }
        bool is_false() const { return type() == Type::False; }
        bool is_null() const { return type() == Type::Null; }

        int get_int() const {
            const char* s = ASM::skip_whitespace(parser->base + offset, parser->base + parser->len);
            int i;
            std::from_chars(s, parser->base + parser->len, i);
            return i;
        }

        double get_double() const {
            const char* s = ASM::skip_whitespace(parser->base + offset, parser->base + parser->len);
            double d;
            std::from_chars(s, parser->base + parser->len, d);
            return d;
        }

        std::string_view get_string() const {
            if (!is_string()) return {};
            const char* s = ASM::skip_whitespace(parser->base + offset, parser->base + parser->len);
            uint32_t start = (uint32_t)(s - parser->base);
            Cursor c(parser, start + 1);
            uint32_t end = c.next();
            if (end == (uint32_t)-1) return {};
            return std::string_view(parser->base + start + 1, end - start - 1);
        }

        void skip_container(Cursor& c, char open, char close) const {
            int depth = 1;
            while (depth > 0) {
                uint32_t curr = c.next();
                if (curr == (uint32_t)-1) break;
                char ch = parser->base[curr];
                if (ch == open) depth++;
                else if (ch == close) depth--;
                else if (ch == '"') c.next();
            }
        }

        uint32_t size() const {
            if (!is_array()) return 0;
            const char* s = ASM::skip_whitespace(parser->base + offset, parser->base + parser->len);
            uint32_t start = (uint32_t)(s - parser->base) + 1;

            const char* check = ASM::skip_whitespace(parser->base + start, parser->base + parser->len);
            if (*check == ']') return 0;

            Cursor c(parser, start);
            uint32_t commas = 0;
            while (true) {
                uint32_t curr = c.next();
                if (curr == (uint32_t)-1) break;
                char ch = parser->base[curr];
                if (ch == ']') break;
                if (ch == ',') commas++;
                else if (ch == '{') skip_container(c, '{', '}');
                else if (ch == '[') skip_container(c, '[', ']');
                else if (ch == '"') c.next();
            }
            return commas + 1;
        }

        Value operator[](size_t idx) const {
            if (!is_array()) return {};
            const char* s = ASM::skip_whitespace(parser->base + offset, parser->base + parser->len);
            uint32_t start = (uint32_t)(s - parser->base) + 1;

            const char* check = ASM::skip_whitespace(parser->base + start, parser->base + parser->len);
            if (*check == ']') return {};

            Cursor c(parser, start);
            size_t count = 0;
            uint32_t element_start = start;

            while (true) {
                if (count == idx) return Value(parser, element_start);

                while (true) {
                    uint32_t curr = c.next();
                    if (curr == (uint32_t)-1) return {};
                    char ch = parser->base[curr];
                    if (ch == ']') return {};
                    if (ch == ',') {
                        element_start = curr + 1;
                        count++;
                        break;
                    }
                    if (ch == '{') skip_container(c, '{', '}');
                    else if (ch == '[') skip_container(c, '[', ']');
                    else if (ch == '"') c.next();
                }
            }
            return {};
        }

        Value operator[](std::string_view key) const {
            if (!is_object()) return {};
             const char* s = ASM::skip_whitespace(parser->base + offset, parser->base + parser->len);
            uint32_t start = (uint32_t)(s - parser->base) + 1;

            Cursor c(parser, start);
            while (true) {
                uint32_t curr = c.next();
                if (curr == (uint32_t)-1) return {};
                char ch = parser->base[curr];
                if (ch == '}') return {};
                if (ch == ',') continue;

                if (ch == '"') {
                    uint32_t end_q = c.next();
                    size_t k_len = end_q - curr - 1;
                    std::string_view k(parser->base + curr + 1, k_len);

                    uint32_t colon = c.next();

                    if (k == key) {
                        return Value(parser, colon + 1);
                    }

                    const char* v_s = ASM::skip_whitespace(parser->base + colon + 1, parser->base + parser->len);
                    char v_ch = *v_s;

                    if (v_ch == '{') {
                        uint32_t tmp = c.next(); skip_container(c, '{', '}');
                    } else if (v_ch == '[') {
                         uint32_t tmp = c.next(); skip_container(c, '[', ']');
                    } else if (v_ch == '"') {
                         uint32_t tmp = c.next(); c.next();
                    }
                }
            }
        }

        template<typename T> T get() const {
             if constexpr (std::is_same_v<T, bool>) return type() == Type::True;
             else if constexpr (std::is_same_v<T, int>) return get_int();
             else if constexpr (std::is_same_v<T, double>) return get_double();
             else if constexpr (std::is_same_v<T, std::string_view>) return get_string();
             else if constexpr (std::is_same_v<T, const char*>) return get_string().data();
             else return T{};
        }

        void dump(std::ostream& os) const {
            Type t = type();
            switch (t) {
                case Type::Null: os << "null"; break;
                case Type::True: os << "true"; break;
                case Type::False: os << "false"; break;
                case Type::Number: {
                     double d = get_double();
                     if (d == (int)d) os << (int)d;
                     else os << d;
                     break;
                }
                case Type::String: {
                    os << '"' << get_string() << '"';
                    break;
                }
                case Type::Array: {
                    os << "[";
                    const char* s = ASM::skip_whitespace(parser->base + offset, parser->base + parser->len);
                    uint32_t start = (uint32_t)(s - parser->base) + 1;

                    if (*(ASM::skip_whitespace(parser->base + start, parser->base + parser->len)) == ']') {
                        os << "]"; return;
                    }

                    Cursor c(parser, start);
                    bool first = true;
                    uint32_t el_start = start;

                    while (true) {
                        if (!first) os << ",";
                        first = false;

                        Value(parser, el_start).dump(os);

                        while (true) {
                            uint32_t curr = c.next();
                            if (curr == (uint32_t)-1) break;
                            char ch = parser->base[curr];
                            if (ch == ']') goto end_arr;
                            if (ch == ',') {
                                el_start = curr + 1;
                                break;
                            }
                            if (ch == '{') skip_container(c, '{', '}');
                            else if (ch == '[') skip_container(c, '[', ']');
                            else if (ch == '"') c.next();
                        }
                    }
                    end_arr:
                    os << "]";
                    break;
                }
                case Type::Object: {
                    os << "{";
                    const char* s = ASM::skip_whitespace(parser->base + offset, parser->base + parser->len);
                    uint32_t start = (uint32_t)(s - parser->base) + 1;
                    Cursor c(parser, start);
                    bool first = true;
                    while (true) {
                        uint32_t curr = c.next();
                        if (curr == (uint32_t)-1) break;
                        char ch = parser->base[curr];
                        if (ch == '}') break;
                        if (ch == ',') continue;

                        if (ch == '"') {
                            if (!first) os << ",";
                            first = false;

                            uint32_t end_q = c.next();
                            size_t k_len = end_q - curr - 1;
                            std::string_view k(parser->base + curr + 1, k_len);
                            os << '"' << k << "\":";

                            uint32_t colon = c.next();
                            Value(parser, colon + 1).dump(os);

                            const char* v_s = ASM::skip_whitespace(parser->base + colon + 1, parser->base + parser->len);
                            char v_ch = *v_s;
                            if (v_ch == '{') {
                                uint32_t tmp = c.next(); skip_container(c, '{', '}');
                            } else if (v_ch == '[') {
                                uint32_t tmp = c.next(); skip_container(c, '[', ']');
                            } else if (v_ch == '"') {
                                uint32_t tmp = c.next(); c.next();
                            }
                        }
                    }
                    os << "}";
                    break;
                }
            }
        }
    };

    class Document {
        Parser parser;
        std::string storage;
    public:
        void parse(std::string json) {
            storage = std::move(json);
            parser.parse(storage.data(), storage.size());
        }
        void parse_view(const char* data, size_t len) {
            parser.parse(data, len);
        }
        Value root() const {
            if (parser.len == 0) return Value();
            return Value(&parser, 0);
        }
        void dump(std::ostream& os) const {
            root().dump(os);
        }
    };
}

// -----------------------------------------------------------------------------
// Nlohmann-like API Facade
// -----------------------------------------------------------------------------
namespace Tachyon {
    // Macro to map structs
    #define TACHYON_DEFINE_TYPE_NON_INTRUSIVE(Type, ...) \
        /* Implementation placeholder for struct mapping */
}

#endif // TACHYON_HPP
