#ifndef TACHYON_HPP
#define TACHYON_HPP

// Tachyon v7.2 - Saturating SWAR & Diagnostic Suite
// Architecture: Tape-based, Raw Inline ASM Kernel, AVX2, Saturating SWAR Number Parsing
// Copyright (c) 2024 Tachyon Authors. All Rights Reserved.

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <string_view>
#include <immintrin.h>
#include <iostream>
#include <algorithm>
#include <memory>
#include <cmath>
#include <bit>
#include <stdexcept>

#ifdef _MSC_VER
#include <intrin.h>
#define TACHYON_FORCE_INLINE __forceinline
#define TACHYON_NOINLINE __declspec(noinline)
inline int count_trailing_zeros(uint32_t x) { unsigned long r; _BitScanForward(&r, x); return (int)r; }
inline void* aligned_alloc_impl(size_t size) { return _aligned_malloc(size, 64); }
inline void aligned_free_impl(void* ptr) { _aligned_free(ptr); }
#else
#define TACHYON_FORCE_INLINE __attribute__((always_inline)) inline
#define TACHYON_NOINLINE __attribute__((noinline))
inline int count_trailing_zeros(uint32_t x) { return __builtin_ctz(x); }
inline void* aligned_alloc_impl(size_t size) { void* p; if(posix_memalign(&p, 64, size)!=0) return nullptr; return p; }
inline void aligned_free_impl(void* ptr) { free(ptr); }
#endif

namespace Tachyon {

enum TokenType : uint8_t {
    T_END = 0,
    T_NULL = 'n',
    T_TRUE = 't',
    T_FALSE = 'f',
    T_NUMBER_INT = 'i',
    T_NUMBER_DOUBLE = 'd',
    T_STRING = '"',
    T_OBJ_START = '{',
    T_OBJ_END = '}',
    T_ARR_START = '[',
    T_ARR_END = ']'
};

static constexpr uint64_t make_tape(uint8_t type, uint64_t payload) {
    return (static_cast<uint64_t>(type) << 56) | (payload & 0x00FFFFFFFFFFFFFFULL);
}

static constexpr uint8_t get_type(uint64_t val) { return static_cast<uint8_t>(val >> 56); }
static constexpr uint64_t get_payload(uint64_t val) { return val & 0x00FFFFFFFFFFFFFFULL; }

// SWAR Number Parsing
TACHYON_FORCE_INLINE uint64_t parse_8_digits(uint64_t val) {
    const uint64_t mask = 0x0F0F0F0F0F0F0F0FULL;
    val &= mask;
    val = (val * 10) + (val >> 8);
    val = (((val & 0x00FF00FF00FF00FFULL) * 100) + ((val >> 16) & 0x00FF00FF00FF00FFULL));
    val = (((val & 0x0000FFFF0000FFFFULL) * 10000) + ((val >> 32) & 0x0000FFFF0000FFFFULL));
    return val & 0xFFFFFFFF;
}

TACHYON_FORCE_INLINE const char* parse_double_saturated(const char* p, double& out) {
    bool neg = false;
    if (*p == '-') { neg = true; ++p; }

    uint64_t mantissa = 0;
    int exponent = 0;
    int digits_processed = 0;

    {
        uint64_t chunk; memcpy(&chunk, p, 8);
        uint64_t val = chunk - 0x3030303030303030ULL;
        uint64_t has_non_digit = (val + 0x7676767676767676ULL) | val;
        has_non_digit &= 0x8080808080808080ULL;

        if (has_non_digit == 0) {
            mantissa = parse_8_digits(chunk);
            p += 8;
            digits_processed += 8;

            memcpy(&chunk, p, 8);
            val = chunk - 0x3030303030303030ULL;
            has_non_digit = (val + 0x7676767676767676ULL) | val;
            has_non_digit &= 0x8080808080808080ULL;

            if (has_non_digit == 0) {
                mantissa = mantissa * 100000000ULL + parse_8_digits(chunk);
                p += 8;
                digits_processed += 8;
                while ((unsigned char)(*p - '0') <= 9) {
                    if (digits_processed < 19) {
                        mantissa = mantissa * 10 + (*p - '0');
                        digits_processed++;
                    } else {
                        exponent++;
                    }
                    p++;
                }
            } else {
                while ((unsigned char)(*p - '0') <= 9) {
                    mantissa = mantissa * 10 + (*p - '0');
                    digits_processed++;
                    p++;
                }
            }
        } else {
            while ((unsigned char)(*p - '0') <= 9) {
                mantissa = mantissa * 10 + (*p - '0');
                digits_processed++;
                p++;
            }
        }
    }

    if (*p == '.') {
        ++p;
        while ((unsigned char)(*p - '0') <= 9) {
            if (digits_processed < 19) {
                mantissa = mantissa * 10 + (*p - '0');
                digits_processed++;
                exponent--;
            }
            p++;
        }
    }

    if (*p == 'e' || *p == 'E') {
        ++p;
        bool eneg = (*p == '-');
        if (eneg || *p == '+') ++p;
        int exp_part = 0;
        while ((unsigned char)(*p - '0') <= 9) {
            exp_part = exp_part * 10 + (*p - '0');
            ++p;
        }
        if (eneg) exponent -= exp_part;
        else exponent += exp_part;
    }

    double result = (double)mantissa;
    if (exponent != 0) {
        if (exponent > 0 && exponent <= 22) {
            static const double powers_pos[] = {1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};
            result *= powers_pos[exponent];
        } else if (exponent < 0 && exponent >= -22) {
             static const double powers_neg[] = {1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};
             result /= powers_neg[-exponent];
        } else {
            result *= std::pow(10.0, exponent);
        }
    }

    if (neg) result = -result;
    out = result;
    return p;
}

class Document {
public:
    uint64_t* tape = nullptr;
    size_t tape_len = 0;
    size_t tape_cap = 0;
    char* data = nullptr;
    size_t data_len = 0;
    size_t data_cap = 0;

