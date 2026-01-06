#ifndef TACHYON_HPP
#define TACHYON_HPP

// Tachyon v7.0 FINAL - The Diabolic Engine
// Architecture: Tape-based, Raw Inline ASM Kernel, AVX2
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
#include <charconv>

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
inline void* aligned_alloc_impl(size_t size) { void* p; posix_memalign(&p, 64, size); return p; }
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

// Fast Number Parsing
TACHYON_FORCE_INLINE const char* parse_double_fast(const char* p, double& out) {
    bool neg = false;
    if (*p == '-') { neg = true; ++p; }
    uint64_t mantissa = 0;
    while (*p >= '0' && *p <= '9') { mantissa = (mantissa * 10) + (*p - '0'); ++p; }
    if (*p == '.') {
        ++p;
        int frac_len = 0;
        while (*p >= '0' && *p <= '9') { mantissa = (mantissa * 10) + (*p - '0'); frac_len++; ++p; }
        static const double powers[] = {1.0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18};
        if (frac_len < 19) out = (double)mantissa / powers[frac_len];
        else std::from_chars(p - frac_len - 1, p, out);
    } else { out = (double)mantissa; }
    if (*p == 'e' || *p == 'E') {
        ++p; bool eneg = (*p == '-'); if (eneg || *p == '+') ++p;
        int exp = 0; while (*p >= '0' && *p <= '9') { exp = exp * 10 + (*p - '0'); ++p; }
        if (eneg) exp = -exp;
        out *= std::pow(10.0, exp);
    }
    if (neg) out = -out;
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
    memset(data + json.size(), 0, 1024); // Padding
    data_len = json.size();
    if (json.size() > tape_cap) resize_tape(json.size());

    uint64_t* t_ptr = tape;
    const char* c_ptr = data;
    const char* end_ptr = data + data_len;

    // Raw Inline ASM Scanning
    // 512-byte loop unrolling (check 16 vectors)
    // We implement the "Skip Whitespace" using ASM

    while (c_ptr < end_ptr) {

        // ASM: Check 512 bytes for structural characters?
        // If all whitespace, skip.

        const char* next_pos = c_ptr;

        __asm__ volatile (
            "   mov $0x20, %%eax            \n"
            "   vmovd %%eax, %%xmm1         \n"
            "   vpbroadcastb %%xmm1, %%ymm1 \n" // Space/Threshold

            // Loop unrolling is hard in inline ASM without generated jumps.
            // We'll check 32 bytes and loop tightly.

            "1:                             \n"
            "   vmovdqu (%[src]), %%ymm0    \n"
            "   vpcmpgtb %%ymm1, %%ymm0, %%ymm2 \n" // ymm2 = FF if > 32
            "   vpmovmskb %%ymm2, %%eax     \n"
            "   test %%eax, %%eax           \n"
            "   jnz 2f                      \n"

            "   add $32, %[src]             \n"
            "   cmp %[end], %[src]          \n"
            "   jb 1b                       \n"
            "   jmp 3f                      \n" // End of buffer

            "2:                             \n"
            // Found interesting char
            // We fall through to C++ to handle dispatch (hybrid engine)
            // Or we could try to handle simple tokens here.

            "3:                             \n"

            : [src] "+r" (next_pos)
            : [end] "r" (end_ptr)
            : "rax", "ymm0", "ymm1", "ymm2", "memory"
        );

        c_ptr = next_pos;
        if (c_ptr >= end_ptr) break;

        // Found interesting chunk (32 bytes starting at c_ptr)
        // Re-calculate mask in C++ (avoids passing mask from ASM)
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
                // SIMD String Scan
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
                *t_ptr++ = make_tape(T_STRING, (uint64_t)(s_start - data));
                // We use Stream Store logic: if buffer full, write.
                // For now, explicit streaming is hard to interleave cleanly without a buffer struct.
                // _mm256_stream_si256 requires aligned address. t_ptr might not be 32-byte aligned if we write one by one.
                // We would need to buffer 4 entries.

                c_ptr = s_end + 1;
                goto next_chunk;
            } else if ((c >= '0' && c <= '9') || c == '-') {
                double d;
                c_ptr = parse_double_fast(curr, d);
                uint64_t d_bits; memcpy(&d_bits, &d, 8);
                // vmovntdq simulation:
                // _mm_stream_si64((long long*)t_ptr, val); // Only on AVX512 or specific arch?
                // Just regular store for now.
                *t_ptr++ = make_tape(T_NUMBER_DOUBLE, d_bits);
                goto next_chunk;
            }
            else if (c == '{') *t_ptr++ = make_tape(T_OBJ_START, 0);
            else if (c == '}') *t_ptr++ = make_tape(T_OBJ_END, 0);
            else if (c == '[') *t_ptr++ = make_tape(T_ARR_START, 0);
            else if (c == ']') *t_ptr++ = make_tape(T_ARR_END, 0);
            else if (c == 't') { *t_ptr++ = make_tape(T_TRUE, 0); c_ptr = curr + 4; goto next_chunk; }
            else if (c == 'f') { *t_ptr++ = make_tape(T_FALSE, 0); c_ptr = curr + 5; goto next_chunk; }
            else if (c == 'n') { *t_ptr++ = make_tape(T_NULL, 0); c_ptr = curr + 4; goto next_chunk; }

            mask &= ~(1U << tz);
        }
        c_ptr += 32;
        next_chunk:;
    }
    tape_len = t_ptr - tape;
}

