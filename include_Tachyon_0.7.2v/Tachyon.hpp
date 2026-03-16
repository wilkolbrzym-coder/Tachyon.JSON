#ifndef TACHYON_HPP
#define TACHYON_HPP

// TACHYON 0.7.6 "QUASAR" - MISSION CRITICAL
// The World's Fastest JSON & CSV Library (AVX2 Optimized)
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
#include <filesystem>
#include <fstream>

#ifdef _MSC_VER
#include <intrin.h>
#include <malloc.h>
#else
#include <immintrin.h>
#include <cpuid.h>
#endif

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

    enum class Mode { Apex, Turbo, CSV };

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
        }
    };
    static HardwareGuard g_hardware_guard;

    template<typename T> concept Numeric = std::integral<T> || std::floating_point<T>;
    template<typename T> concept StringLike = std::convertible_to<T, std::string_view>;

    class json;
    template<typename T> void to_json(json& j, const T& t);
    template<typename T> void from_json(const json& j, T& t);

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

        [[nodiscard]] __attribute__((target("avx2"))) inline const char* skip_whitespace(const char* p, const char* end) {
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

        __attribute__((target("avx2")))
        inline bool validate_utf8(const char* data, size_t len) {
            const __m256i v_128 = _mm256_set1_epi8(0x80);
            size_t i = 0;
            while (i + 32 <= len) {
                __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
                if (_mm256_testz_si256(chunk, v_128)) {
                    i += 32;
                    continue;
                }
                size_t j = 0;
                while (j < 32) {
                    unsigned char c = (unsigned char)data[i+j];
                    if (c < 0x80) {
                        j++;
                    } else {
                        size_t n = 0;
                        if ((c & 0xE0) == 0xC0) n = 2;
                        else if ((c & 0xF0) == 0xE0) n = 3;
                        else if ((c & 0xF8) == 0xF0) n = 4;
                        else return false;
                        if (i + j + n > len) return false;
                        for (size_t k = 1; k < n; ++k) {
                            if ((data[i+j+k] & 0xC0) != 0x80) return false;
                        }
                        j += n;
                    }
                }
                i += j;
            }
            while (i < len) {
                unsigned char c = (unsigned char)data[i];
                if (c < 0x80) {
                    i++;
                } else {
                    size_t n = 0;
                    if ((c & 0xE0) == 0xC0) n = 2;
                    else if ((c & 0xF0) == 0xE0) n = 3;
                    else if ((c & 0xF8) == 0xF0) n = 4;
                    else return false;
                    if (i + n > len) return false;
                    for (size_t k = 1; k < n; ++k) {
                        if ((data[i+k] & 0xC0) != 0x80) return false;
                    }
                    i += n;
                }
            }
            return true;
        }
    }

    namespace SIMD {
        __attribute__((target("avx2")))
        inline size_t compute_structural_mask_avx2(const char* data, size_t len, uint32_t* mask_array, size_t& prev_escapes, uint32_t& in_string_mask, bool& utf8_error) {
            static const __m256i v_lo_tbl = _mm256_broadcastsi128_si256(_mm_setr_epi8(0, 0, 0x40, 0, 0, 0, 0, 0, 0, 0, 0x80, 0x80, 0xA0, 0x80, 0, 0x80));
            static const __m256i v_hi_tbl = _mm256_broadcastsi128_si256(_mm_setr_epi8(0, 0, 0xC0, 0x80, 0, 0xA0, 0, 0x80, 0, 0, 0, 0, 0, 0, 0, 0));
            static const __m256i v_0f = _mm256_set1_epi8(0x0F);
            static const __m256i v_utf8_check = _mm256_set1_epi8(0x80);

            size_t i = 0;
            size_t block_idx = 0;
            size_t p_esc = prev_escapes;
            uint32_t is_mask = in_string_mask;

            for (; i + 128 <= len; i += 128) {
                uint32_t m0, m1, m2, m3;

                _mm_prefetch((const char*)(data + i + 1024), _MM_HINT_T0);

                __m256i chunk0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
                __m256i chunk1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i + 32));
                __m256i chunk2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i + 64));
                __m256i chunk3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i + 96));

                __m256i or_all = _mm256_or_si256(_mm256_or_si256(chunk0, chunk1), _mm256_or_si256(chunk2, chunk3));
                if (TACHYON_UNLIKELY(!_mm256_testz_si256(or_all, v_utf8_check))) {
                     if (!ASM::validate_utf8(data + i, 128)) {
                         utf8_error = true;
                         return block_idx;
                     }
                }

                auto compute_chunk_loaded = [&](__m256i chunk, size_t offset) -> uint32_t {
                    __m256i lo = _mm256_and_si256(chunk, v_0f);
                    __m256i hi = _mm256_and_si256(_mm256_srli_epi16(chunk, 4), v_0f);
                    __m256i char_class = _mm256_and_si256(_mm256_shuffle_epi8(v_lo_tbl, lo), _mm256_shuffle_epi8(v_hi_tbl, hi));
                    uint32_t struct_mask = _mm256_movemask_epi8(char_class);
                    uint32_t quote_mask = _mm256_movemask_epi8(_mm256_slli_epi16(char_class, 1));
                    uint32_t bs_mask = _mm256_movemask_epi8(_mm256_slli_epi16(char_class, 2));

                    if (TACHYON_UNLIKELY(bs_mask != 0 || p_esc > 0)) {
                         uint32_t real_quote_mask = 0;
                         const char* c_ptr = data + offset;
                         for(int j=0; j<32; ++j) {
                             if (c_ptr[j] == '"' && (p_esc & 1) == 0) real_quote_mask |= (1U << j);
                             if (c_ptr[j] == '\\') p_esc++; else p_esc = 0;
                         }
                         quote_mask = real_quote_mask;
                    } else { p_esc = 0; }

                    uint32_t p = quote_mask;
                    p ^= (p << 1); p ^= (p << 2); p ^= (p << 4); p ^= (p << 8); p ^= (p << 16);
                    p ^= is_mask;
                    uint32_t odd = std::popcount(quote_mask) & 1;
                    is_mask ^= (0 - odd);
                    return (struct_mask & ~p) | quote_mask;
                };

                m0 = compute_chunk_loaded(chunk0, i);
                m1 = compute_chunk_loaded(chunk1, i + 32);
                m2 = compute_chunk_loaded(chunk2, i + 64);
                m3 = compute_chunk_loaded(chunk3, i + 96);
                __m128i m_pack = _mm_setr_epi32(m0, m1, m2, m3);
                _mm_stream_si128((__m128i*)(mask_array + block_idx), m_pack); // Restore Stream for Throughput
                block_idx += 4;
            }
            prev_escapes = p_esc;
            in_string_mask = is_mask;
            return block_idx;
        }
    }

    struct AlignedDeleter { void operator()(uint32_t* p) const { ASM::aligned_free(p); } };

    class Document {
    public:
        std::string storage;
        std::unique_ptr<uint32_t[], AlignedDeleter> bitmask_ptr;
        alignas(32) uint32_t sbo[128]; // 512 bytes stack buffer (Handles up to 4KB input)
        uint32_t* bitmask = nullptr;
        const char* base_ptr = nullptr;
        size_t len = 0;
        size_t bitmask_cap = 0;

        size_t processed_bytes = 0;
        size_t processed_blocks = 0;
        size_t prev_escapes = 0;
        uint32_t in_string_mask = 0;

        Document() {}

        void parse(std::string&& json_str) {
            storage = std::move(json_str);
            init_view(storage.data(), storage.size());
        }

        void init_view(const char* data, size_t size) {
            base_ptr = data;
            len = size;
            size_t req_blocks = (len + 31) / 32 + 8;

            // SBO logic
            if (req_blocks <= 128) {
                bitmask = sbo;
            } else {
                if (req_blocks > bitmask_cap) {
                    bitmask_ptr.reset(static_cast<uint32_t*>(ASM::aligned_alloc(req_blocks * sizeof(uint32_t))));
                    bitmask_cap = req_blocks;
                }
                bitmask = bitmask_ptr.get();
            }

            processed_bytes = 0;
            processed_blocks = 0;
            prev_escapes = 0;
            in_string_mask = 0;
        }

        TACHYON_FORCE_INLINE void ensure_mask(size_t target_offset) {
            if (target_offset < processed_bytes) return;
            size_t target_aligned = (target_offset + 65536) & ~65535;
            if (target_aligned > len) target_aligned = len;
            if (target_aligned <= processed_bytes) target_aligned = len;

            size_t bytes_to_proc = target_aligned - processed_bytes;
            if (bytes_to_proc == 0) return;

            bool utf8_error = false;
            size_t blocks_written = SIMD::compute_structural_mask_avx2(
                base_ptr + processed_bytes, bytes_to_proc, bitmask + processed_blocks, prev_escapes, in_string_mask, utf8_error
            );

            if (TACHYON_UNLIKELY(utf8_error)) {
                 throw std::runtime_error("Invalid UTF-8");
            }

            size_t processed_in_simd = blocks_written * 32;
            size_t remainder_start = processed_bytes + processed_in_simd;

            if (target_aligned == len) {
                if (!ASM::validate_utf8(base_ptr + remainder_start, len - remainder_start)) {
                    throw std::runtime_error("Invalid UTF-8");
                }

                uint32_t final_mask = 0;
                int j = 0;
                for (size_t k = remainder_start; k < len; ++k, ++j) {
                     if (j == 32) { bitmask[processed_blocks + blocks_written++] = final_mask; final_mask = 0; j = 0; }
                    char c = base_ptr[k];
                    bool is_quote = (c == '"') && ((prev_escapes & 1) == 0);
                    if (c == '\\') prev_escapes++; else prev_escapes = 0;
                    if (in_string_mask) {
                        if (is_quote) { in_string_mask = 0; final_mask |= (1U << j); }
                    } else {
                        if (is_quote) { in_string_mask = ~0; final_mask |= (1U << j); }
                        else if (c=='{'||c=='}'||c=='['||c==']'||c==':'||c==','||c=='/') final_mask |= (1U << j);
                    }
                }
                bitmask[processed_blocks + blocks_written++] = final_mask;
                processed_bytes = len;
            } else {
                processed_bytes += processed_in_simd;
            }
            processed_blocks += blocks_written;
        }
    };

    struct Cursor {
        Document* doc;
        uint32_t block_idx;
        uint32_t mask;
        const char* base;

        Cursor(Document* d, uint32_t offset) : doc(d), base(d->base_ptr) {
            doc->ensure_mask(offset + 128);
            block_idx = offset / 32;
            int bit = offset % 32;
            if (block_idx < doc->processed_blocks) {
                mask = doc->bitmask[block_idx];
                mask &= ~((1U << bit) - 1);
            } else { mask = 0; }
        }

        TACHYON_FORCE_INLINE uint32_t next() {
            while (true) {
                if (mask != 0) {
                    int bit = std::countr_zero(mask);
                    uint32_t offset = block_idx * 32 + bit;
                    mask &= (mask - 1);
                    return offset;
                }
                block_idx++;
                if (block_idx >= doc->processed_blocks) {
                     if (doc->processed_bytes >= doc->len) return (uint32_t)-1;
                     doc->ensure_mask(doc->processed_bytes + 65536);
                     if (block_idx >= doc->processed_blocks) return (uint32_t)-1;
                }
                mask = doc->bitmask[block_idx];
            }
        }

        TACHYON_FORCE_INLINE uint32_t find_key(const char* key, size_t len) {
             while (true) {
                uint32_t curr = next();
                if (curr == (uint32_t)-1) return (uint32_t)-1;
                char c = base[curr];
                if (c == '}') return (uint32_t)-1;
                if (c == '"') {
                    uint32_t next_struct = next();
                    if (next_struct == (uint32_t)-1) return (uint32_t)-1;
                    size_t k_len = next_struct - curr - 1;
                    bool match = false;
                    if (k_len == len) {
                        if (len >= 8) {
                            if (*(uint64_t*)(base + curr + 1) == *(uint64_t*)key) {
                                if (len == 8 || memcmp(base + curr + 1 + 8, key + 8, len - 8) == 0) match = true;
                            }
                        } else {
                            if (memcmp(base + curr + 1, key, len) == 0) match = true;
                        }
                    }
                    uint32_t colon = next();
                    if (match) {
                         const char* val_ptr = ASM::skip_whitespace(base + colon + 1, doc->base_ptr + doc->len);
                         return (uint32_t)(val_ptr - base);
                    }
                    int depth = 0;
                     while(true) {
                         uint32_t v_curr = next();
                         if (v_curr == (uint32_t)-1) return (uint32_t)-1;
                         char vc = base[v_curr];
                         if (depth == 0) {
                             if (vc == ',' || vc == '}') {
                                 if (vc == ',') break;
                                 if (vc == '}') return (uint32_t)-1;
                             }
                         }
                         if (vc == '{' || vc == '[') depth++;
                         else if (vc == '}' || vc == ']') depth--;
                    }
                }
             }
        }
    };

    using ObjectType = std::map<std::string, class json, std::less<>>;
    using ArrayType = std::vector<class json>;
    struct LazyNode {
        Document* doc;
        uint32_t offset;
        std::shared_ptr<Document> owner; // Null if View
    };

    class Context {
    public:
        std::shared_ptr<Document> doc;
        Context() : doc(std::make_shared<Document>()) {}
        class json parse_view(const char* data, size_t len);
    };

    class json {
        std::variant<std::monostate, bool, int64_t, uint64_t, double, std::string, ObjectType, ArrayType, LazyNode> value;
        static std::string unescape_string(std::string_view sv) {
            std::string res; res.reserve(sv.size());
            for(size_t i=0; i<sv.size(); ++i) {
                if(sv[i] == '\\' && i+1 < sv.size()) { char c = sv[++i]; if(c == 'n') res += '\n'; else if(c == 't') res += '\t'; else res += c; }
                else res += sv[i];
            }
            return res;
        }

        void materialize() {
             if (auto* l = std::get_if<LazyNode>(&value)) {
                 const char* s = ASM::skip_whitespace(l->doc->base_ptr + l->offset, l->doc->base_ptr + l->doc->len);
                 if (*s == '{') {
                     ObjectType obj;
                     Cursor c(l->doc, (uint32_t)(s - l->doc->base_ptr) + 1);
                     while(true) {
                         uint32_t curr = c.next();
                         if (curr == (uint32_t)-1 || l->doc->base_ptr[curr] == '}') break;
                         if (l->doc->base_ptr[curr] == ',') continue;
                         if (l->doc->base_ptr[curr] == '"') {
                             uint32_t end_q = c.next();
                             std::string key = unescape_string(std::string_view(l->doc->base_ptr + curr + 1, end_q - curr - 1));
                             uint32_t colon = c.next();
                             const char* val_ptr = ASM::skip_whitespace(l->doc->base_ptr + colon + 1, l->doc->base_ptr + l->doc->len);
                             obj[key] = json(LazyNode{l->doc, (uint32_t)(val_ptr - l->doc->base_ptr), l->owner});

                             int depth = 0;
                             while(true) {
                                 uint32_t v = c.next();
                                 if (v == (uint32_t)-1) break;
                                 char vc = l->doc->base_ptr[v];
                                 if (depth == 0 && (vc == ',' || vc == '}')) break;
                                 if (vc == '{' || vc == '[') depth++;
                                 else if (vc == '}' || vc == ']') depth--;
                             }
                         }
                     }
                     value = std::move(obj);
                 } else if (*s == '[') {
                     ArrayType arr;
                     Cursor c(l->doc, (uint32_t)(s - l->doc->base_ptr) + 1);
                     const char* ptr = s + 1;
                     while(true) {
                         ptr = ASM::skip_whitespace(ptr, l->doc->base_ptr + l->doc->len);
                         if (*ptr == ']') break;
                         arr.push_back(json(LazyNode{l->doc, (uint32_t)(ptr - l->doc->base_ptr), l->owner}));

                         int depth = 0;
                         while(true) {
                             uint32_t v = c.next();
                             if (v == (uint32_t)-1) break;
                             char vc = l->doc->base_ptr[v];
                             if (depth == 0 && (vc == ',' || vc == ']')) { ptr = l->doc->base_ptr + v + 1; if (vc == ']') ptr--; break; }
                             if (vc == '{' || vc == '[') depth++;
                             else if (vc == '}' || vc == ']') depth--;
                         }
                     }
                     value = std::move(arr);
                 }
             }
        }

    public:
        json() : value(std::monostate{}) {}
        json(LazyNode l) : value(l) {}
        json(bool b) : value(b) {}
        json(std::string s) : value(std::move(s)) {}
        json(ObjectType o) : value(std::move(o)) {}
        json(ArrayType a) : value(std::move(a)) {}

        template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>>>
        json(T t) { if constexpr (std::is_floating_point_v<T>) value = (double)t; else if constexpr (std::is_unsigned_v<T>) value = (uint64_t)t; else value = (int64_t)t; }

        template<typename T, typename = std::enable_if_t<!std::is_same_v<T, json> && !std::is_same_v<T, std::string> && !std::is_same_v<T, const char*> && !std::is_arithmetic_v<T> && !std::is_null_pointer_v<T>>>
        json(const T& t) { to_json(*this, t); }

        static json object() { return json(ObjectType{}); }
        static json array() { return json(ArrayType{}); }
        static json parse_view(const char* ptr, size_t len) { auto doc = std::make_shared<Document>(); doc->init_view(ptr, len); return json(LazyNode{doc.get(), 0, doc}); }
        static json parse(std::string s) { auto doc = std::make_shared<Document>(); doc->parse(std::move(s)); return json(LazyNode{doc.get(), 0, doc}); }

        static std::vector<std::vector<std::string>> parse_csv(const std::string& csv) {
            std::vector<std::vector<std::string>> rows; rows.reserve(csv.size() / 50);
            const char* p = csv.data(); const char* end = p + csv.size();
            while (p < end) {
                std::vector<std::string> row; row.reserve(10);
                while (p < end) {
                    const char* start = p; bool quote = false;
                    if (*p == '"') { quote = true; start++; p++; }
                    while (p < end) {
                        if (quote && *p == '"') { if (p+1 < end && *(p+1) == '"') { p+=2; continue; } else { break; } }
                        if (!quote && (*p == ',' || *p == '\n' || *p == '\r')) break;
                        p++;
                    }
                    row.emplace_back(start, p - start);
                    if (quote) p++;
                    if (p < end && *p == ',') p++; else break;
                }
                rows.push_back(std::move(row));
                if (p < end && *p == '\r') p++; if (p < end && *p == '\n') p++;
            }
            return rows;
        }

        template<typename T>
        static std::vector<T> parse_csv_typed(const std::string& csv) {
            auto rows = parse_csv(csv);
            std::vector<T> result;
            if (rows.empty()) return result;
            const auto& headers = rows[0];
            result.reserve(rows.size() - 1);
            for (size_t i = 1; i < rows.size(); ++i) {
                const auto& row = rows[i];
                if (row.size() != headers.size()) continue;
                ObjectType o;
                for (size_t j = 0; j < headers.size(); ++j) o[headers[j]] = json(row[j]);
                T t; from_json(json(o), t);
                result.push_back(std::move(t));
            }
            return result;
        }

        template<typename T> void get_to(T& t) const {
            if constexpr (std::is_same_v<T, int>) t = (int)as_int64();
            else if constexpr (std::is_same_v<T, int64_t>) t = as_int64();
            else if constexpr (std::is_same_v<T, uint64_t>) t = (uint64_t)as_int64();
            else if constexpr (std::is_same_v<T, double>) t = as_double();
            else if constexpr (std::is_same_v<T, bool>) t = as_bool();
            else if constexpr (std::is_same_v<T, std::string>) t = as_string();
            else from_json(*this, t);
        }
        template<typename T> T get() const { T t; get_to(t); return t; }

        bool is_array() const {
            if (std::holds_alternative<ArrayType>(value)) return true;
            if (auto* l = std::get_if<LazyNode>(&value)) return ASM::skip_whitespace(l->doc->base_ptr + l->offset, l->doc->base_ptr + l->doc->len)[0] == '[';
            return false;
        }

        size_t size() const {
             if (std::holds_alternative<ArrayType>(value)) return std::get<ArrayType>(value).size();
             if (std::holds_alternative<ObjectType>(value)) return std::get<ObjectType>(value).size();
             if (auto* l = std::get_if<LazyNode>(&value)) {
                 const char* s = ASM::skip_whitespace(l->doc->base_ptr + l->offset, l->doc->base_ptr + l->doc->len);
                 if (*s == '[') {
                     const char* next_char = ASM::skip_whitespace(s + 1, l->doc->base_ptr + l->doc->len);
                     if (*next_char == ']') return 0;
                     size_t count = 1;
                     Cursor c(l->doc, (uint32_t)(s - l->doc->base_ptr) + 1);
                     int depth = 1;
                     while(true) {
                         uint32_t off = c.next();
                         if (off == (uint32_t)-1) break;
                         char ch = l->doc->base_ptr[off];
                         if (depth == 1 && ch == ',') count++;
                         if (ch == '{' || ch == '[') depth++;
                         else if (ch == '}' || ch == ']') { depth--; if (depth == 0) return count; }
                     }
                 }
             }
             return 0;
        }

        bool contains(const std::string& key) const {
            if (auto* l = std::get_if<LazyNode>(&value)) {
                const char* base = l->doc->base_ptr; const char* s = ASM::skip_whitespace(base + l->offset, base + l->doc->len);
                if (*s != '{') return false;
                Cursor c(l->doc, (uint32_t)(s - base) + 1);
                return c.find_key(key.data(), key.size()) != (uint32_t)-1;
            }
            if (auto* o = std::get_if<ObjectType>(&value)) return o->contains(key);
            return false;
        }

        const json at(const std::string& key) const {
             if (auto* l = std::get_if<LazyNode>(&value)) {
                const char* base = l->doc->base_ptr; const char* s = ASM::skip_whitespace(base + l->offset, base + l->doc->len);
                if (*s != '{') throw std::runtime_error("Not an object");
                Cursor c(l->doc, (uint32_t)(s - base) + 1);
                uint32_t val_start = c.find_key(key.data(), key.size());
                if (val_start == (uint32_t)-1) throw std::out_of_range("Key not found");
                return json(LazyNode{l->doc, val_start, l->owner});
             }
             if (auto* o = std::get_if<ObjectType>(&value)) return o->at(key);
             throw std::runtime_error("Type mismatch");
        }

        std::string as_string() const {
             if (auto* s = std::get_if<std::string>(&value)) return *s;
             if (auto* l = std::get_if<LazyNode>(&value)) {
                 const char* s = ASM::skip_whitespace(l->doc->base_ptr + l->offset, l->doc->base_ptr + l->doc->len);
                 if (*s == '"') {
                     Cursor c(l->doc, (uint32_t)(s - l->doc->base_ptr) + 1);
                     uint32_t end = c.next();
                     size_t start_idx = (s - l->doc->base_ptr) + 1;
                     return unescape_string(std::string_view(l->doc->base_ptr + start_idx, end - start_idx));
                 }
             }
             return "";
        }

        int64_t as_int64() const {
            if (auto* i = std::get_if<int64_t>(&value)) return *i;
            if (auto* u = std::get_if<uint64_t>(&value)) return (int64_t)*u;
            if (auto* s = std::get_if<std::string>(&value)) { int64_t v = 0; std::from_chars(s->data(), s->data() + s->size(), v); return v; }
            if (auto* l = std::get_if<LazyNode>(&value)) {
                 const char* s = ASM::skip_whitespace(l->doc->base_ptr + l->offset, l->doc->base_ptr + l->doc->len);
                 int64_t v; std::from_chars(s, l->doc->base_ptr + l->doc->len, v); return v;
            }
            return 0;
        }

        double as_double() const {
            if (auto* d = std::get_if<double>(&value)) return *d;
            if (auto* s = std::get_if<std::string>(&value)) { double v = 0.0; std::from_chars(s->data(), s->data() + s->size(), v); return v; }
            if (auto* l = std::get_if<LazyNode>(&value)) {
                 const char* s = ASM::skip_whitespace(l->doc->base_ptr + l->offset, l->doc->base_ptr + l->doc->len);
                 double v; std::from_chars(s, l->doc->base_ptr + l->doc->len, v); return v;
            }
            return 0.0;
        }

        bool as_bool() const {
            if (auto* b = std::get_if<bool>(&value)) return *b;
            if (auto* s = std::get_if<std::string>(&value)) return *s == "true";
            if (auto* l = std::get_if<LazyNode>(&value)) return *ASM::skip_whitespace(l->doc->base_ptr + l->offset, l->doc->base_ptr + l->doc->len) == 't';
            return false;
        }

        json& operator[](const std::string& key) {
             if (std::holds_alternative<LazyNode>(value)) materialize();
             if (!std::holds_alternative<ObjectType>(value)) value = ObjectType{};
             return std::get<ObjectType>(value)[key];
        }

        json& operator[](size_t index) {
             if (std::holds_alternative<LazyNode>(value)) materialize();
             if (!std::holds_alternative<ArrayType>(value)) value = ArrayType{};
             auto& arr = std::get<ArrayType>(value);
             if (index >= arr.size()) arr.resize(index + 1);
             return arr[index];
        }
    };

    inline json Context::parse_view(const char* data, size_t len) {
        doc->init_view(data, len);
        return json(LazyNode{doc.get(), 0, nullptr}); // View mode: No ownership (shared_ptr is null)
    }

    inline void from_json(const json& j, uint64_t& val) { val = (uint64_t)j.as_int64(); }

    template<typename T>
    void from_json(const json& j, std::vector<T>& v) {
        v.clear();
        json copy = j; // materializes if lazy inside operator[]
        size_t s = copy.size();
        v.reserve(s);
        for(size_t i=0; i<s; ++i) {
            T t; copy[i].get_to(t);
            v.push_back(std::move(t));
        }
    }

} // namespace Tachyon
#endif // TACHYON_HPP