    Document() {
        tape_cap = 2 * 1024 * 1024;
        tape = (uint64_t*)aligned_alloc_impl(tape_cap * sizeof(uint64_t));
        data_cap = 4 * 1024 * 1024;
        data = (char*)aligned_alloc_impl(data_cap);
    }
    ~Document() {
        if (tape) aligned_free_impl(tape);
        if (data) aligned_free_impl(data);
    }
    void resize_tape(size_t needed) {
        size_t new_cap = tape_cap * 2; if (new_cap < needed) new_cap = needed;
        uint64_t* new_tape = (uint64_t*)aligned_alloc_impl(new_cap * sizeof(uint64_t));
        memcpy(new_tape, tape, tape_len * sizeof(uint64_t));
        aligned_free_impl(tape); tape = new_tape; tape_cap = new_cap;
    }
    void resize_data(size_t needed) {
        size_t new_cap = data_cap * 2; if (new_cap < needed) new_cap = needed;
        char* new_data = (char*)aligned_alloc_impl(new_cap);
        memcpy(new_data, data, data_len);
        aligned_free_impl(data); data = new_data; data_cap = new_cap;
    }
    void parse(std::string_view json);
};

TACHYON_FORCE_INLINE void Document::parse(std::string_view json) {
    if (json.size() + 1024 > data_cap) resize_data(json.size() + 1024);
    memcpy(data, json.data(), json.size());
    memset(data + json.size(), 0x7F, 1024); // Sentinel
    data_len = json.size();
    if (json.size() > tape_cap) resize_tape(json.size());

    uint64_t* t_ptr = tape;
    const char* c_ptr = data;

    // Explicit Stream Buffer
    // Store 8 items then stream. 8 to allow bursts of 2-slot tokens.
    uint64_t t_buf[8];
    int t_buf_idx = 0;

    auto emit = [&](uint64_t val) {
        t_buf[t_buf_idx++] = val;
        if (t_buf_idx >= 4) { // Stream 4 at a time (AVX2 256-bit)
             _mm256_stream_si256((__m256i*)t_ptr, _mm256_loadu_si256((const __m256i*)t_buf));
             t_ptr += 4;
             t_buf_idx -= 4;
             if (t_buf_idx > 0) { // Shift remainder
                 t_buf[0] = t_buf[4]; t_buf[1] = t_buf[5]; t_buf[2] = t_buf[6]; t_buf[3] = t_buf[7];
             }
        }
    };

    // Special emit for Double (2 slots)
    auto emit_double = [&](double d) {
        uint64_t bits; memcpy(&bits, &d, 8);
        t_buf[t_buf_idx++] = make_tape(T_NUMBER_DOUBLE, 0);
        t_buf[t_buf_idx++] = bits;
        if (t_buf_idx >= 4) {
             _mm256_stream_si256((__m256i*)t_ptr, _mm256_loadu_si256((const __m256i*)t_buf));
             t_ptr += 4;
             t_buf_idx -= 4;
             if (t_buf_idx > 0) {
                 for(int k=0; k<t_buf_idx; ++k) t_buf[k] = t_buf[k+4];
             }
        }
    };

    auto flush = [&]() {
        for(int i=0; i<t_buf_idx; ++i) *t_ptr++ = t_buf[i];
        t_buf_idx = 0;
    };

    while (true) {
        const char* next_pos = c_ptr;
        __asm__ volatile (
            "   mov $0x20, %%eax            \n"
            "   vmovd %%eax, %%xmm1         \n"
            "   vpbroadcastb %%xmm1, %%ymm1 \n"
            "1:                             \n"
            "   vmovdqu (%[src]), %%ymm0    \n" "   vpcmpgtb %%ymm1, %%ymm0, %%ymm2 \n" "   vpmovmskb %%ymm2, %%eax     \n" "   test %%eax, %%eax \n" "   jnz 2f \n"
            "   vmovdqu 32(%[src]), %%ymm0    \n" "   vpcmpgtb %%ymm1, %%ymm0, %%ymm2 \n" "   vpmovmskb %%ymm2, %%eax     \n" "   test %%eax, %%eax \n" "   jnz 3f \n"
            "   vmovdqu 64(%[src]), %%ymm0    \n" "   vpcmpgtb %%ymm1, %%ymm0, %%ymm2 \n" "   vpmovmskb %%ymm2, %%eax     \n" "   test %%eax, %%eax \n" "   jnz 4f \n"
            "   vmovdqu 96(%[src]), %%ymm0    \n" "   vpcmpgtb %%ymm1, %%ymm0, %%ymm2 \n" "   vpmovmskb %%ymm2, %%eax     \n" "   test %%eax, %%eax \n" "   jnz 5f \n"
            "   add $128, %[src]             \n"
            "   jmp 1b                      \n"
            "3: add $32, %[src]; jmp 2f     \n"
            "4: add $64, %[src]; jmp 2f     \n"
            "5: add $96, %[src]; jmp 2f     \n"
            "2:                             \n"
            : [src] "+r" (next_pos) : : "rax", "ymm0", "ymm1", "ymm2", "memory"
        );
        c_ptr = next_pos;

        __m256i v = _mm256_loadu_si256((const __m256i*)c_ptr);
        __m256i v_thr = _mm256_set1_epi8(0x20);
        uint32_t mask = _mm256_movemask_epi8(_mm256_cmpgt_epi8(v, v_thr));

        while (mask) {
            int tz = count_trailing_zeros(mask);
            const char* curr = c_ptr + tz;
            char c = *curr;

            if (c == '"') {
                const char* s_start = curr + 1;
                const char* s_end = s_start;
                while (true) {
                    __m256i vs = _mm256_loadu_si256((const __m256i*)s_end);
                    __m256i v_quote = _mm256_set1_epi8('"');
                    __m256i v_bs = _mm256_set1_epi8('\\');
                    uint32_t m_eq = _mm256_movemask_epi8(_mm256_cmpeq_epi8(vs, v_quote));
                    uint32_t m_bs = _mm256_movemask_epi8(_mm256_cmpeq_epi8(vs, v_bs));
                    if ((m_eq | m_bs) != 0) {
                        if (m_bs != 0 && (m_eq == 0 || count_trailing_zeros(m_bs) < count_trailing_zeros(m_eq))) {
                            s_end += count_trailing_zeros(m_bs) + 2; continue;
                        }
                        s_end += count_trailing_zeros(m_eq);
                        break;
                    }
                    s_end += 32;
                }
                emit(make_tape(T_STRING, (uint64_t)(s_start - data)));
                c_ptr = s_end + 1;
                goto next_chunk;
            } else if ((c >= '0' && c <= '9') || c == '-') {
                double d;
                c_ptr = parse_double_saturated(curr, d);
                emit_double(d); // Uses 2 slots
                goto next_chunk;
            }
            else if (c == '{') emit(make_tape(T_OBJ_START, 0));
            else if (c == '}') emit(make_tape(T_OBJ_END, 0));
            else if (c == '[') emit(make_tape(T_ARR_START, 0));
            else if (c == ']') emit(make_tape(T_ARR_END, 0));
            else if (c == 't') { emit(make_tape(T_TRUE, 0)); c_ptr = curr + 4; goto next_chunk; }
            else if (c == 'f') { emit(make_tape(T_FALSE, 0)); c_ptr = curr + 5; goto next_chunk; }
            else if (c == 'n') { emit(make_tape(T_NULL, 0)); c_ptr = curr + 4; goto next_chunk; }
            else if ((unsigned char)c == 0x7F) { goto done; }
            mask &= ~(1U << tz);
        }
        c_ptr += 32;
        next_chunk:;
    }
    done:
    flush();
    tape_len = t_ptr - tape;
}

class json {
    std::shared_ptr<Document> doc;
    size_t idx;

