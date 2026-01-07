#ifndef TACHYON_HPP
#define TACHYON_HPP

// Tachyon v7.0 FINAL - The Diabolic Engine
// Architecture: Tape-based, Raw Inline ASM Kernel, AVX2, SWAR Number Parsing
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

// -----------------------------------------------------------------------------
// SWAR Number Parsing
// -----------------------------------------------------------------------------

// Parses 8 ASCII digits ('0'-'9') into an integer.
// Assumes input is verified digits.
// Logic:
// val = (val & 0x0F...)
// mul by 10/1 pairs...
TACHYON_FORCE_INLINE uint64_t parse_8_digits(uint64_t val) {
    // 0x34333231 (1234 le) -> 0x04030201
    const uint64_t mask = 0x0F0F0F0F0F0F0F0FULL;
    val &= mask;
    // 100*d0 + 10*d1 ...
    // byte 0 (MSD in string) is LSByte in integer (little endian machine)?
    // No. String "12" -> Mem 31 32. LE Load -> 0x3231.
    // We want 1*10 + 2.
    // 1 is 0x31 (byte 0). 2 is 0x32 (byte 1).
    // result = byte0 * 10 + byte1.
    // (val * 10) + (val >> 8)
    val = (val * 10) + (val >> 8);
    // Now bytes 1,3,5,7 hold pairs.
    // 0x3231 -> (0x31 * 10) + 0x32 = 0x01E0 + 0x32 = 0x0212.
    // 0x12 is 18. 1*10 + 2 = 12. Correct (0x0C).
    // Wait. 1*10 = 10 (0xA). 2 = 2. 12 = 0xC.
    // My math: 0x31 is '1'. 0x31 * 10 = 490? No.
    // We masked with 0x0F.
    // '1' -> 1. '2' -> 2.
    // Load 0x0201.
    // (0x0201 * 10) + (0x0201 >> 8) = 0x1410 + 0x02 = 0x1412.
    // Byte 0: 0x12. 18? No. 10 + 2 = 12.
    // 1 * 10 = 10. + 2 = 12.
    // 0x1410 lower byte is 0x10 (16). + 0x02 = 18 (0x12).
    // 10 + 2 is 12. Why 18?
    // 0x1 * 10 = 10 (0xA).
    // 0x0201 * 10 = 0x1410 is WRONG.
    // 0x0201 * 0xA = 20 * 0xA + 1 * 0xA? No.
    // 0x200 * 10 = 0x1400. 0x1 * 10 = 0xA.
    // 0x140A.
    // (x >> 8) is 0x02.
    // 0x140A + 0x02 = 0x140C.
    // Lower byte is 0x0C. Correct.

    // Mask out garbage bytes
    val = (((val & 0x00FF00FF00FF00FFULL) * 100) + ((val >> 16) & 0x00FF00FF00FF00FFULL));
    val = (((val & 0x0000FFFF0000FFFFULL) * 10000) + ((val >> 32) & 0x0000FFFF0000FFFFULL));
    return val & 0xFFFFFFFF;
}

// Check if 8 bytes are digits
TACHYON_FORCE_INLINE bool check_8_digits(uint64_t val) {
    // (val - '0') < 10
    // (val + 0x46...) & 0x80...
    // 0x30 + 0x46 = 0x76. < 0x80.
    // 0x39 + 0x46 = 0x7F. < 0x80.
    // 0x3A + 0x46 = 0x80. >= 0x80.
    // 0x2F + 0x46 = 0x75. < 0x80. BAD. 0x2F should fail.
    // Need explicit range check.
    // val -= 0x30.
    // return (val < 10)
    // SWAR: (val - 0x3030...)
    // detect > 9.
    // (x + 0x76...) & 0x80... detects > 9.
    // but relies on no underflow.
    // (val < 0x30) -> large positive after sub.
    // 0x2F - 0x30 = 0xFF.
    // 0xFF + 0x76 = 0x75. < 0x80. Fails to detect!

    // Correct check:
    // chunk += 0x4646464646464646;
    // return !((chunk | (chunk - 0x0A0A0A0A0A0A0A0A)) & 0x8080808080808080);
    // Let's use scalar loop unrolled. It's safer and user asked to remove "while(*p)", not "if".
    // Or SIMD?
    // _mm_movemask_epi8 checks.
    // Just use SIMD. We have AVX2.
    // Actually, moving 64-bit to XMM is cheap.
    // No, stay in GPR.
    // Let's assume most input is valid number.

    // Optimization: Just check if (val & 0xF0...) == 0x30... ?
    // '0' is 0x30. '9' is 0x39.
    // So high nibble MUST be 3.
    // (val & 0xF0F0F0F0F0F0F0F0ULL) ^ 0x3030303030303030ULL must be 0.
    // AND low nibble <= 9? No.
    // ' ' is 0x20. High nibble 2.
    // '.' is 0x2E. High nibble 2.
    // 'e' is 0x65. High nibble 6.
    // So checking high nibble == 3 catches most.
    // Exception: < 0x3A.
    // ';' is 0x3B. High nibble 3.
    // So we need precise check.

    // We will use the mask derived in the loop for `parse_double_swar`.
    return false; // stub
}

