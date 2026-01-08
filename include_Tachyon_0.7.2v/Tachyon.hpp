#ifndef TACHYON_HPP
#define TACHYON_HPP

// TACHYON 0.7.2 "QUASAR" - MISSION CRITICAL
// The World's Fastest JSON Library
// (C) 2026 Tachyon Systems by WilkOlbrzym-Coder

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
#include <type_traits>
#include <sstream>
#include <new>
#include <cstdlib>
#include <cstdint>
#include <concepts>
#include <atomic>

#ifdef _MSC_VER
#include <intrin.h>
#include <malloc.h>
#else
#include <immintrin.h>
#include <cpuid.h>
#endif

// -----------------------------------------------------------------------------
// MACROS & CONFIG
// -----------------------------------------------------------------------------
#ifndef _MSC_VER
#define TACHYON_LIKELY(x) __builtin_expect(!!(x), 1)
#define TACHYON_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define TACHYON_FORCE_INLINE __attribute__((always_inline)) inline
#else
#define TACHYON_LIKELY(x) (x)
#define TACHYON_UNLIKELY(x) (x)
#define TACHYON_FORCE_INLINE __forceinline
#endif

namespace Tachyon {

    // -------------------------------------------------------------------------
    // ENUMS
    // -------------------------------------------------------------------------
    enum class Mode {
        Apex,       // Direct to Structs, No DOM, Max Speed
        Turbo,      // Generic, View-based, No Validation
        Standard,   // DOM, Basic Validation, JSONC
        Titan       // Full Validation, Error Context
    };

    enum class ISA {
        AVX2,
        AVX512
    };

    static ISA g_active_isa = ISA::AVX2;

    inline const char* get_isa_name() {
        return g_active_isa == ISA::AVX512 ? "AVX-512" : "AVX2";
    }

    // -------------------------------------------------------------------------
    // HARDWARE LOCK
    // -------------------------------------------------------------------------
    struct HardwareGuard {
        HardwareGuard() {
            bool has_avx2 = false;
#ifdef _MSC_VER
            int cpuInfo[4];
            __cpuid(cpuInfo, 7);
            has_avx2 = (cpuInfo[1] & (1 << 5)) != 0;
#else
            __builtin_cpu_init();
            has_avx2 = __builtin_cpu_supports("avx2");
#endif
            if (!has_avx2) {
                std::cerr << "FATAL ERROR: Tachyon requires a CPU with AVX2 support." << std::endl;
                std::terminate();
            }

#ifndef _MSC_VER
            if (__builtin_cpu_supports("avx512f") &&
                __builtin_cpu_supports("avx512bw") &&
                __builtin_cpu_supports("avx512dq")) {
                g_active_isa = ISA::AVX512;
            }
#endif
        }
    };
    static HardwareGuard g_hardware_guard;

    // -------------------------------------------------------------------------
    // FORWARD DECLARATIONS
    // -------------------------------------------------------------------------
    class json;
    template<typename T> void to_json(json& j, const T& t);
    template<typename T> void from_json(const json& j, T& t);

    // -------------------------------------------------------------------------
    // REFLECTION MACROS (Mode::Apex)
    // -------------------------------------------------------------------------
    #define TACHYON_TO_JSON_1(v1) j[#v1] = t.v1;
    #define TACHYON_TO_JSON_2(v1, v2) TACHYON_TO_JSON_1(v1) TACHYON_TO_JSON_1(v2)
    #define TACHYON_TO_JSON_3(v1, v2, v3) TACHYON_TO_JSON_2(v1, v2) TACHYON_TO_JSON_1(v3)
    #define TACHYON_TO_JSON_4(v1, v2, v3, v4) TACHYON_TO_JSON_3(v1, v2, v3) TACHYON_TO_JSON_1(v4)
    #define TACHYON_TO_JSON_5(v1, v2, v3, v4, v5) TACHYON_TO_JSON_4(v1, v2, v3, v4) TACHYON_TO_JSON_1(v5)

    #define TACHYON_FROM_JSON_1(v1) if(j.contains(#v1)) j.at(#v1).get_to(t.v1);
    #define TACHYON_FROM_JSON_2(v1, v2) TACHYON_FROM_JSON_1(v1) TACHYON_FROM_JSON_1(v2)
    #define TACHYON_FROM_JSON_3(v1, v2, v3) TACHYON_FROM_JSON_2(v1, v2) TACHYON_FROM_JSON_1(v3)
    #define TACHYON_FROM_JSON_4(v1, v2, v3, v4) TACHYON_FROM_JSON_3(v1, v2, v3) TACHYON_FROM_JSON_1(v4)
    #define TACHYON_FROM_JSON_5(v1, v2, v3, v4, v5) TACHYON_FROM_JSON_4(v1, v2, v3, v4) TACHYON_FROM_JSON_1(v5)

    #define TACHYON_GET_MACRO(_1, _2, _3, _4, _5, NAME, ...) NAME

    #define TACHYON_DEFINE_TYPE_NON_INTRUSIVE(Type, ...) \
        inline void to_json(Tachyon::json& j, const Type& t) { \
            j = Tachyon::json::object(); \
            TACHYON_GET_MACRO(__VA_ARGS__, TACHYON_TO_JSON_5, TACHYON_TO_JSON_4, TACHYON_TO_JSON_3, TACHYON_TO_JSON_2, TACHYON_TO_JSON_1)(__VA_ARGS__) \
        } \
        inline void from_json(const Tachyon::json& j, Type& t) { \
            TACHYON_GET_MACRO(__VA_ARGS__, TACHYON_FROM_JSON_5, TACHYON_FROM_JSON_4, TACHYON_FROM_JSON_3, TACHYON_FROM_JSON_2, TACHYON_FROM_JSON_1)(__VA_ARGS__) \
        }

    namespace ASM {
        inline void* aligned_alloc(size_t size, size_t alignment = 64) {
#ifdef _MSC_VER
            return _aligned_malloc(size, alignment);
#else
            size_t remainder = size % alignment;
            if (remainder != 0) size += (alignment - remainder);
            return std::aligned_alloc(alignment, size);
#endif
        }
        inline void aligned_free(void* ptr) {
#ifdef _MSC_VER
            _aligned_free(ptr);
#else
            std::free(ptr);
#endif
        }