    std::string dump_recursive(size_t& curr) const {
        if (curr >= doc->tape_len) return "";
        uint8_t t = get_type(doc->tape[curr]);
        if (t == T_STRING) {
             uint64_t off = get_payload(doc->tape[curr]);
             const char* s = doc->data + off;
             const char* e = s; while(*e != '"') { if(*e=='\\')e+=2; else e++; }
             return std::string("\"") + std::string(s, e-s) + "\"";
        }
        if (t == T_NUMBER_DOUBLE) {
            double d;
            uint64_t bits = doc->tape[curr+1]; // Next slot
            memcpy(&d, &bits, 8);
            curr++; // Skip extra slot
            return std::to_string(d);
        }
        if (t == T_NUMBER_INT) return std::to_string((int64_t)get_payload(doc->tape[curr]));
        if (t == T_TRUE) return "true";
        if (t == T_FALSE) return "false";
        if (t == T_NULL) return "null";
        if (t == T_OBJ_START) {
            std::string s = "{";
            curr++;
            bool first = true;
            while (curr < doc->tape_len) {
                if (get_type(doc->tape[curr]) == T_OBJ_END) break;
                if (!first) s += ",";
                first = false;
                s += dump_recursive(curr);
                curr++;
                s += ":";
                s += dump_recursive(curr);
                curr++;
            }
            s += "}";
            return s;
        }
        if (t == T_ARR_START) {
            std::string s = "[";
            curr++;
            bool first = true;
            while (curr < doc->tape_len) {
                if (get_type(doc->tape[curr]) == T_ARR_END) break;
                if (!first) s += ",";
                first = false;
                s += dump_recursive(curr);
                curr++;
            }
            s += "]";
            return s;
        }
        return "";
    }

public:
    json() : doc(nullptr), idx(0) {}
    json(std::shared_ptr<Document> d, size_t i) : doc(d), idx(i) {}