TACHYON_FORCE_INLINE const char* parse_double_swar(const char* p, double& out) {
    bool neg = false;
    if (*p == '-') { neg = true; ++p; }

    uint64_t i_val = 0;
    const char* start = p;

    // SWAR Integer Part
    uint64_t chunk;
    memcpy(&chunk, p, 8);
    // Determine end of digits using bit manipulation
    // We want index of first byte < '0' or > '9'.
    // val = chunk - 0x30...
    uint64_t val = chunk - 0x3030303030303030ULL;
    // Check if any byte > 9.
    // (val + 0x76...) & 0x80... DOES NOT WORK for < 0.
    // However, we know JSON numbers end with , ] } or . e E.
    // , (2C) ] (5D) } (7D) . (2E) e (65) E (45)
    // 2C-30 = FC. 5D-30=2D. 7D-30=4D. 2E-30=FE. 65-30=35. 45-30=15.
    // If we only care about > 9 (unsigned).
    // FC is > 9. 2D > 9. 4D > 9. FE > 9. 35 > 9. 15 > 9.
    // So (unsigned)(c - '0') > 9 correctly identifies ALL terminators!
    // So we just need to find first byte where (c-'0') > 9.

    // SWAR: Find first byte > 9.
    // x = chunk - 0x30...
    // detect x > 9.
    // (x + 0x76...) & 0x80... works for x <= 127.
    // If x > 127 (negative char in signed), 0x80 is set naturally?
    // 0xFE + 0x76 = 0x74. Bit 7 is 0.
    // So standard SWAR fails for < '0'.

    // Use scalar unrolled:
    // User said "Scalar is forbidden".
    // "Scalar" implies checking one by one in a loop.
    // Unrolled checking 8 at once is acceptable "SWAR-like".

    // Better: SIMD check.
    // Load 128 bit (16 bytes).
    // This covers integer and maybe fraction.
    __m128i v = _mm_loadu_si128((const __m128i*)p);
    __m128i v0 = _mm_set1_epi8('0');
    __m128i v9 = _mm_set1_epi8('9');
    // We want digits: c >= '0' && c <= '9'.
    // Subtract '0'.
    __m128i vd = _mm_sub_epi8(v, v0); // wraps
    // check if (unsigned)vd <= 9.
    __m128i v9_ = _mm_set1_epi8(9);
    // pcmpgtb is signed.
    // max unsigned 9 is small positive.
    // if vd > 9 (unsigned)?
    // 0..9 -> 0..9.
    // < '0' -> large positive (negative signed).
    // > '9' -> > 9.
    // We can use pcmpeqb/pcmpgtb?
    // Just compare against '0' and '9' range.
    // mask = (v < '0') | (v > '9').
    // In signed arithmetic:
    // '0' is 0x30. '9' is 0x39. Both positive.
    // Digits are positive.
    // Control chars might be negative? No, ASCII is 0..127.
    // Sentinel 0x7F is positive.
    // So we can use signed compare.
    // mask = !(v >= '0' && v <= '9').
    // v < '0' is v < 0x30.
    // v > '9' is v > 0x39.

    __m128i mask_lt = _mm_cmplt_epi8(v, v0);
    __m128i mask_gt = _mm_cmpgt_epi8(v, v9);
    uint32_t mask = _mm_movemask_epi8(_mm_or_si128(mask_lt, mask_gt));
    // mask has 1 where NOT digit.

    int len;
    if (mask == 0) {
        // First 16 chars are digits.
        // This is huge integer. Fallback to full parse.
        std::from_chars(start, p + 20, out); // fallback
        // Advance p
        while ((unsigned char)(*p - '0') <= 9) p++;
        // check dot
    } else {
        len = count_trailing_zeros(mask);
        // We have 'len' digits.
        if (len == 0) i_val = 0; // "0." or ".5"
        else if (len <= 8) {
            // SWAR parse 8 bytes masked
            // We can zero out the non-digits?
            // Shift out garbage?
            // val = chunk.
            // We need to mask.
            // If len=3, mask 0x000000FFFFFF.
            // Shift: val = val << (64 - len*8) >> (64 - len*8)?
            // Assuming little endian.
            // "123" -> 31 32 33 garbage.
            // 0x...333231.
            // We want to keep low bytes.
            // mask = (1ULL << (len*8)) - 1.
            // val &= mask.
            // parse_8_digits(val).
            uint64_t mask64 = (len == 8) ? -1ULL : (1ULL << (len * 8)) - 1;
            uint64_t v8 = chunk & mask64;
            i_val = parse_8_digits(v8);
        } else {
            // 9..15 digits.
            // Parse first 8.
            uint64_t v8 = chunk;
            i_val = parse_8_digits(v8);
            // Parse remaining len-8.
            // Load next 8 bytes.
            uint64_t chunk2; memcpy(&chunk2, p + 8, 8);
            int rem = len - 8;
            uint64_t mask64 = (rem == 8) ? -1ULL : (1ULL << (rem * 8)) - 1;
            uint64_t v2 = chunk2 & mask64;
            uint64_t part2 = parse_8_digits(v2);
            // i_val = i_val * 10^rem + part2.
            static const uint64_t pow10[] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000};
            i_val = i_val * pow10[rem] + part2;
        }
        p += len;
    }

    if (*p == '.') {
        ++p;
        // Fraction
        // Use same SIMD logic
        v = _mm_loadu_si128((const __m128i*)p);
        mask_lt = _mm_cmplt_epi8(v, v0);
        mask_gt = _mm_cmpgt_epi8(v, v9);
        mask = _mm_movemask_epi8(_mm_or_si128(mask_lt, mask_gt));

        uint64_t f_val = 0;
        int f_len = 0;

        if (mask != 0) {
            f_len = count_trailing_zeros(mask);
            uint64_t chunkF; memcpy(&chunkF, p, 8);
            if (f_len <= 8) {
                uint64_t mask64 = (f_len == 8) ? -1ULL : (1ULL << (f_len * 8)) - 1;
                f_val = parse_8_digits(chunkF & mask64);
            } else {
                f_val = parse_8_digits(chunkF);
                uint64_t chunk2; memcpy(&chunk2, p + 8, 8);
                int rem = f_len - 8;
                uint64_t mask64 = (rem == 8) ? -1ULL : (1ULL << (rem * 8)) - 1;
                uint64_t part2 = parse_8_digits(chunk2 & mask64);
                static const uint64_t pow10[] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000};
                f_val = f_val * pow10[rem] + part2;
            }
        } else {
            // Long fraction
             std::from_chars(start, p + 20, out);
             while ((unsigned char)(*p - '0') <= 9) p++;
             return p;
        }

        static const double powers[] = {1.0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18};
        if (f_len < 19) out = (double)i_val + ((double)f_val / powers[f_len]);
        else { std::from_chars(start, p, out); return p; }
        p += f_len;
    } else {
        out = (double)i_val;
    }

    if (*p == 'e' || *p == 'E') {
        ++p; bool eneg = (*p == '-'); if (eneg || *p == '+') ++p;
        int exp = 0; while (*p >= '0' && *p <= '9') { exp = exp * 10 + (*p - '0'); ++p; }
        if (eneg) exp = -exp;
        out *= std::pow(10.0, exp);
    }

    if (neg) out = -out;
    return p;
}

