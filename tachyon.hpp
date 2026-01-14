#ifndef TACHYON_HPP
#define TACHYON_HPP

// TACHYON 7.5 BETA - "QUASAR"
// The Ultimate Drop-in Replacement for nlohmann::json
// (C) 2026 Tachyon Systems by wilkolbrzym-coder
// License: MIT

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
#include <fstream>
#include <iterator>

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

namespace tachyon {

    enum class ISA { AVX2, AVX512 };
    static ISA g_active_isa = ISA::AVX2;

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
                // Ideally we fallback to scalar, but for now we throw to avoid terminate
                 throw std::runtime_error("FATAL ERROR: Tachyon requires a CPU with AVX2 support.");
            }
#ifndef _MSC_VER
            if (__builtin_cpu_supports("avx512f") && __builtin_cpu_supports("avx512bw") && __builtin_cpu_supports("avx512dq")) {
                g_active_isa = ISA::AVX512;
            }
#endif
        }
    };
    static HardwareGuard g_hardware_guard;

    namespace asm_utils {
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
                 _mm256_zeroupper();
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
    }

    namespace simd {
        __attribute__((target("avx2")))
        inline size_t compute_structural_mask_avx2(const char* data, size_t len, uint32_t* mask_array) {
            static const __m256i v_lo_tbl = _mm256_broadcastsi128_si256(_mm_setr_epi8(0, 0, 0x40, 0, 0, 0, 0, 0, 0, 0, 0x80, 0x80, 0xA0, 0x80, 0, 0x80));
            static const __m256i v_hi_tbl = _mm256_broadcastsi128_si256(_mm_setr_epi8(0, 0, 0xC0, 0x80, 0, 0xA0, 0, 0x80, 0, 0, 0, 0, 0, 0, 0, 0));
            static const __m256i v_0f = _mm256_set1_epi8(0x0F);

            size_t i = 0;
            size_t block_idx = 0;
            uint64_t prev_escapes = 0;
            uint32_t in_string_mask = 0;

            for (; i + 32 <= len; i += 32) {
                __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
                __m256i lo = _mm256_and_si256(chunk, v_0f);
                __m256i hi = _mm256_and_si256(_mm256_srli_epi16(chunk, 4), v_0f);
                __m256i char_class = _mm256_and_si256(_mm256_shuffle_epi8(v_lo_tbl, lo), _mm256_shuffle_epi8(v_hi_tbl, hi));
                uint32_t struct_mask = _mm256_movemask_epi8(char_class);
                uint32_t quote_mask = _mm256_movemask_epi8(_mm256_slli_epi16(char_class, 1));
                uint32_t bs_mask = _mm256_movemask_epi8(_mm256_slli_epi16(char_class, 2));

                if (TACHYON_UNLIKELY(bs_mask != 0 || prev_escapes > 0)) {
                        uint32_t real_quote_mask = 0;
                        const char* c_ptr = data + i;
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
                mask_array[block_idx++] = (struct_mask & ~p) | quote_mask;
            }
            if (i < len) {
                uint32_t final_mask = 0;
                int j = 0;
                for (; i < len; ++i, ++j) {
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

        // AVX-512 Skip
        __attribute__((target("avx512f,avx512bw")))
        inline size_t compute_structural_mask_avx512(const char* data, size_t len, uint32_t* mask_array) {
             return compute_structural_mask_avx2(data, len, mask_array);
        }
    }

    struct AlignedDeleter { void operator()(uint32_t* p) const { asm_utils::aligned_free(p); } };

    class Document {
    public:
        std::string storage;
        std::unique_ptr<uint32_t[], AlignedDeleter> bitmask;
        size_t len = 0;
        size_t bitmask_len = 0;
        size_t bitmask_cap = 0;

        void parse_view(const char* data, size_t size) {
            len = size;
            size_t req_len = (len + 31) / 32 + 2;
            if (req_len > bitmask_cap) {
                bitmask.reset(static_cast<uint32_t*>(asm_utils::aligned_alloc(req_len * sizeof(uint32_t))));
                bitmask_cap = req_len;
            }
            if (g_active_isa == ISA::AVX512) bitmask_len = simd::compute_structural_mask_avx512(data, len, bitmask.get());
            else bitmask_len = simd::compute_structural_mask_avx2(data, len, bitmask.get());
        }
    };

    struct LazyNode {
        std::shared_ptr<Document> doc;
        uint32_t offset;
        const char* base_ptr;
        size_t length; // 0 means unknown/re-calculate
    };

    struct Cursor {
        const uint32_t* bitmask_ptr;
        size_t max_block;
        uint32_t block_idx;
        uint32_t mask;
        const char* base;

        Cursor(const Document* d, uint32_t offset, const char* b_ptr) : base(b_ptr) {
            bitmask_ptr = d->bitmask.get();
            max_block = d->bitmask_len;
            block_idx = offset / 32;
            int bit = offset % 32;
            if (block_idx < max_block) {
                mask = bitmask_ptr[block_idx];
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
                if (block_idx >= max_block) return (uint32_t)-1;
                mask = bitmask_ptr[block_idx];
            }
        }
    };

    class json {
        friend std::ostream& operator<<(std::ostream& o, const json& j);
        friend std::istream& operator>>(std::istream& i, json& j);

    public:
        using object_t = std::map<std::string, json, std::less<>>;
        using array_t = std::vector<json>;
        using string_t = std::string;
        using boolean_t = bool;
        using number_integer_t = int64_t;
        using number_unsigned_t = uint64_t;
        using number_float_t = double;

    private:
        std::variant<std::monostate, boolean_t, number_integer_t, number_unsigned_t, number_float_t, string_t, object_t, array_t, LazyNode> value;

        void skip_container(Cursor& c, const char* base, char open, char close) {
            int depth = 0;
            while (true) {
                uint32_t curr = c.next();
                if (curr == (uint32_t)-1) break;
                char ch = base[curr];
                if (ch == open) depth++;
                else if (ch == close) depth--;
                if (depth == 0) break;
            }
        }

        static std::string unescape_string(std::string_view sv) {
            std::string res; res.reserve(sv.size());
            for(size_t i=0; i<sv.size(); ++i) {
                if(sv[i] == '\\' && i+1 < sv.size()) {
                    i++;
                    if(sv[i]=='n') res+='\n';
                    else if(sv[i]=='t') res+='\t';
                    else if(sv[i]=='r') res+='\r';
                    else if(sv[i]=='"') res+='"';
                    else if(sv[i]=='\\') res+='\\';
                    else res += sv[i];
                } else res += sv[i];
            }
            return res;
        }

        void materialize() {
             if (std::holds_alternative<LazyNode>(value)) {
                 LazyNode l = std::get<LazyNode>(value);
                 const char* base = l.base_ptr;
                 const char* s = asm_utils::skip_whitespace(base + l.offset, base + l.doc->len);

                 if (s >= base + l.doc->len) { value = std::monostate{}; return; }

                 char c = *s;
                 uint32_t start = (uint32_t)(s - base) + 1;

                 if (c == '{') {
                    object_t obj;
                    Cursor cur(l.doc.get(), start, base);
                    while (true) {
                        uint32_t curr = cur.next(); // Expected: Key or End
                        if (curr == (uint32_t)-1 || base[curr] == '}') break;

                        // curr should be quote
                        if (base[curr] == ',') continue; // Robustness: skip comma if it was left over (shouldn't be)

                        if (base[curr] == '"') {
                            uint32_t end_q = cur.next();
                            std::string_view ksv(base + curr + 1, end_q - curr - 1);
                            std::string k = unescape_string(ksv);

                            uint32_t colon = cur.next();
                            const char* vs = asm_utils::skip_whitespace(base + colon + 1, base + l.doc->len);
                            uint32_t val_offset = (uint32_t)(vs - base);

                            LazyNode child_node{l.doc, val_offset, base, 0};

                            // Advance cursor past value
                            uint32_t next_delim;
                            char vc = *vs;
                            if (vc == '{') { skip_container(cur, base, '{', '}'); next_delim = cur.next(); }
                            else if (vc == '[') { skip_container(cur, base, '[', ']'); next_delim = cur.next(); }
                            else if (vc == '"') { cur.next(); cur.next(); next_delim = cur.next(); }
                            else { next_delim = cur.next(); } // primitive

                            obj[std::move(k)] = json(child_node);

                            if (next_delim != (uint32_t)-1 && base[next_delim] == '}') break;
                        } else if (base[curr] == '}') {
                            break;
                        }
                    }
                    value = std::move(obj);
                 } else if (c == '[') {
                     array_t arr;
                     Cursor cur(l.doc.get(), start, base);
                     const char* p = s + 1;
                     while (true) {
                         p = asm_utils::skip_whitespace(p, base + l.doc->len);
                         if (*p == ']') break;

                         uint32_t val_offset = (uint32_t)(p - base);
                         arr.push_back(json(LazyNode{l.doc, val_offset, base, 0}));

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
                 } else if (c == '"') {
                     Cursor cur(l.doc.get(), start, base);
                     uint32_t end_q = cur.next();
                     std::string_view sv(base + start, end_q - start);
                     value = unescape_string(sv);
                 } else if (c == 't') { value = true; }
                 else if (c == 'f') { value = false; }
                 else if (c == 'n') { value = std::monostate{}; }
                 else {
                     char* end_ptr;
                     double d = std::strtod(s, &end_ptr);
                     if (std::string_view(s, end_ptr-s).find('.') == std::string::npos &&
                         std::string_view(s, end_ptr-s).find('e') == std::string::npos &&
                         std::string_view(s, end_ptr-s).find('E') == std::string::npos) {
                         value = (int64_t)d;
                     } else {
                         value = d;
                     }
                 }
             }
        }

        size_t lazy_size() const {
             const_cast<json*>(this)->materialize();
             if (is_array()) return std::get<array_t>(value).size();
             if (is_object()) return std::get<object_t>(value).size();
             return 0;
        }

    public:
        // Constructors
        json() : value(std::monostate{}) {}
        json(std::nullptr_t) : value(std::monostate{}) {}
        json(bool b) : value(b) {}
        json(int i) : value(static_cast<int64_t>(i)) {}
        json(int64_t i) : value(i) {}
        json(size_t i) : value(static_cast<uint64_t>(i)) {}
        json(double d) : value(d) {}
        json(const char* s) : value(string_t(s)) {}
        json(const std::string& s) : value(s) {}
        json(const object_t& o) : value(o) {}
        json(const array_t& a) : value(a) {}
        json(LazyNode l) : value(l) {}

        json(std::initializer_list<json> init) {
            bool is_obj = std::all_of(init.begin(), init.end(), [](const json& j){
                return j.is_array() && j.size() == 2 && j[0].is_string();
            });
            if (is_obj) {
                object_t o;
                for (const auto& element : init) {
                    o[element[0].get<std::string>()] = element[1];
                }
                value = o;
            } else {
                value = array_t(init);
            }
        }

        static json parse(const std::string& s) {
            auto doc = std::make_shared<Document>();
            doc->parse_view(s.data(), s.size());
            doc->storage = s;
            return json(LazyNode{doc, 0, doc->storage.data(), s.size()});
        }

        static json parse(const char* s) { return parse(std::string(s)); }

        // Implicit Conversions
        operator int() const { return get<int>(); }
        operator int64_t() const { return get<int64_t>(); }
        operator double() const { return get<double>(); }
        operator bool() const { return get<bool>(); }
        operator std::string() const { return get<std::string>(); }

        // Type Checks
        bool is_null() const {
            if (std::holds_alternative<LazyNode>(value)) return const_cast<json*>(this)->lazy_peek() == 'n';
            return std::holds_alternative<std::monostate>(value);
        }
        bool is_boolean() const {
            if (std::holds_alternative<LazyNode>(value)) { char c = const_cast<json*>(this)->lazy_peek(); return c == 't' || c == 'f'; }
            return std::holds_alternative<boolean_t>(value);
        }
        bool is_number() const {
             if (std::holds_alternative<LazyNode>(value)) { char c = const_cast<json*>(this)->lazy_peek(); return isdigit(c) || c=='-'; }
             return std::holds_alternative<number_integer_t>(value) || std::holds_alternative<number_unsigned_t>(value) || std::holds_alternative<number_float_t>(value);
        }
        bool is_string() const {
             if (std::holds_alternative<LazyNode>(value)) return const_cast<json*>(this)->lazy_peek() == '"';
             return std::holds_alternative<string_t>(value);
        }
        bool is_object() const {
             if (std::holds_alternative<LazyNode>(value)) return const_cast<json*>(this)->lazy_peek() == '{';
             return std::holds_alternative<object_t>(value);
        }
        bool is_array() const {
             if (std::holds_alternative<LazyNode>(value)) return const_cast<json*>(this)->lazy_peek() == '[';
             return std::holds_alternative<array_t>(value);
        }
        bool is_discarded() const { return false; }

        // Accessors
        template<typename T> T get() const {
            const_cast<json*>(this)->materialize();
            if constexpr(std::is_same_v<T, int>) return (int)get_int();
            if constexpr(std::is_same_v<T, int64_t>) return get_int();
            if constexpr(std::is_same_v<T, std::string>) return get_str();
            if constexpr(std::is_same_v<T, double>) return get_double();
            if constexpr(std::is_same_v<T, bool>) {
                if(std::holds_alternative<boolean_t>(value)) return std::get<boolean_t>(value);
                return false;
            }
            return T();
        }

        template<typename T> void get_to(T& t) const { t = get<T>(); }

        json& operator[](const std::string& key) {
            materialize();
            if (is_null()) value = object_t{};
            if (!is_object()) throw std::domain_error("cannot use operator[] with a non-object type");
            return std::get<object_t>(value)[key];
        }
        json& operator[](const char* key) { return (*this)[std::string(key)]; }
        json& operator[](int idx) { return (*this)[static_cast<size_t>(idx)]; }

        json& operator[](size_t idx) {
            materialize();
            if (is_null()) value = array_t{};
            if (!is_array()) throw std::domain_error("cannot use operator[] with a non-array type");
            array_t& arr = std::get<array_t>(value);
            if (idx >= arr.size()) arr.resize(idx + 1);
            return arr[idx];
        }

        const json& operator[](int idx) const { return (*this)[static_cast<size_t>(idx)]; }
        const json& operator[](size_t idx) const {
             const_cast<json*>(this)->materialize();
             if (is_array()) return std::get<array_t>(value).at(idx);
             throw std::out_of_range("Index out of range or not an array");
        }
        const json& operator[](const std::string& key) const {
             const_cast<json*>(this)->materialize();
             const auto& obj = std::get<object_t>(value);
             auto it = obj.find(key);
             if (it == obj.end()) throw std::out_of_range("key not found");
             return it->second;
        }

        size_t size() const {
             if (is_array() || is_object()) return lazy_size();
             return 0;
        }
        bool empty() const { return size() == 0; }
        bool contains(const std::string& key) const {
            const_cast<json*>(this)->materialize();
            if (!is_object()) return false;
            return std::get<object_t>(value).count(key);
        }

        std::string dump(int indent = -1, char indent_char = ' ', bool ensure_ascii = false) const {
            if (std::holds_alternative<LazyNode>(value) && indent == -1) {
                const LazyNode& l = std::get<LazyNode>(value);
                if (l.length > 0) {
                     return std::string(l.base_ptr + l.offset, l.length);
                }
            }

            const_cast<json*>(this)->materialize();
            if (is_null()) return "null";
            if (is_boolean()) return std::get<boolean_t>(value) ? "true" : "false";
            if (std::holds_alternative<number_integer_t>(value)) return std::to_string(std::get<number_integer_t>(value));
            if (std::holds_alternative<number_float_t>(value)) return std::to_string(std::get<number_float_t>(value));
            if (is_string()) return "\"" + std::get<string_t>(value) + "\"";

            if (is_array()) {
                const auto& arr = std::get<array_t>(value);
                if (arr.empty()) return "[]";
                std::string s = "[";
                if (indent >= 0) s += "\n";
                for (size_t i = 0; i < arr.size(); ++i) {
                     if (indent >= 0) s += std::string((i + 1) * indent, indent_char);
                     s += arr[i].dump(indent, indent_char, ensure_ascii);
                     if (i < arr.size() - 1) s += ",";
                     if (indent >= 0) s += "\n";
                }
                s += "]";
                return s;
            }
            if (is_object()) {
                 const auto& obj = std::get<object_t>(value);
                 if (obj.empty()) return "{}";
                 std::string s = "{";
                 bool first = true;
                 for (const auto& [k, v] : obj) {
                     if (!first) s += ",";
                     first = false;
                     s += "\"" + k + "\": " + v.dump(indent, indent_char, ensure_ascii);
                 }
                 s += "}";
                 return s;
            }
            return "";
        }

        // Iterators
        auto begin() { materialize();
            if (is_array()) return std::get<array_t>(value).begin();
             throw std::runtime_error("Iterator mismatch");
        }
        auto end() { materialize();
            if (is_array()) return std::get<array_t>(value).end();
            throw std::runtime_error("Iterator mismatch");
        }

        struct items_proxy {
             const json& j;
             auto begin() { return std::get<object_t>(j.value).begin(); }
             auto end() { return std::get<object_t>(j.value).end(); }
        };

        items_proxy items() const {
             const_cast<json*>(this)->materialize();
             if (!is_object()) throw std::runtime_error("items() called on non-object");
             return items_proxy{*this};
        }

    private:
        char lazy_peek() {
             LazyNode& l = std::get<LazyNode>(value);
             const char* s = asm_utils::skip_whitespace(l.base_ptr + l.offset, l.base_ptr + l.doc->len);
             return *s;
        }
        int64_t get_int() const {
            if (std::holds_alternative<number_integer_t>(value)) return std::get<number_integer_t>(value);
            return 0;
        }
        double get_double() const {
            if (std::holds_alternative<number_float_t>(value)) return std::get<number_float_t>(value);
            if (std::holds_alternative<number_integer_t>(value)) return (double)std::get<number_integer_t>(value);
            return 0.0;
        }
        std::string get_str() const {
            if (std::holds_alternative<string_t>(value)) return std::get<string_t>(value);
            return "";
        }
    };

    inline std::ostream& operator<<(std::ostream& o, const json& j) {
        o << j.dump();
        return o;
    }
}

#endif // TACHYON_HPP