    static json parse(std::string_view s) {
        auto d = std::make_shared<Document>();
        d->parse(s);
        return json(d, 0);
    }

    std::string dump() const {
        if (!doc) return "null";
        size_t curr = idx;
        return dump_recursive(curr);
    }

    template<typename T> T get() const {
        if (!doc) return T();
        uint64_t val = doc->tape[idx];
        uint8_t type = get_type(val);
        uint64_t payload = get_payload(val);
        if constexpr (std::is_same_v<T, double>) {
            if (type == T_NUMBER_DOUBLE) {
                double d;
                uint64_t bits = doc->tape[idx+1];
                memcpy(&d, &bits, 8);
                return d;
            }
            if (type == T_NUMBER_INT) return (double)payload;
        } else if constexpr (std::is_same_v<T, std::string>) {
             if (type == T_STRING) {
                 const char* s = doc->data + payload;
                 const char* e = s; while(*e != '"') { if(*e=='\\')e+=2; else e++; }
                 return std::string(s, e-s);
             }
        }
        return T();
    }

    bool is_array() const { return doc && get_type(doc->tape[idx]) == T_ARR_START; }
    bool is_object() const { return doc && get_type(doc->tape[idx]) == T_OBJ_START; }
    bool is_null() const { return !doc || get_type(doc->tape[idx]) == T_NULL; }
    bool is_string() const { return doc && get_type(doc->tape[idx]) == T_STRING; }
    bool is_number() const { return doc && (get_type(doc->tape[idx]) == T_NUMBER_INT || get_type(doc->tape[idx]) == T_NUMBER_DOUBLE); }
    bool is_boolean() const { return doc && (get_type(doc->tape[idx]) == T_TRUE || get_type(doc->tape[idx]) == T_FALSE); }