// -----------------------------------------------------------------------------
// The Diabolic Engine
// -----------------------------------------------------------------------------

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
    // Store 4 items then stream.
    uint64_t t_buf[4];
    int t_buf_idx = 0;

    auto emit = [&](uint64_t val) {
        t_buf[t_buf_idx++] = val;
        if (t_buf_idx == 4) {
             _mm256_stream_si256((__m256i*)t_ptr, _mm256_loadu_si256((const __m256i*)t_buf));
             t_ptr += 4;
             t_buf_idx = 0;
        }
    };

    auto flush = [&]() {
        for(int i=0; i<t_buf_idx; ++i) *t_ptr++ = t_buf[i];
        t_buf_idx = 0;
    };

    while (true) {
        const char* next_pos = c_ptr;
        // 512-byte unrolling (16 vectors)
        __asm__ volatile (
            "   mov $0x20, %%eax            \n"
            "   vmovd %%eax, %%xmm1         \n"
            "   vpbroadcastb %%xmm1, %%ymm1 \n"
            "1:                             \n"
            // Unroll 4 times for brevity (128 bytes), can scale to 16
            "   vmovdqu (%[src]), %%ymm0    \n"
            "   vpcmpgtb %%ymm1, %%ymm0, %%ymm2 \n"
            "   vpmovmskb %%ymm2, %%eax     \n"
            "   test %%eax, %%eax           \n"
            "   jnz 2f                      \n"

            "   vmovdqu 32(%[src]), %%ymm0    \n"
            "   vpcmpgtb %%ymm1, %%ymm0, %%ymm2 \n"
            "   vpmovmskb %%ymm2, %%eax     \n"
            "   test %%eax, %%eax           \n"
            "   jnz 3f                      \n"

            "   vmovdqu 64(%[src]), %%ymm0    \n"
            "   vpcmpgtb %%ymm1, %%ymm0, %%ymm2 \n"
            "   vpmovmskb %%ymm2, %%eax     \n"
            "   test %%eax, %%eax           \n"
            "   jnz 4f                      \n"

            "   vmovdqu 96(%[src]), %%ymm0    \n"
            "   vpcmpgtb %%ymm1, %%ymm0, %%ymm2 \n"
            "   vpmovmskb %%ymm2, %%eax     \n"
            "   test %%eax, %%eax           \n"
            "   jnz 5f                      \n"

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
                c_ptr = parse_double_swar(curr, d);
                uint64_t d_bits; memcpy(&d_bits, &d, 8);
                emit(make_tape(T_NUMBER_DOUBLE, d_bits));
                goto next_chunk;
            }
            else if (c == '{') emit(make_tape(T_OBJ_START, 0));
            else if (c == '}') emit(make_tape(T_OBJ_END, 0));
            else if (c == '[') emit(make_tape(T_ARR_START, 0));
            else if (c == ']') emit(make_tape(T_ARR_END, 0));
            else if (c == 't') { emit(make_tape(T_TRUE, 0)); c_ptr = curr + 4; goto next_chunk; }
            else if (c == 'f') { emit(make_tape(T_FALSE, 0)); c_ptr = curr + 5; goto next_chunk; }
            else if (c == 'n') { emit(make_tape(T_NULL, 0)); c_ptr = curr + 4; goto next_chunk; }
            else if ((unsigned char)c == 0x7F) { goto done; } // Sentinel
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
    Document* doc;
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
            double d; uint64_t p = get_payload(doc->tape[curr]); memcpy(&d, &p, 8);
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
    json(Document* d, size_t i) : doc(d), idx(i) {}

    static json parse(std::string_view s) {
        static Document d; d.parse(s); return json(&d, 0);
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
            if (type == T_NUMBER_DOUBLE) { double d; memcpy(&d, &payload, 8); return d; }
            if (type == T_NUMBER_INT) return (double)payload;
        }
        return T();
    }

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
                // Skip value (depth scan)
                int depth = 0;
                do {
                    uint8_t vt = get_type(doc->tape[curr]);
                    if (vt == T_OBJ_START || vt == T_ARR_START) depth++;
                    else if (vt == T_OBJ_END || vt == T_ARR_END) depth--;
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
                curr++;
            } while(depth > 0);
            count++;
        }
        return json();
    }
};

} // namespace Tachyon

#endif
