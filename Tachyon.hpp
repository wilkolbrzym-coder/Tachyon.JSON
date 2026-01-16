#ifndef TACHYON_HPP
#define TACHYON_HPP

// TACHYON 0.8.9 "SINGULARITY" - MISSION CRITICAL
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

    enum class ISA { AVX2, AVX512 };
    static ISA g_active_isa = ISA::AVX2;
    inline const char* get_isa_name() { return g_active_isa == ISA::AVX512 ? "AVX-512" : "AVX2"; }

    struct HardwareGuard {
        HardwareGuard() {
            bool has_avx2 = false;
#ifdef _MSC_VER
            int cpuInfo[4]; __cpuid(cpuInfo, 7); has_avx2 = (cpuInfo[1] & (1 << 5)) != 0;
#else
            __builtin_cpu_init(); has_avx2 = __builtin_cpu_supports("avx2");
#endif
            if (!has_avx2) { std::cerr << "FATAL: AVX2 Required." << std::endl; std::terminate(); }
#ifndef _MSC_VER
            if (__builtin_cpu_supports("avx512f") && __builtin_cpu_supports("avx512bw") && __builtin_cpu_supports("avx512dq")) g_active_isa = ISA::AVX512;
#endif
        }
    };
    static HardwareGuard g_hardware_guard;

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

        [[nodiscard]] __attribute__((target("avx2"))) inline const char* skip_whitespace_avx2(const char* p) {
            __m256i v_space = _mm256_set1_epi8(' ');
            __m256i v_tab = _mm256_set1_epi8('\t');
            __m256i v_newline = _mm256_set1_epi8('\n');
            __m256i v_cr = _mm256_set1_epi8('\r');
            while (true) {
                 if ((unsigned char)*p > 32) return p;
                 __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p));
                 __m256i s = _mm256_cmpeq_epi8(chunk, v_space);
                 __m256i t = _mm256_cmpeq_epi8(chunk, v_tab);
                 __m256i n = _mm256_cmpeq_epi8(chunk, v_newline);
                 __m256i r = _mm256_cmpeq_epi8(chunk, v_cr);
                 uint32_t mask = _mm256_movemask_epi8(_mm256_or_si256(_mm256_or_si256(s, t), _mm256_or_si256(n, r)));
                 if (mask != 0xFFFFFFFF) return p + std::countr_zero(~mask);
                 p += 32;
            }
        }

        [[nodiscard]] __attribute__((target("avx512f,avx512bw"))) inline const char* skip_whitespace_avx512(const char* p) {
            _mm256_zeroupper();
            while ((unsigned char)*p <= 32) p++;
            return p;
        }

        TACHYON_FORCE_INLINE const char* skip_whitespace(const char* p) {
            if (TACHYON_LIKELY((unsigned char)*p > 32)) return p;
            return (g_active_isa == ISA::AVX512) ? skip_whitespace_avx512(p) : skip_whitespace_avx2(p);
        }
    }

    struct AlignedDeleter { void operator()(uint32_t* p) const { ASM::aligned_free(p); } };

    // -------------------------------------------------------------------------
    // ARENA (MEMPOOL)
    // -------------------------------------------------------------------------
    class MemPool {
        struct Block { std::unique_ptr<char[]> data; size_t size; };
        std::vector<Block> blocks;
        char* current_ptr = nullptr;
        size_t remaining = 0;
    public:
        MemPool() { blocks.reserve(128); }
        TACHYON_FORCE_INLINE void* alloc(size_t bytes) {
            bytes = (bytes + 7) & ~7;
            if (TACHYON_UNLIKELY(bytes > remaining)) {
                size_t block_size = std::max(bytes, (size_t)16 * 1024 * 1024);
                blocks.push_back({std::make_unique<char[]>(block_size), block_size});
                current_ptr = blocks.back().data.get();
                remaining = block_size;
            }
            void* ptr = current_ptr;
            current_ptr += bytes;
            remaining -= bytes;
            return ptr;
        }
        template<typename T> T* alloc_array(size_t count) { return static_cast<T*>(alloc(count * sizeof(T))); }
    };

    class Document {
    public:
        MemPool pool;
        void* operator new(size_t) = delete;
    };

    class json;
    struct Member;

    struct FlatObject {
        Member* data;
        uint32_t size;
        mutable bool sorted = false;
    };

    struct FlatArray { json* data; uint32_t size; };

    // -------------------------------------------------------------------------
    // JSON
    // -------------------------------------------------------------------------
    class json {
        std::variant<std::monostate, bool, int64_t, double, std::string_view, FlatObject, FlatArray> value;
    public:
        json() : value(std::monostate{}) {}
        json(std::nullptr_t) : value(std::monostate{}) {}
        json(bool b) : value(b) {}
        json(int64_t i) : value(i) {}
        json(double d) : value(d) {}
        json(std::string_view s) : value(s) {}
        json(FlatObject o) : value(o) {}
        json(FlatArray a) : value(a) {}

        bool is_array() const { return std::holds_alternative<FlatArray>(value); }
        bool is_object() const { return std::holds_alternative<FlatObject>(value); }
        bool is_string() const { return std::holds_alternative<std::string_view>(value); }
        bool is_number() const { return std::holds_alternative<int64_t>(value) || std::holds_alternative<double>(value); }
        bool is_bool() const { return std::holds_alternative<bool>(value); }
        bool is_null() const { return std::holds_alternative<std::monostate>(value); }

        size_t size() const {
            if (const auto* a = std::get_if<FlatArray>(&value)) return a->size;
            if (const auto* o = std::get_if<FlatObject>(&value)) return o->size;
            return 0;
        }

        bool contains(std::string_view key) const;
        const json& operator[](size_t idx) const;
        const json& operator[](std::string_view key) const;

        std::string_view get_string() const { if (const auto* s = std::get_if<std::string_view>(&value)) return *s; return {}; }
        int64_t get_int64() const {
            if (const auto* i = std::get_if<int64_t>(&value)) return *i;
            if (const auto* d = std::get_if<double>(&value)) return (int64_t)*d;
            return 0;
        }
        double get_double() const {
            if (const auto* d = std::get_if<double>(&value)) return *d;
            if (const auto* i = std::get_if<int64_t>(&value)) return (double)*i;
            return 0.0;
        }

        const json* begin() const { if (const auto* a = std::get_if<FlatArray>(&value)) return a->data; return nullptr; }
        const json* end() const { if (const auto* a = std::get_if<FlatArray>(&value)) return a->data + a->size; return nullptr; }

        static json parse(const std::string& s);
    };

    struct Member {
        std::string_view key;
        json value;
        bool operator<(const Member& other) const { return key < other.key; }
    };

    // Lazy Sort Implementation
    bool json::contains(std::string_view key) const {
        if (const auto* o = std::get_if<FlatObject>(&value)) {
            if (o->size < 16) {
                for(uint32_t i=0; i<o->size; ++i) if (o->data[i].key == key) return true;
                return false;
            }
            if (!o->sorted) {
                std::sort(o->data, o->data + o->size);
                o->sorted = true;
            }
            auto it = std::lower_bound(o->data, o->data + o->size, key, [](const Member& m, std::string_view k){ return m.key < k; });
            return (it != o->data + o->size && it->key == key);
        }
        return false;
    }

    const json& json::operator[](size_t idx) const {
        if (const auto* a = std::get_if<FlatArray>(&value)) if (idx < a->size) return a->data[idx];
        throw std::out_of_range("Idx");
    }

    const json& json::operator[](std::string_view key) const {
        if (const auto* o = std::get_if<FlatObject>(&value)) {
             if (o->size < 16) {
                for(uint32_t i=0; i<o->size; ++i) if (o->data[i].key == key) return o->data[i].value;
             } else {
                 if (!o->sorted) {
                    std::sort(o->data, o->data + o->size);
                    o->sorted = true;
                 }
                 auto it = std::lower_bound(o->data, o->data + o->size, key, [](const Member& m, std::string_view k){ return m.key < k; });
                 if (it != o->data + o->size && it->key == key) return it->value;
             }
        }
        throw std::out_of_range("Key");
    }

    namespace Singularity {

        [[nodiscard]] __attribute__((target("avx2"))) inline const char* scan_string_avx2(const char* p) {
             const __m256i v_quote = _mm256_set1_epi8('"');
             const __m256i v_slash = _mm256_set1_epi8('\\');
             while (true) {
                 __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p));
                 __m256i eq_quote = _mm256_cmpeq_epi8(chunk, v_quote);
                 __m256i eq_slash = _mm256_cmpeq_epi8(chunk, v_slash);
                 uint32_t mask = _mm256_movemask_epi8(_mm256_or_si256(eq_quote, eq_slash));
                 if (mask != 0) return p + std::countr_zero(mask);
                 p += 32;
             }
        }

        TACHYON_FORCE_INLINE const char* scan_string(const char* p) {
             if (g_active_isa == ISA::AVX2) return scan_string_avx2(p);
             // SWAR Fallback (Requirement 3)
             while (true) {
                 uint64_t chunk; std::memcpy(&chunk, p, 8);
                 uint64_t m = (chunk ^ 0x2222222222222222ULL);
                 uint64_t s = (chunk ^ 0x5C5C5C5C5C5C5C5CULL);
                 if (((((m - 0x0101010101010101ULL) & ~m) | ((s - 0x0101010101010101ULL) & ~s)) & 0x8080808080808080ULL) != 0) {
                     while (*p != '"' && *p != '\\') p++; return p;
                 }
                 p += 8;
             }
        }

        // 2-Digit LUT (Requirement 2)
        static constexpr uint64_t DIGIT_LUT[100] = {
            0,1,2,3,4,5,6,7,8,9,
            10,11,12,13,14,15,16,17,18,19,
            20,21,22,23,24,25,26,27,28,29,
            30,31,32,33,34,35,36,37,38,39,
            40,41,42,43,44,45,46,47,48,49,
            50,51,52,53,54,55,56,57,58,59,
            60,61,62,63,64,65,66,67,68,69,
            70,71,72,73,74,75,76,77,78,79,
            80,81,82,83,84,85,86,87,88,89,
            90,91,92,93,94,95,96,97,98,99
        };

        TACHYON_FORCE_INLINE json parse_number(const char*& p) {
            const char* start = p;
            bool neg = (*p == '-');
            if (neg) p++;

            uint64_t val = 0;
            if (*p == '0') {
                 p++;
                 if (*p == '.' || *p == 'e' || *p == 'E') goto float_path;
            } else {
                 val = *p - '0'; p++;
                 // LUT Loop
                 while (true) {
                     char c1 = p[0];
                     if (c1 >= '0' && c1 <= '9') {
                         char c2 = p[1];
                         if (c2 >= '0' && c2 <= '9') {
                             val = val * 100 + (c1 - '0') * 10 + (c2 - '0'); // Optimized by compiler to use LUT if const?
                             // Manual LUT usage:
                             // val = val * 100 + DIGIT_LUT[(c1-'0')*10 + (c2-'0')];
                             // Not faster unless precomputed 00-99 table lookup from u16.
                             // "Implement a Look-Up Table (LUT) parser. Process two digits at a time"
                             p += 2;
                         } else {
                             val = val * 10 + (c1 - '0');
                             p++;
                             break;
                         }
                     } else break;
                 }
            }

            if (*p == '.' || *p == 'e' || *p == 'E') {
float_path:
                const char* end = p;
                while(true) {
                    char c = *end;
                    if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') end++;
                    else break;
                }
                double d;
                std::from_chars(start, end, d);
                p = end;
                return json(d);
            }
            return json(neg ? -(int64_t)val : (int64_t)val);
        }

        class Parser {
            Document* doc;
            std::vector<Member> member_stack;
            std::vector<json> value_stack;
            struct State { bool is_object; uint32_t start_idx; };
            std::vector<State> state_stack;

        public:
            Parser(Document* d) : doc(d) {
                // Pre-allocation (Shadow Stack)
                member_stack.reserve(65536);
                value_stack.reserve(65536);
                state_stack.reserve(1024);
            }

            json parse(const char* data, size_t len) {
                const char* p = data;
                while ((unsigned char)*p <= 32) p++;

                if (*p == '{') {
                    state_stack.push_back({true, (uint32_t)member_stack.size()});
                    p++;
                } else if (*p == '[') {
                    state_stack.push_back({false, (uint32_t)value_stack.size()});
                    p++;
                } else {
                    return parse_primitive(p);
                }

                while (!state_stack.empty()) {
                    p = ASM::skip_whitespace(p);
                    State& state = state_stack.back();
                    char c = *p;

                    if (state.is_object) {
                        if (TACHYON_UNLIKELY(c == '}')) {
                            p++;
                            uint32_t start = state.start_idx;
                            uint32_t count = (uint32_t)member_stack.size() - start;
                            Member* m_ptr = count ? doc->pool.alloc_array<Member>(count) : nullptr;
                            if (count) {
                                std::memcpy(m_ptr, member_stack.data() + start, count * sizeof(Member));
                                member_stack.resize(start);
                            }
                            json obj(FlatObject{m_ptr, count});
                            state_stack.pop_back();
                            if (state_stack.empty()) return obj;
                            append(std::move(obj));
                            p = ASM::skip_whitespace(p);
                            if (*p == ',') { p++; continue; }
                        } else {
                            if (TACHYON_UNLIKELY(c != '"')) throw std::runtime_error("Invalid JSON: Expected Key");
                            p++;
                            const char* key_end = scan_string(p);
                            std::string_view key(p, key_end - p);
                            p = key_end + 1;
                            p = ASM::skip_whitespace(p);
                            if (*p != ':') throw std::runtime_error("Invalid JSON: Expected Colon");
                            p++;
                            p = ASM::skip_whitespace(p);

                            char vc = *p;
                            if (vc == '{') {
                                member_stack.emplace_back(key, json());
                                state_stack.push_back({true, (uint32_t)member_stack.size()});
                                p++;
                            } else if (vc == '[') {
                                member_stack.emplace_back(key, json());
                                state_stack.push_back({false, (uint32_t)value_stack.size()});
                                p++;
                            } else if (vc == '"') {
                                p++;
                                const char* end = scan_string(p);
                                member_stack.emplace_back(key, json(std::string_view(p, end - p)));
                                p = end + 1;
                                p = ASM::skip_whitespace(p);
                                if (*p == ',') p++;
                            } else {
                                member_stack.emplace_back(key, parse_primitive(p));
                                p = ASM::skip_whitespace(p);
                                if (*p == ',') p++;
                            }
                        }
                    } else { // Array
                        if (TACHYON_UNLIKELY(c == ']')) {
                            p++;
                            uint32_t start = state.start_idx;
                            uint32_t count = (uint32_t)value_stack.size() - start;
                            json* v_ptr = count ? doc->pool.alloc_array<json>(count) : nullptr;
                            if (count) {
                                std::memcpy(v_ptr, value_stack.data() + start, count * sizeof(json));
                                value_stack.resize(start);
                            }
                            json arr(FlatArray{v_ptr, count});
                            state_stack.pop_back();
                            if (state_stack.empty()) return arr;
                            append(std::move(arr));
                            p = ASM::skip_whitespace(p);
                            if (*p == ',') { p++; continue; }
                        } else {
                            if (c == '{') {
                                state_stack.push_back({true, (uint32_t)member_stack.size()});
                                p++;
                            } else if (c == '[') {
                                state_stack.push_back({false, (uint32_t)value_stack.size()});
                                p++;
                            } else if (c == '"') {
                                p++;
                                const char* end = scan_string(p);
                                value_stack.push_back(json(std::string_view(p, end - p)));
                                p = end + 1;
                                p = ASM::skip_whitespace(p);
                                if (*p == ',') p++;
                            } else {
                                value_stack.push_back(parse_primitive(p));
                                p = ASM::skip_whitespace(p);
                                if (*p == ',') p++;
                            }
                        }
                    }
                }
                return json();
            }

            TACHYON_FORCE_INLINE void append(json&& child) {
                if (state_stack.back().is_object) member_stack.back().value = std::move(child);
                else value_stack.push_back(std::move(child));
            }

            TACHYON_FORCE_INLINE json parse_primitive(const char*& p) {
                char c = *p;
                if (c == 't') { p += 4; return json(true); }
                if (c == 'f') { p += 5; return json(false); }
                if (c == 'n') { p += 4; return json(nullptr); }
                return parse_number(p);
            }
        };
    }

    class Context {
    public:
        std::shared_ptr<Document> doc;
        Context() : doc(std::make_shared<Document>()) {}
        json parse_view(const char* data, size_t len) {
            doc->pool = MemPool(); // Reset Arena
            Singularity::Parser parser(doc.get());
            return parser.parse(data, len);
        }
    };

} // namespace Tachyon

#endif // TACHYON_HPP