    json operator[](const std::string& key) const {
        if (!doc || get_type(doc->tape[idx]) != T_OBJ_START) return json();
        size_t curr = idx + 1;
        while (curr < doc->tape_len) {
            uint8_t t = get_type(doc->tape[curr]);
            if (t == T_OBJ_END) return json();
            if (t == T_STRING) {
                uint64_t off = get_payload(doc->tape[curr]);
                const char* k = doc->data + off;
                bool match = true;
                for(size_t i=0; i<key.size(); ++i) if(k[i] != key[i]) { match=false; break; }
                if (match && k[key.size()] == '"') return json(doc, curr + 1);
                curr++; // Skip key
                // Skip value
                int depth = 0;
                do {
                    uint8_t vt = get_type(doc->tape[curr]);
                    if (vt == T_OBJ_START || vt == T_ARR_START) depth++;
                    else if (vt == T_OBJ_END || vt == T_ARR_END) depth--;
                    else if (vt == T_NUMBER_DOUBLE) curr++; // Skip extra slot
                    curr++;
                } while(depth > 0);
            } else curr++;
        }
        return json();
    }

    json operator[](size_t i) const {
        if (!doc || get_type(doc->tape[idx]) != T_ARR_START) return json();
        size_t curr = idx + 1;
        size_t count = 0;
        while (curr < doc->tape_len) {
            if (get_type(doc->tape[curr]) == T_ARR_END) return json();
            if (count == i) return json(doc, curr);
            int depth = 0;
            do {
                uint8_t vt = get_type(doc->tape[curr]);
                if (vt == T_OBJ_START || vt == T_ARR_START) depth++;
                else if (vt == T_OBJ_END || vt == T_ARR_END) depth--;
                else if (vt == T_NUMBER_DOUBLE) curr++; // Skip extra slot
                curr++;
            } while(depth > 0);
            count++;
        }
        return json();
    }
};

} // namespace Tachyon

#endif