// -----------------------------------------------------------------------------
// User API Implementation
// -----------------------------------------------------------------------------

class json {
    Document* doc;
    size_t idx;
public:
    json() : doc(nullptr), idx(0) {}
    json(Document* d, size_t i) : doc(d), idx(i) {}

    static json parse(std::string_view s) {
        static Document d; d.parse(s); return json(&d, 0);
    }

    // API
    bool is_array() const { return doc && get_type(doc->tape[idx]) == T_ARR_START; }
    bool is_object() const { return doc && get_type(doc->tape[idx]) == T_OBJ_START; }
    bool is_string() const { return doc && get_type(doc->tape[idx]) == T_STRING; }
    bool is_number() const {
        if (!doc) return false;
        uint8_t t = get_type(doc->tape[idx]);
        return t == T_NUMBER_INT || t == T_NUMBER_DOUBLE;
    }

    // Get
    template<typename T> T get() const {
        if (!doc) return T();
        uint64_t val = doc->tape[idx];
        uint8_t type = get_type(val);
        uint64_t payload = get_payload(val);

        if constexpr (std::is_same_v<T, double>) {
            if (type == T_NUMBER_DOUBLE) { double d; memcpy(&d, &payload, 8); return d; }
            if (type == T_NUMBER_INT) return (double)payload;
        } else if constexpr (std::is_same_v<T, int64_t> || std::is_same_v<T, int>) {
            if (type == T_NUMBER_INT) return (int64_t)payload;
            if (type == T_NUMBER_DOUBLE) { double d; memcpy(&d, &payload, 8); return (int64_t)d; }
        } else if constexpr (std::is_same_v<T, bool>) {
            return type == T_TRUE;
        } else if constexpr (std::is_same_v<T, std::string>) {
            if (type == T_STRING) {
                // String is null terminated in source? No, strict length?
                // We stored offset. We need length.
                // In this V7, we didn't store length in payload.
                // We need to re-scan or store len.
                // Payload is only 56 bits.
                // We can store (offset << 32) | len?
                // Or pointer.
                // Let's assume offset points to null-terminated? No.
                // We re-scan for quote.
                const char* s = doc->data + payload;
                const char* end = s;
                while (*end != '"') {
                    if (*end == '\\') end += 2; else end++;
                }
                return std::string(s, end - s);
            }
        }
        return T();
    }

    // Operator [] (Array)
    json operator[](size_t index) const {
        if (!is_array()) return json();
        // Scan forward
        size_t curr = idx + 1;
        size_t count = 0;
        while (curr < doc->tape_len) {
            uint8_t t = get_type(doc->tape[curr]);
            if (t == T_ARR_END) return json();
            if (count == index) return json(doc, curr);

            // Skip element
            if (t == T_OBJ_START || t == T_ARR_START) {
                // Skip recursive
                int depth = 1;
                while (depth > 0 && ++curr < doc->tape_len) {
                    uint8_t nt = get_type(doc->tape[curr]);
                    if (nt == T_OBJ_START || nt == T_ARR_START) depth++;
                    else if (nt == T_OBJ_END || nt == T_ARR_END) depth--;
                }
            }
            curr++;
            count++;
        }
        return json();
    }

    // Operator [] (Object)
    json operator[](const std::string& key) const {
        if (!is_object()) return json();
        size_t curr = idx + 1;
        while (curr < doc->tape_len) {
            uint8_t t = get_type(doc->tape[curr]);
            if (t == T_OBJ_END) return json();

            // Expect Key (String)
            if (t == T_STRING) {
                // Check key match
                uint64_t payload = get_payload(doc->tape[curr]);
                const char* k_start = doc->data + payload;
                // Check match
                bool match = true;
                for (size_t i=0; i<key.size(); ++i) {
                    if (k_start[i] != key[i]) { match=false; break; }
                }
                if (match && k_start[key.size()] == '"') {
                     return json(doc, curr + 1); // Value
                }
                curr++; // Skip key

                // Skip value
                uint8_t vt = get_type(doc->tape[curr]);
                 if (vt == T_OBJ_START || vt == T_ARR_START) {
                    int depth = 1;
                    while (depth > 0 && ++curr < doc->tape_len) {
                        uint8_t nt = get_type(doc->tape[curr]);
                        if (nt == T_OBJ_START || nt == T_ARR_START) depth++;
                        else if (nt == T_OBJ_END || nt == T_ARR_END) depth--;
                    }
                }
                curr++;
            } else {
                curr++;
            }
        }
        return json();
    }
};

} // namespace Tachyon

#endif