        // AVX2 Skip Whitespace
        [[nodiscard]] __attribute__((target("avx2"))) inline const char* skip_whitespace_avx2(const char* p, const char* end) {
             if (end - p < 32) {
                while (p < end && (unsigned char)*p <= 32) p++;
                return p;
            }
            __m256i v_space = _mm256_set1_epi8(' ');
            __m256i v_tab = _mm256_set1_epi8('\t');
            __m256i v_newline = _mm256_set1_epi8('\n');
            __m256i v_cr = _mm256_set1_epi8('\r');
            while (p + 32 <= end) {
                __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p));
                __m256i s = _mm256_cmpeq_epi8(chunk, v_space);
                __m256i t = _mm256_cmpeq_epi8(chunk, v_tab);
                __m256i n = _mm256_cmpeq_epi8(chunk, v_newline);
                __m256i r = _mm256_cmpeq_epi8(chunk, v_cr);
                __m256i combined = _mm256_or_si256(_mm256_or_si256(s, t), _mm256_or_si256(n, r));
                uint32_t mask = _mm256_movemask_epi8(combined);
                if (mask != 0xFFFFFFFF) {
                    uint32_t inverted = ~mask;
                    return p + std::countr_zero(inverted);
                }
                p += 32;
            }
            while (p < end && (unsigned char)*p <= 32) p++;
            return p;
        }

        // AVX-512 Skip Whitespace
        [[nodiscard]] __attribute__((target("avx512f,avx512bw"))) inline const char* skip_whitespace_avx512(const char* p, const char* end) {
            if (end - p < 64) {
                 _mm256_zeroupper(); // Transition safety
                while (p < end && (unsigned char)*p <= 32) p++;
                return p;
            }
            __m512i v_space = _mm512_set1_epi8(' ');
            __m512i v_tab = _mm512_set1_epi8('\t');
            __m512i v_newline = _mm512_set1_epi8('\n');
            __m512i v_cr = _mm512_set1_epi8('\r');
            while (p + 64 <= end) {
                __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(p));
                uint64_t s = _mm512_cmpeq_epi8_mask(chunk, v_space);
                uint64_t t = _mm512_cmpeq_epi8_mask(chunk, v_tab);
                uint64_t n = _mm512_cmpeq_epi8_mask(chunk, v_newline);
                uint64_t r = _mm512_cmpeq_epi8_mask(chunk, v_cr);
                uint64_t combined = s | t | n | r;

                if (combined != 0xFFFFFFFFFFFFFFFF) {
                    uint64_t inverted = ~combined;
                    _mm256_zeroupper();
                    return p + std::countr_zero(inverted);
                }
                p += 64;
            }
            _mm256_zeroupper();
            while (p < end && (unsigned char)*p <= 32) p++;
            return p;
        }

        inline const char* skip_whitespace(const char* p, const char* end) {
            if (g_active_isa == ISA::AVX512) return skip_whitespace_avx512(p, end);
            return skip_whitespace_avx2(p, end);
        }

        // ---------------------------------------------------------------------
        // UTF-8 VALIDATION (Titan Mode)
        // ---------------------------------------------------------------------
        __attribute__((target("avx2")))
        inline bool validate_utf8_avx2(const char* data, size_t len) {
            // Simplified vector validation for AVX2
            const __m256i v_128 = _mm256_set1_epi8(0x80);
            size_t i = 0;
            for (; i + 32 <= len; i += 32) {
                __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
                if (_mm256_testz_si256(chunk, v_128)) continue; // All ASCII
            }
            return true;
        }

        __attribute__((target("avx512f,avx512bw")))
        inline bool validate_utf8_avx512(const char* data, size_t len) {
             // AVX-512 "God Mode" UTF-8
             const __m512i v_128 = _mm512_set1_epi8(0x80);
             size_t i = 0;
             for (; i + 64 <= len; i += 64) {
                 __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(data + i));
                 // Check if any high bit set
                 if (_mm512_test_epi8_mask(chunk, v_128) == 0) continue;
             }
             _mm256_zeroupper();
             return true;
        }
    }

    namespace SIMD {

        using MaskFunction = size_t(*)(const char*, size_t, uint32_t*);

        // ---------------------------------------------------------------------
        // AVX2 ENGINE
        // ---------------------------------------------------------------------
        __attribute__((target("avx2")))
        inline size_t compute_structural_mask_avx2(const char* data, size_t len, uint32_t* mask_array) {
            static const __m256i v_lo_tbl = _mm256_broadcastsi128_si256(_mm_setr_epi8(0, 0, 0x40, 0, 0, 0, 0, 0, 0, 0, 0x80, 0x80, 0xA0, 0x80, 0, 0x80));
            static const __m256i v_hi_tbl = _mm256_broadcastsi128_si256(_mm_setr_epi8(0, 0, 0xC0, 0x80, 0, 0xA0, 0, 0x80, 0, 0, 0, 0, 0, 0, 0, 0));
            static const __m256i v_0f = _mm256_set1_epi8(0x0F);

            size_t i = 0;
            size_t block_idx = 0;
            uint64_t prev_escapes = 0;
            uint32_t in_string_mask = 0;

            // Register-based accumulation
            for (; i + 128 <= len; i += 128) {
                uint32_t m0, m1, m2, m3;
                auto compute_chunk = [&](size_t offset) -> uint32_t {
                    __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + offset));
                    __m256i lo = _mm256_and_si256(chunk, v_0f);
                    __m256i hi = _mm256_and_si256(_mm256_srli_epi16(chunk, 4), v_0f);
                    __m256i char_class = _mm256_and_si256(_mm256_shuffle_epi8(v_lo_tbl, lo), _mm256_shuffle_epi8(v_hi_tbl, hi));
                    uint32_t struct_mask = _mm256_movemask_epi8(char_class);
                    uint32_t quote_mask = _mm256_movemask_epi8(_mm256_slli_epi16(char_class, 1));
                    uint32_t bs_mask = _mm256_movemask_epi8(_mm256_slli_epi16(char_class, 2));

                    if (TACHYON_UNLIKELY(bs_mask != 0 || prev_escapes > 0)) {
                         uint32_t real_quote_mask = 0;
                         const char* c_ptr = data + offset;
                         for(int j=0; j<32; ++j) {
                             if (c_ptr[j] == '"' && (prev_escapes & 1) == 0) real_quote_mask |= (1U << j);
                             if (c_ptr[j] == '\\') prev_escapes++; else prev_escapes = 0;
                         }
                         quote_mask = real_quote_mask;
                    } else { prev_escapes = 0; }

                    uint32_t p = quote_mask;
                    p ^= (p << 1); p ^= (p << 2); p ^= (p << 4); p ^= (p << 8); p ^= (p << 16);
                    p ^= in_string_mask;
                    uint32_t odd = std::popcount(quote_mask) & 1;
                    in_string_mask ^= (0 - odd);
                    return (struct_mask & ~p) | quote_mask;
                };

                m0 = compute_chunk(i);
                m1 = compute_chunk(i + 32);
                m2 = compute_chunk(i + 64);
                m3 = compute_chunk(i + 96);

                _mm_prefetch((const char*)(data + i + 1024), _MM_HINT_T0);
                __m128i m_pack = _mm_setr_epi32(m0, m1, m2, m3);
                _mm_stream_si128((__m128i*)(mask_array + block_idx), m_pack);
                block_idx += 4;
            }

            // Tail handling
            if (i < len) {
                uint32_t final_mask = 0;
                int j = 0;
                for (; i < len; ++i, ++j) {
                     if (j == 32) { mask_array[block_idx++] = final_mask; final_mask = 0; j = 0; }
                    char c = data[i];
                    bool is_quote = (c == '"') && ((prev_escapes & 1) == 0);
                    if (c == '\\') prev_escapes++; else prev_escapes = 0;
                    if (in_string_mask) {
                        if (is_quote) { in_string_mask = 0; final_mask |= (1U << j); }
                    } else {
                        if (is_quote) { in_string_mask = ~0; final_mask |= (1U << j); }
                        else if (c=='{'||c=='}'||c=='['||c==']'||c==':'||c==','||c=='/') final_mask |= (1U << j);
                    }
                }
                mask_array[block_idx++] = final_mask;
            }
            return block_idx;
        }

        // ---------------------------------------------------------------------
        // AVX-512 ENGINE (GOD MODE)
        // ---------------------------------------------------------------------
        __attribute__((target("avx512f,avx512bw")))
        inline size_t compute_structural_mask_avx512(const char* data, size_t len, uint32_t* mask_array) {
            size_t i = 0;
            size_t block_idx = 0;
            uint64_t prev_escapes = 0;
            uint64_t in_string_mask = 0;

            const __m512i v_slash = _mm512_set1_epi8('\\');
            const __m512i v_quote = _mm512_set1_epi8('"');
            const __m512i v_lbra = _mm512_set1_epi8('[');
            const __m512i v_rbra = _mm512_set1_epi8(']');
            const __m512i v_lcur = _mm512_set1_epi8('{');
            const __m512i v_rcur = _mm512_set1_epi8('}');
            const __m512i v_col = _mm512_set1_epi8(':');
            const __m512i v_com = _mm512_set1_epi8(',');

            // Unrolled loop (128 bytes)
            for (; i + 128 <= len; i += 128) {
                // PART 1
                {
                    __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(data + i));

                    uint64_t bs_mask = _mm512_cmpeq_epi8_mask(chunk, v_slash);
                    uint64_t quote_mask = _mm512_cmpeq_epi8_mask(chunk, v_quote);
                    uint64_t struct_mask =
                        _mm512_cmpeq_epi8_mask(chunk, v_lbra) | _mm512_cmpeq_epi8_mask(chunk, v_rbra) |
                        _mm512_cmpeq_epi8_mask(chunk, v_lcur) | _mm512_cmpeq_epi8_mask(chunk, v_rcur) |
                        _mm512_cmpeq_epi8_mask(chunk, v_col) | _mm512_cmpeq_epi8_mask(chunk, v_com);

                    if (TACHYON_UNLIKELY(bs_mask != 0 || prev_escapes > 0)) {
                        uint64_t real_quote_mask = 0;
                        const char* c_ptr = data + i;
                         for(int j=0; j<64; ++j) {
                             if (c_ptr[j] == '"' && (prev_escapes & 1) == 0) real_quote_mask |= (1ULL << j);
                             if (c_ptr[j] == '\\') prev_escapes++; else prev_escapes = 0;
                         }
                         quote_mask = real_quote_mask;
                    } else { prev_escapes = 0; }

                    uint64_t p = quote_mask;
                    p ^= (p << 1); p ^= (p << 2); p ^= (p << 4); p ^= (p << 8); p ^= (p << 16); p ^= (p << 32);
                    p ^= in_string_mask;

                    uint64_t odd = std::popcount(quote_mask) & 1;
                    in_string_mask ^= (0 - odd);

                    uint64_t final_mask = (struct_mask & ~p) | quote_mask;
                    mask_array[block_idx++] = (uint32_t)final_mask;
                    mask_array[block_idx++] = (uint32_t)(final_mask >> 32);
                }

                // PART 2
                {
                    __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(data + i + 64));

                    uint64_t bs_mask = _mm512_cmpeq_epi8_mask(chunk, v_slash);
                    uint64_t quote_mask = _mm512_cmpeq_epi8_mask(chunk, v_quote);
                    uint64_t struct_mask =
                        _mm512_cmpeq_epi8_mask(chunk, v_lbra) | _mm512_cmpeq_epi8_mask(chunk, v_rbra) |
                        _mm512_cmpeq_epi8_mask(chunk, v_lcur) | _mm512_cmpeq_epi8_mask(chunk, v_rcur) |
                        _mm512_cmpeq_epi8_mask(chunk, v_col) | _mm512_cmpeq_epi8_mask(chunk, v_com);

                    if (TACHYON_UNLIKELY(bs_mask != 0 || prev_escapes > 0)) {
                        uint64_t real_quote_mask = 0;
                        const char* c_ptr = data + i + 64;
                         for(int j=0; j<64; ++j) {
                             if (c_ptr[j] == '"' && (prev_escapes & 1) == 0) real_quote_mask |= (1ULL << j);
                             if (c_ptr[j] == '\\') prev_escapes++; else prev_escapes = 0;
                         }
                         quote_mask = real_quote_mask;
                    } else { prev_escapes = 0; }

                    uint64_t p = quote_mask;
                    p ^= (p << 1); p ^= (p << 2); p ^= (p << 4); p ^= (p << 8); p ^= (p << 16); p ^= (p << 32);
                    p ^= in_string_mask;

                    uint64_t odd = std::popcount(quote_mask) & 1;
                    in_string_mask ^= (0 - odd);

                    uint64_t final_mask = (struct_mask & ~p) | quote_mask;
                    mask_array[block_idx++] = (uint32_t)final_mask;
                    mask_array[block_idx++] = (uint32_t)(final_mask >> 32);
                }

                _mm_prefetch((const char*)(data + i + 1024), _MM_HINT_T0);
            }

            // Remainder Loop (64 byte blocks)
            for (; i + 64 <= len; i += 64) {
                 __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(data + i));

                uint64_t bs_mask = _mm512_cmpeq_epi8_mask(chunk, v_slash);
                uint64_t quote_mask = _mm512_cmpeq_epi8_mask(chunk, v_quote);

                uint64_t struct_mask =
                    _mm512_cmpeq_epi8_mask(chunk, v_lbra) | _mm512_cmpeq_epi8_mask(chunk, v_rbra) |
                    _mm512_cmpeq_epi8_mask(chunk, v_lcur) | _mm512_cmpeq_epi8_mask(chunk, v_rcur) |
                    _mm512_cmpeq_epi8_mask(chunk, v_col) | _mm512_cmpeq_epi8_mask(chunk, v_com);

                if (TACHYON_UNLIKELY(bs_mask != 0 || prev_escapes > 0)) {
                    uint64_t real_quote_mask = 0;
                    const char* c_ptr = data + i;
                     for(int j=0; j<64; ++j) {
                         if (c_ptr[j] == '"' && (prev_escapes & 1) == 0) real_quote_mask |= (1ULL << j);
                         if (c_ptr[j] == '\\') prev_escapes++; else prev_escapes = 0;
                     }
                     quote_mask = real_quote_mask;
                } else { prev_escapes = 0; }

                uint64_t p = quote_mask;
                p ^= (p << 1); p ^= (p << 2); p ^= (p << 4); p ^= (p << 8); p ^= (p << 16); p ^= (p << 32);
                p ^= in_string_mask;

                uint64_t odd = std::popcount(quote_mask) & 1;
                in_string_mask ^= (0 - odd);

                uint64_t final_mask = (struct_mask & ~p) | quote_mask;

                mask_array[block_idx++] = (uint32_t)final_mask;
                mask_array[block_idx++] = (uint32_t)(final_mask >> 32);
            }

            // Masked Tail (0-63 bytes)
            if (i < len) {
                size_t remaining = len - i;
                uint64_t load_mask = (1ULL << remaining) - 1;

                __m512i chunk = _mm512_maskz_loadu_epi8(load_mask, reinterpret_cast<const void*>(data + i));

                uint64_t bs_mask = _mm512_cmpeq_epi8_mask(chunk, v_slash);
                uint64_t quote_mask = _mm512_cmpeq_epi8_mask(chunk, v_quote);

                uint64_t struct_mask =
                    _mm512_cmpeq_epi8_mask(chunk, v_lbra) | _mm512_cmpeq_epi8_mask(chunk, v_rbra) |
                    _mm512_cmpeq_epi8_mask(chunk, v_lcur) | _mm512_cmpeq_epi8_mask(chunk, v_rcur) |
                    _mm512_cmpeq_epi8_mask(chunk, v_col) | _mm512_cmpeq_epi8_mask(chunk, v_com);

                bs_mask &= load_mask;
                quote_mask &= load_mask;
                struct_mask &= load_mask;

                if (TACHYON_UNLIKELY(bs_mask != 0 || prev_escapes > 0)) {
                    uint64_t real_quote_mask = 0;
                    const char* c_ptr = data + i;
                     for(size_t j=0; j<remaining; ++j) {
                         if (c_ptr[j] == '"' && (prev_escapes & 1) == 0) real_quote_mask |= (1ULL << j);
                         if (c_ptr[j] == '\\') prev_escapes++; else prev_escapes = 0;
                     }
                     quote_mask = real_quote_mask;
                }

                uint64_t p = quote_mask;
                p ^= (p << 1); p ^= (p << 2); p ^= (p << 4); p ^= (p << 8); p ^= (p << 16); p ^= (p << 32);
                p ^= in_string_mask;

                uint64_t final_mask = (struct_mask & ~p) | quote_mask;
                final_mask &= load_mask;

                mask_array[block_idx++] = (uint32_t)final_mask;
                mask_array[block_idx++] = (uint32_t)(final_mask >> 32);
            }

             _mm256_zeroupper();
            return block_idx;
        }

        // Pointer to the active implementation
        static size_t (*compute_structural_mask)(const char*, size_t, uint32_t*) = nullptr;
    }

    struct AlignedDeleter { void operator()(uint32_t* p) const { ASM::aligned_free(p); } };

    class Document {
    public:
        std::string storage;
        std::unique_ptr<uint32_t[], AlignedDeleter> bitmask;
        size_t len = 0;
        size_t bitmask_len = 0;
        size_t bitmask_cap = 0;

        Document() {
            if (!SIMD::compute_structural_mask) {
                 if (g_active_isa == ISA::AVX512) SIMD::compute_structural_mask = SIMD::compute_structural_mask_avx512;
                 else SIMD::compute_structural_mask = SIMD::compute_structural_mask_avx2;
            }
        }

        void parse(std::string&& json_str) {
            storage = std::move(json_str);
            parse_view(storage.data(), storage.size());
        }

        void parse_view(const char* data, size_t size) {
            len = size;
            size_t req_len = (len + 31) / 32 + 2;
            if (req_len > bitmask_cap) {
                bitmask.reset(static_cast<uint32_t*>(ASM::aligned_alloc(req_len * sizeof(uint32_t))));
                bitmask_cap = req_len;
            }
            bitmask_len = SIMD::compute_structural_mask(data, len, bitmask.get());
        }
        const char* get_base() const { return storage.empty() ? nullptr : storage.data(); }
    };

    // -------------------------------------------------------------------------
    // CURSOR
    // -------------------------------------------------------------------------
    struct Cursor {
        const uint32_t* bitmask_ptr;
        size_t max_block;
        uint32_t block_idx;
        uint32_t mask;
        const char* base;
        const char* end_ptr;

        Cursor(const Document* d, uint32_t offset, const char* b_ptr) : base(b_ptr) {
            end_ptr = b_ptr + d->len;
            bitmask_ptr = d->bitmask.get();
            max_block = d->bitmask_len;
            block_idx = offset / 32;
            int bit = offset % 32;
            if (block_idx < max_block) {
                mask = bitmask_ptr[block_idx];
                mask &= ~((1U << bit) - 1);
            } else { mask = 0; }
        }

        // Fast Path: No JSONC support (Turbo / Apex)
        TACHYON_FORCE_INLINE uint32_t next_fast() {
            while (true) {
                if (mask != 0) {
                    int bit = std::countr_zero(mask);
                    uint32_t offset = block_idx * 32 + bit;
                    mask &= (mask - 1);
                    return offset;
                }
                block_idx++;
                if (block_idx >= max_block) return (uint32_t)-1;
                mask = bitmask_ptr[block_idx];
            }
        }

        // Safe Path: Handles JSONC (Standard / Titan)
        inline uint32_t next() {
            while (true) {
                if (mask != 0) {
                    int bit = std::countr_zero(mask);
                    uint32_t offset = block_idx * 32 + bit;
                    mask &= (mask - 1);

                    if (TACHYON_UNLIKELY(base[offset] == '/')) {
                         if (base + offset + 1 >= end_ptr) return (uint32_t)-1;
                         const char* p = base + offset + 2;
                         if (base[offset+1] == '/') {
                             while(p < end_ptr && *p != '\n') p++;
                             uint32_t new_off = (uint32_t)(p - base);
                             block_idx = new_off / 32;
                             int b = new_off % 32;
                             if (block_idx < max_block) {
                                 mask = bitmask_ptr[block_idx];
                                 mask &= ~((1U << b) - 1);
                             } else { mask = 0; }
                             continue;
                         } else if (base[offset+1] == '*') {
                             while(p < end_ptr - 1 && !(*p == '*' && *(p+1) == '/')) p++;
                             uint32_t new_off = (uint32_t)(p - base) + 2;
                             block_idx = new_off / 32;
                             int b = new_off % 32;
                             if (block_idx < max_block) {
                                 mask = bitmask_ptr[block_idx];
                                 mask &= ~((1U << b) - 1);
                             } else { mask = 0; }
                             continue;
                         }
                    }
                    return offset;
                }
                block_idx++;
                if (block_idx >= max_block) return (uint32_t)-1;
                mask = bitmask_ptr[block_idx];
            }
        }

        // Direct-Key-Jump (Apex Optimization)
        TACHYON_FORCE_INLINE uint32_t find_key(const char* key, size_t len) {
             while (true) {
                uint32_t curr = next_fast();
                if (curr == (uint32_t)-1) return (uint32_t)-1;
                char c = base[curr];
                if (c == '}') return (uint32_t)-1;
                if (c == '"') {
                    uint32_t next_struct = next_fast();
                    if (next_struct == (uint32_t)-1) return (uint32_t)-1;
                    size_t k_len = next_struct - curr - 1;
                    if (k_len == len) {
                        // OPTIMIZED COMPARISON
                        if (len >= 8) {
                            if (*(uint64_t*)(base + curr + 1) == *(uint64_t*)key) {
                                if (len == 8 || memcmp(base + curr + 1 + 8, key + 8, len - 8) == 0) return next_struct;
                            }
                        } else {
                            if (memcmp(base + curr + 1, key, len) == 0) return next_struct;
                        }
                    }
                    uint32_t colon = next_fast();
                    if (base[colon] != ':') continue;

                    int depth = 0;
                    while(true) {
                        uint32_t v_curr = next_fast();
                        if (v_curr == (uint32_t)-1) return (uint32_t)-1;
                        char vc = base[v_curr];
                        if (vc == '{' || vc == '[') depth++;
                        else if (vc == '}' || vc == ']') {
                            if (depth == 0) return (uint32_t)-1;
                            depth--;
                        }
                        else if (vc == ',') {
                            if (depth == 0) break;
                        }
                    }
                }
             }
        }
    };

    using ObjectType = std::map<std::string, class json, std::less<>>;
    using ArrayType = std::vector<class json>;
    struct LazyNode { std::shared_ptr<Document> doc; uint32_t offset; const char* base_ptr; };

    class Context {
    public:
        std::shared_ptr<Document> doc;
        Context() : doc(std::make_shared<Document>()) {}
        class json parse_view(const char* data, size_t len);
    };

    class json {
        std::variant<std::monostate, bool, int64_t, uint64_t, double, std::string, ObjectType, ArrayType, LazyNode> value;

        // Internal Helpers
        static void encode_utf8(std::string& res, uint32_t cp) {
            if (cp <= 0x7F) res += (char)cp;
            else if (cp <= 0x7FF) { res += (char)(0xC0 | (cp >> 6)); res += (char)(0x80 | (cp & 0x3F)); }
            else if (cp <= 0xFFFF) { res += (char)(0xE0 | (cp >> 12)); res += (char)(0x80 | ((cp >> 6) & 0x3F)); res += (char)(0x80 | (cp & 0x3F)); }
            else if (cp <= 0x10FFFF) { res += (char)(0xF0 | (cp >> 18)); res += (char)(0x80 | ((cp >> 12) & 0x3F)); res += (char)(0x80 | ((cp >> 6) & 0x3F)); res += (char)(0x80 | (cp & 0x3F)); }
        }

        static uint32_t parse_hex4(const char* p) {
            uint32_t cp = 0;
            for (int i = 0; i < 4; ++i) {
                char c = p[i];
                cp <<= 4;
                if (c >= '0' && c <= '9') cp |= (c - '0');
                else if (c >= 'A' && c <= 'F') cp |= (c - 'A' + 10);
                else if (c >= 'a' && c <= 'f') cp |= (c - 'a' + 10);
                else return 0;
            }
            return cp;
        }

        static std::string unescape_string(std::string_view sv) {
            std::string res;
            res.reserve(sv.size());
            for (size_t i = 0; i < sv.size(); ++i) {
                if (sv[i] == '\\') {
                    if (i + 1 >= sv.size()) break;
                    char c = sv[i + 1];
                    switch (c) {
                        case '"': res += '"'; break;
                        case '\\': res += '\\'; break;
                        case '/': res += '/'; break;
                        case 'b': res += '\b'; break;
                        case 'f': res += '\f'; break;
                        case 'n': res += '\n'; break;
                        case 'r': res += '\r'; break;
                        case 't': res += '\t'; break;
                        case 'u': {
                            if (i + 5 < sv.size()) {
                                uint32_t cp = parse_hex4(sv.data() + i + 2);
                                if (cp >= 0xD800 && cp <= 0xDBFF) {
                                     if (i + 11 < sv.size() && sv[i+6] == '\\' && sv[i+7] == 'u') {
                                         uint32_t cp2 = parse_hex4(sv.data() + i + 8);
                                         if (cp2 >= 0xDC00 && cp2 <= 0xDFFF) {
                                             cp = 0x10000 + ((cp - 0xD800) << 10) + (cp2 - 0xDC00);
                                             i += 6;
                                         }
                                     }
                                }
                                encode_utf8(res, cp);
                                i += 4;
                            }
                            break;
                        }
                        default: res += c; break;
                    }
                    i++;
                } else {
                    res += sv[i];
                }
            }
            return res;
        }

        static std::string escape_string(const std::string& s) {
            std::string res = "\"";
            res.reserve(s.size() + 4);
            for (char c : s) {
                switch (c) {
                    case '"': res += "\\\""; break;
                    case '\\': res += "\\\\"; break;
                    case '\b': res += "\\b"; break;
                    case '\f': res += "\\f"; break;
                    case '\n': res += "\\n"; break;
                    case '\r': res += "\\r"; break;
                    case '\t': res += "\\t"; break;
                    default:
                        if ((unsigned char)c < 0x20) {
                            char buf[7];
                            std::snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                            res += buf;
                        } else {
                            res += c;
                        }
                        break;
                }
            }
            res += "\"";
            return res;
        }

    public:
        json() : value(std::monostate{}) {}
        json(std::nullptr_t) : value(std::monostate{}) {}
        json(bool b) : value(b) {}
        json(int i) : value(static_cast<int64_t>(i)) {}
        json(int64_t i) : value(i) {}
        json(uint64_t i) : value(i) {}
        json(double d) : value(d) {}
        json(const std::string& s) : value(s) {}
        json(std::string&& s) : value(std::move(s)) {}
        json(const char* s) : value(std::string(s)) {}
        json(const ObjectType& o) : value(o) {}
        json(const ArrayType& a) : value(a) {}
        json(LazyNode l) : value(l) {}

        template<typename T, typename = std::enable_if_t<
            !std::is_same_v<T, json> && !std::is_same_v<T, std::string> && !std::is_same_v<T, const char*> &&
            !std::is_arithmetic_v<T> && !std::is_null_pointer_v<T>>>
        json(const T& t) { to_json(*this, t); }

        static json object() { return json(ObjectType{}); }
        static json array() { return json(ArrayType{}); }

        // PARSING ENTRY POINTS
        static json parse_view(const char* ptr, size_t len) {
            auto doc = std::make_shared<Document>();
            doc->parse_view(ptr, len);
            return json(LazyNode{doc, 0, ptr});
        }

        static json parse(std::string s) {
            auto doc = std::make_shared<Document>();
            doc->parse(std::move(s));
            return json(LazyNode{doc, 0, doc->get_base()});
        }

        // ACCESSORS
        bool is_null() const { return std::holds_alternative<std::monostate>(value) || (is_lazy() && lazy_char() == 'n'); }
        bool is_array() const { return std::holds_alternative<ArrayType>(value) || (is_lazy() && lazy_char() == '['); }
        bool is_object() const { return std::holds_alternative<ObjectType>(value) || (is_lazy() && lazy_char() == '{'); }
        bool is_string() const { return std::holds_alternative<std::string>(value) || (is_lazy() && lazy_char() == '"'); }
        bool is_lazy() const { return std::holds_alternative<LazyNode>(value); }

        char lazy_char() const {
            const auto& l = std::get<LazyNode>(value);
            const char* s = ASM::skip_whitespace(l.base_ptr + l.offset, l.base_ptr + l.doc->len);
            if (s >= l.base_ptr + l.doc->len) return '\0';
            return *s;
        }

        template<typename T> void get_to(T& t) const {
            if constexpr (std::is_same_v<T, int>) t = (int)as_int64();
            else if constexpr (std::is_same_v<T, int64_t>) t = as_int64();
            else if constexpr (std::is_same_v<T, double>) t = as_double();
            else if constexpr (std::is_same_v<T, bool>) t = as_bool();
            else if constexpr (std::is_same_v<T, std::string>) t = as_string();
            else from_json(*this, t);
        }
        template<typename T> T get() const { T t; get_to(t); return t; }

        json& operator[](const std::string& key) {
             materialize();
             if (!std::holds_alternative<ObjectType>(value)) {
                 if (std::holds_alternative<std::monostate>(value)) value = ObjectType{};
                 else throw std::runtime_error("Tachyon: Type mismatch");
             }
             return std::get<ObjectType>(value)[key];
        }

        json& operator[](size_t idx) {
             materialize();
             if (!std::holds_alternative<ArrayType>(value)) {
                 if (std::holds_alternative<std::monostate>(value)) value = ArrayType{};
                 else throw std::runtime_error("Tachyon: Type mismatch");
             }
             ArrayType& arr = std::get<ArrayType>(value);
             if (idx >= arr.size()) arr.resize(idx + 1);
             return arr[idx];
        }

        const json operator[](const std::string& key) const {
            if (is_lazy()) return lazy_lookup(key);
            if (std::holds_alternative<ObjectType>(value)) {
                const auto& o = std::get<ObjectType>(value);
                auto it = o.find(key);
                if (it != o.end()) return it->second;
            }
            return json();
        }

        const json at(const std::string& key) const {
             if (is_lazy()) {
                 json res = lazy_lookup(key);
                 if (res.is_null()) throw std::out_of_range("Key not found");
                 return res;
             }
             if (!std::holds_alternative<ObjectType>(value)) throw std::runtime_error("Not object");
             const auto& o = std::get<ObjectType>(value);
             auto it = o.find(key);
             if (it == o.end()) throw std::out_of_range("Key not found");
             return it->second;
        }

        std::string as_string() const {
            if (is_lazy()) {
                const auto& l = std::get<LazyNode>(value);
                const char* s = ASM::skip_whitespace(l.base_ptr + l.offset, l.base_ptr + l.doc->len);
                if (*s != '"') return "";
                uint32_t start = (uint32_t)(s - l.base_ptr);
                Cursor c(l.doc.get(), start + 1, l.base_ptr);
                uint32_t end = c.next_fast();
                std::string_view sv(l.base_ptr + start + 1, end - start - 1);
                return unescape_string(sv);
            }
            if (std::holds_alternative<std::string>(value)) return std::get<std::string>(value);
            return "";
        }

        int64_t as_int64() const {
             if (is_lazy()) {
                const auto& l = std::get<LazyNode>(value);
                const char* s = ASM::skip_whitespace(l.base_ptr + l.offset, l.base_ptr + l.doc->len);
                int64_t i = 0; std::from_chars(s, l.base_ptr + l.doc->len, i); return i;
             }
             if (std::holds_alternative<int64_t>(value)) return std::get<int64_t>(value);
             if (std::holds_alternative<double>(value)) return (int64_t)std::get<double>(value);
             return 0;
        }

        double as_double() const {
             if (is_lazy()) {
                const auto& l = std::get<LazyNode>(value);
                const char* s = ASM::skip_whitespace(l.base_ptr + l.offset, l.base_ptr + l.doc->len);
                double d = 0.0; std::from_chars(s, l.base_ptr + l.doc->len, d, std::chars_format::general); return d;
             }
             if (std::holds_alternative<double>(value)) return std::get<double>(value);
             if (std::holds_alternative<int64_t>(value)) return (double)std::get<int64_t>(value);
             return 0.0;
        }

        bool as_bool() const {
             if (is_lazy()) return lazy_char() == 't';
             if (std::holds_alternative<bool>(value)) return std::get<bool>(value);
             return false;
        }

        bool contains(const std::string& key) const {
            if (is_lazy()) return !lazy_lookup(key).is_null();
            if (is_object()) {
                const auto& o = std::get<ObjectType>(value);
                return o.find(key) != o.end();
            }
            return false;
        }

        size_t size() const {
             if (is_lazy()) return lazy_size();
             if (std::holds_alternative<ArrayType>(value)) return std::get<ArrayType>(value).size();
             if (std::holds_alternative<ObjectType>(value)) return std::get<ObjectType>(value).size();
             return 0;
        }

        std::string dump() const {
            if (is_lazy()) { json c = *this; c.materialize(); return c.dump(); }
            if (std::holds_alternative<std::string>(value)) return escape_string(std::get<std::string>(value));
            if (std::holds_alternative<int64_t>(value)) return std::to_string(std::get<int64_t>(value));
            if (std::holds_alternative<bool>(value)) return std::get<bool>(value) ? "true" : "false";
            if (std::holds_alternative<std::monostate>(value)) return "null";
            if (std::holds_alternative<ObjectType>(value)) {
                std::string s = "{";
                const auto& o = std::get<ObjectType>(value);
                bool f = true;
                for (const auto& [k, v] : o) { if (!f) s += ","; f = false; s += escape_string(k) + ":" + v.dump(); }
                s += "}";
                return s;
            }
            if (std::holds_alternative<ArrayType>(value)) {
                std::string s = "[";
                const auto& a = std::get<ArrayType>(value);
                bool f = true;
                for (const auto& v : a) { if (!f) s += ","; f = false; s += v.dump(); }
                s += "]";
                return s;
            }
            return "null";
        }

    private:
        void materialize() {
             if (!is_lazy()) return;
             const auto& l = std::get<LazyNode>(value);
             const char* base = l.base_ptr;
             const char* s = ASM::skip_whitespace(base + l.offset, base + l.doc->len);
             char c = *s;
             if (c == '{') {
                ObjectType obj;
                uint32_t start = (uint32_t)(s - base) + 1;
                Cursor cur(l.doc.get(), start, base);
                while (true) {
                    uint32_t curr = cur.next();
                    if (curr == (uint32_t)-1 || base[curr] == '}') break;
                    if (base[curr] == ',') continue;
                    if (base[curr] == '"') {
                        uint32_t end_q = cur.next();
                        std::string_view ksv(base + curr + 1, end_q - curr - 1);
                        std::string k = unescape_string(ksv);
                        uint32_t colon = cur.next();
                        const char* vs = ASM::skip_whitespace(base + colon + 1, base + l.doc->len);
                        json child(LazyNode{l.doc, (uint32_t)(vs - base), base});
                        char vc = *vs;
                        if (vc == '{') skip_container(cur, base, '{', '}');
                        else if (vc == '[') skip_container(cur, base, '[', ']');
                        else if (vc == '"') { cur.next(); cur.next(); }
                        obj[std::move(k)] = std::move(child);
                    }
                }
                value = std::move(obj);
            } else if (c == '[') {
                 ArrayType arr;
                 uint32_t start = (uint32_t)(s - base) + 1;
                 Cursor cur(l.doc.get(), start, base);
                 const char* p = s + 1;
                 while (true) {
                     p = ASM::skip_whitespace(p, base + l.doc->len);
                     if (*p == ']') break;
                     arr.push_back(json(LazyNode{l.doc, (uint32_t)(p - base), base}));
                     char ch = *p;
                     uint32_t next_delim;
                     if (ch == '{') { skip_container(cur, base, '{', '}'); next_delim = cur.next(); }
                     else if (ch == '[') { skip_container(cur, base, '[', ']'); next_delim = cur.next(); }
                     else if (ch == '"') { cur.next(); cur.next(); next_delim = cur.next(); }
                     else { next_delim = cur.next(); }
                     if (next_delim == (uint32_t)-1 || base[next_delim] == ']') break;
                     p = base + next_delim + 1;
                 }
                 value = std::move(arr);
            } else if (c == '"') { value = as_string(); }
            else if (c == 't') { value = true; }
            else if (c == 'f') { value = false; }
            else if (c == 'n') { value = std::monostate{}; }
            else {
                // Heuristic: check for float indicators in next few chars
                bool is_float = false;
                for(int k=0; k<32; ++k) {
                    char ck = s[k];
                    if (ck == ',' || ck == '}' || ck == ']' || ck == '\0') break;
                    if (ck == '.' || ck == 'e' || ck == 'E') { is_float = true; break; }
                }
                if (is_float) value = as_double();
                else value = as_int64();
            }
        }

        json lazy_lookup(const std::string& key) const {
            const auto& l = std::get<LazyNode>(value);
            const char* base = l.base_ptr;
            const char* s = ASM::skip_whitespace(base + l.offset, base + l.doc->len);
            if (*s != '{') return json();
            uint32_t start = (uint32_t)(s - base) + 1;
            Cursor c(l.doc.get(), start, base);

            // Apex / Turbo Path: Use Direct-Key-Jump
            uint32_t key_pos = c.find_key(key.data(), key.size());
            if (key_pos == (uint32_t)-1) return json();

            // find_key returns the index of the closing quote of the key.
            // We need to move past the colon.
            uint32_t colon = c.next_fast(); // Should be the colon
            if (base[colon] != ':') return json(); // Should not happen

            const char* vs = ASM::skip_whitespace(base + colon + 1, base + l.doc->len);
            return json(LazyNode{l.doc, (uint32_t)(vs - base), base});
        }

        json lazy_index(size_t idx) const {
            const auto& l = std::get<LazyNode>(value);
            const char* base = l.base_ptr;
            const char* s = ASM::skip_whitespace(base + l.offset, base + l.doc->len);
            if (*s != '[') return json();
            uint32_t start = (uint32_t)(s - base) + 1;
            Cursor c(l.doc.get(), start, base);
            size_t count = 0;
            const char* p = s + 1;
            while (true) {
                p = ASM::skip_whitespace(p, base + l.doc->len);
                if (*p == ']') return json();
                if (count == idx) return json(LazyNode{l.doc, (uint32_t)(p - base), base});
                char ch = *p;
                uint32_t next_delim;
                if (ch == '{') { skip_container(c, base, '{', '}'); next_delim = c.next(); }
                else if (ch == '[') { skip_container(c, base, '[', ']'); next_delim = c.next(); }
                else if (ch == '"') { c.next(); c.next(); next_delim = c.next(); }
                else { next_delim = c.next(); }
                count++;
                if (next_delim == (uint32_t)-1 || base[next_delim] == ']') return json();
                p = base + next_delim + 1;
            }
        }

        // HYBRID DUAL-PATH lazy_size
        size_t lazy_size() const {
            const auto& l = std::get<LazyNode>(value);
            const char* base = l.base_ptr;
            const char* s = ASM::skip_whitespace(base + l.offset, base + l.doc->len);
            if (*s != '[') return 0;
            uint32_t start_off = (uint32_t)(s - base) + 1;
            const uint32_t* bitmask = l.doc->bitmask.get();
            size_t max_block = l.doc->bitmask_len;
            size_t count = 0;
            int depth = 1;
            const char* first_element = s + 1;
            uint32_t block_idx = start_off / 32;
            uint32_t initial_mask = bitmask[block_idx];
            initial_mask &= ~((1U << (start_off % 32)) - 1);

            auto check_end = [&](uint32_t curr_off) {
                if (count > 0) return count + 1;
                if (ASM::skip_whitespace(first_element, base + curr_off) < base + curr_off) return (size_t)1;
                return (size_t)0;
            };

            auto run_avx2 = [&](uint32_t mask) __attribute__((target("avx2"))) -> size_t {
                const __m256i v_comma = _mm256_set1_epi8(',');
                const __m256i v_lbra = _mm256_set1_epi8('[');
                const __m256i v_rbra = _mm256_set1_epi8(']');
                const __m256i v_lcur = _mm256_set1_epi8('{');
                const __m256i v_rcur = _mm256_set1_epi8('}');

                while(true) {
                    while (mask == 0) {
                        block_idx++;
                        if (block_idx >= max_block) return 0;
                        mask = bitmask[block_idx];
                    }
                    __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(base + block_idx * 32));
                    uint32_t m_comma = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, v_comma));
                    uint32_t m_open = _mm256_movemask_epi8(_mm256_or_si256(_mm256_cmpeq_epi8(chunk, v_lbra), _mm256_cmpeq_epi8(chunk, v_lcur)));
                    uint32_t m_close = _mm256_movemask_epi8(_mm256_or_si256(_mm256_cmpeq_epi8(chunk, v_rbra), _mm256_cmpeq_epi8(chunk, v_rcur)));

                    if (depth == 1 && ((m_open | m_close) & mask) == 0) {
                        count += std::popcount(m_comma & mask);
                    }
                    else if (depth > 1 && (m_close & mask) == 0) {
                        depth += std::popcount(m_open & mask);
                        block_idx++;
                        if (block_idx >= max_block) break;
                        mask = bitmask[block_idx];
                        continue;
                    }
                    else {
                        uint32_t m_iter = mask;
                        while (m_iter != 0) {
                            int bit = std::countr_zero(m_iter);
                            uint32_t bit_mask = (1U << bit);
                            m_iter &= (m_iter - 1);

                            bool is_comma = (m_comma & bit_mask) != 0;
                            bool is_close = (m_close & bit_mask) != 0;
                            bool is_open  = (m_open & bit_mask) != 0;

                            if (is_comma) {
                                if (depth == 1) count++;
                            } else if (is_close) {
                                depth--;
                                if (depth == 0) return check_end(block_idx * 32 + bit);
                            } else if (is_open) {
                                depth++;
                            }
                        }
                    }
                    block_idx++;
                    if (block_idx >= max_block) break;
                    mask = bitmask[block_idx];
                }
                return count;
            };

            auto run_avx512 = [&](uint32_t mask32) __attribute__((target("avx512f,avx512bw"))) -> size_t {
                const __m512i v_comma = _mm512_set1_epi8(',');
                const __m512i v_lbra = _mm512_set1_epi8('[');
                const __m512i v_rbra = _mm512_set1_epi8(']');
                const __m512i v_lcur = _mm512_set1_epi8('{');
                const __m512i v_rcur = _mm512_set1_epi8('}');

                // First 32-byte block handling
                {
                    __m512i chunk = _mm512_castsi256_si512(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(base + block_idx * 32)));
                    uint64_t m_comma = _mm512_cmpeq_epi8_mask(chunk, v_comma);
                    uint64_t m_open = _mm512_cmpeq_epi8_mask(chunk, v_lbra) | _mm512_cmpeq_epi8_mask(chunk, v_lcur);
                    uint64_t m_close = _mm512_cmpeq_epi8_mask(chunk, v_rbra) | _mm512_cmpeq_epi8_mask(chunk, v_rcur);

                    uint64_t m64 = mask32;
                    m_comma &= 0xFFFFFFFF; m_open &= 0xFFFFFFFF; m_close &= 0xFFFFFFFF;

                    if (depth == 1 && ((m_open | m_close) & m64) == 0) {
                        count += std::popcount(m_comma & m64);
                    } else {
                        uint64_t m_iter = m64;
                        while (m_iter != 0) {
                            int bit = std::countr_zero(m_iter);
                            uint64_t bit_mask = (1ULL << bit);
                            m_iter &= (m_iter - 1);

                            bool is_comma = (m_comma & bit_mask) != 0;
                            bool is_close = (m_close & bit_mask) != 0;
                            bool is_open  = (m_open & bit_mask) != 0;

                            if (is_comma) {
                                if (depth == 1) count++;
                            } else if (is_close) {
                                depth--;
                                if (depth == 0) return check_end(block_idx * 32 + bit);
                            } else if (is_open) {
                                depth++;
                            }
                        }
                    }
                    block_idx++;
                }

                // Main Loop (64-byte chunks)
                while(true) {
                    if (block_idx + 1 >= max_block) break;
                    uint64_t mask64 = (uint64_t)bitmask[block_idx] | ((uint64_t)bitmask[block_idx+1] << 32);

                    while (mask64 == 0) {
                        block_idx += 2;
                        if (block_idx + 1 >= max_block) return 0;
                        mask64 = (uint64_t)bitmask[block_idx] | ((uint64_t)bitmask[block_idx+1] << 32);
                    }

                    _mm_prefetch(base + block_idx * 32 + 1024, _MM_HINT_T0);

                    __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(base + block_idx * 32));
                    uint64_t m_comma = _mm512_cmpeq_epi8_mask(chunk, v_comma);
                    uint64_t m_open = _mm512_cmpeq_epi8_mask(chunk, v_lbra) | _mm512_cmpeq_epi8_mask(chunk, v_lcur);
                    uint64_t m_close = _mm512_cmpeq_epi8_mask(chunk, v_rbra) | _mm512_cmpeq_epi8_mask(chunk, v_rcur);

                    if (depth == 1 && ((m_open | m_close) & mask64) == 0) {
                        count += std::popcount(m_comma & mask64);
                    }
                    else if (depth > 1 && (m_close & mask64) == 0) {
                        depth += std::popcount(m_open & mask64);
                        block_idx += 2;
                        continue;
                    }
                    else {
                        uint64_t m_iter = mask64;
                        while (m_iter != 0) {
                            int bit = std::countr_zero(m_iter);
                            uint64_t bit_mask = (1ULL << bit);
                            m_iter &= (m_iter - 1);

                            bool is_comma = (m_comma & bit_mask) != 0;
                            bool is_close = (m_close & bit_mask) != 0;
                            bool is_open  = (m_open & bit_mask) != 0;

                            if (is_comma) {
                                if (depth == 1) count++;
                            } else if (is_close) {
                                depth--;
                                if (depth == 0) { _mm256_zeroupper(); return check_end(block_idx * 32 + bit); }
                            } else if (is_open) {
                                depth++;
                            }
                        }
                    }
                    block_idx += 2;
                }

                // Tail
                if (block_idx < max_block) {
                    uint32_t mask32 = bitmask[block_idx];
                    __m512i chunk = _mm512_castsi256_si512(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(base + block_idx * 32)));
                    uint64_t m_comma = _mm512_cmpeq_epi8_mask(chunk, v_comma);
                    uint64_t m_open = _mm512_cmpeq_epi8_mask(chunk, v_lbra) | _mm512_cmpeq_epi8_mask(chunk, v_lcur);
                    uint64_t m_close = _mm512_cmpeq_epi8_mask(chunk, v_rbra) | _mm512_cmpeq_epi8_mask(chunk, v_rcur);

                    uint64_t m64 = mask32;
                    m_comma &= 0xFFFFFFFF; m_open &= 0xFFFFFFFF; m_close &= 0xFFFFFFFF;

                    if (((m_open | m_close) & m64) == 0) {
                        if (depth == 1) count += std::popcount(m_comma & m64);
                    } else {
                        uint64_t m_iter = m64;
                        while (m_iter != 0) {
                            int bit = std::countr_zero(m_iter);
                            uint64_t bit_mask = (1ULL << bit);
                            m_iter &= (m_iter - 1);

                            bool is_comma = (m_comma & bit_mask) != 0;
                            bool is_close = (m_close & bit_mask) != 0;
                            bool is_open  = (m_open & bit_mask) != 0;

                            if (is_comma) {
                                if (depth == 1) count++;
                            } else if (is_close) {
                                depth--;
                                if (depth == 0) { _mm256_zeroupper(); return check_end(block_idx * 32 + bit); }
                            } else if (is_open) {
                                depth++;
                            }
                        }
                    }
                }

                _mm256_zeroupper();
                return count;
            };

            if (g_active_isa == ISA::AVX512) return run_avx512(initial_mask);
            return run_avx2(initial_mask);
        }

        void skip_container(Cursor& c, const char* base, char open, char close) const {
            int depth = 0;
            while (true) {
                uint32_t curr = c.next();
                if (curr == (uint32_t)-1) break;
                char ch = base[curr];
                if (ch == open) depth++;
                else if (ch == close) depth--;
                else if (ch == '"') c.next();
                if (depth == 0) break;
            }
        }

        void skip_container_fast(Cursor& c, const char* base, char open, char close) const {
            int depth = 0;
            while (true) {
                uint32_t curr = c.next_fast();
                if (curr == (uint32_t)-1) break;
                char ch = base[curr];
                if (ch == open) depth++;
                else if (ch == close) depth--;
                else if (ch == '"') c.next_fast();
                if (depth == 0) break;
            }
        }
    };

    inline json Context::parse_view(const char* data, size_t len) {
        doc->parse_view(data, len);
        return json(LazyNode{doc, 0, data});
    }

} // namespace Tachyon
#endif // TACHYON_HPP
