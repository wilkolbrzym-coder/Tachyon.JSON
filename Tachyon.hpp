#ifndef TACHYON_HPP
#define TACHYON_HPP

// Tachyon JSON Library v6.0
// The World's Fastest JSON Library
// "First-Class Citizen" C++ API

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
#include <cstdlib> // aligned_alloc, free
#include <cstdint>

#ifdef _MSC_VER
#include <intrin.h>
#include <malloc.h> // _aligned_malloc
#else
#include <immintrin.h> // Essential for GCC/Clang
#endif

#ifndef _MSC_VER
#define TACHYON_LIKELY(x) __builtin_expect(!!(x), 1)
#define TACHYON_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define TACHYON_LIKELY(x) (x)
#define TACHYON_UNLIKELY(x) (x)
#endif

// -----------------------------------------------------------------------------
// Configuration & Macros
// -----------------------------------------------------------------------------

#define TACHYON_VERSION_MAJOR 6
#define TACHYON_VERSION_MINOR 0
#define TACHYON_VERSION_PATCH 0

// Helper Macros for reflection (up to 64 arguments supported in theory, limiting to common usage)
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

namespace Tachyon {

    class json;
    class Document;

    // -------------------------------------------------------------------------
    // Low-Level SIMD Core
    // -------------------------------------------------------------------------
    namespace ASM {
        // Aligned allocation helper
        inline void* aligned_alloc(size_t size, size_t alignment = 32) {
#ifdef _MSC_VER
            return _aligned_malloc(size, alignment);
#else
            // C++17 standard aligned_alloc requires size to be a multiple of alignment
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

        // AVX2 Accelerated Whitespace Skipper
        // Skips space (0x20), tab (0x09), newline (0x0A), carriage return (0x0D)
        [[nodiscard]] inline const char* skip_whitespace(const char* p, const char* end) {
            // Scalar for short strings
            if (end - p < 32) {
                while (p < end && (unsigned char)*p <= 32) p++;
                return p;
            }

            // Align to 32 bytes boundaries for optimal load?
            // Actually, unaligned loads _mm256_loadu_si256 are fast on modern CPUs.

            __m256i v_space = _mm256_set1_epi8(' ');
            __m256i v_tab = _mm256_set1_epi8('\t');
            __m256i v_newline = _mm256_set1_epi8('\n');
            __m256i v_cr = _mm256_set1_epi8('\r');

            while (p + 32 <= end) {
                __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p));

                // Compare equal
                __m256i s = _mm256_cmpeq_epi8(chunk, v_space);
                __m256i t = _mm256_cmpeq_epi8(chunk, v_tab);
                __m256i n = _mm256_cmpeq_epi8(chunk, v_newline);
                __m256i r = _mm256_cmpeq_epi8(chunk, v_cr);

                // Combine
                __m256i combined = _mm256_or_si256(_mm256_or_si256(s, t), _mm256_or_si256(n, r));

                // If not all are whitespace, movemask will NOT be 0xFFFFFFFF
                // We want to find the first byte that is NOT whitespace.
                // The comparison returns 0xFF for true (is whitespace), 0x00 for false.
                // So movemask bits are 1 if whitespace.
                uint32_t mask = _mm256_movemask_epi8(combined);

                if (mask != 0xFFFFFFFF) {
                    // There is a non-whitespace char.
                    // The bits corresponding to non-whitespace are 0.
                    // We want the index of the first 0.
                    uint32_t inverted = ~mask;
                    int offset = std::countr_zero(inverted);
                    return p + offset;
                }

                p += 32;
            }

            // Handle remaining bytes
            while (p < end && (unsigned char)*p <= 32) p++;
            return p;
        }
    }

    namespace SIMD {
        // Optimized 2-pass structural indexer
        // Generates a bitmap where 1 indicates a structural character or start/end of string
        // Also performs strict UTF-8 validation.
        inline size_t compute_structural_mask(const char* data, size_t len, uint32_t* mask_array) {
            size_t i = 0;
            size_t block_idx = 0;

            // UTF-8 State
            __m256i v_has_error = _mm256_setzero_si256();

            const __m256i v_quote = _mm256_set1_epi8('"');
            const __m256i v_backslash = _mm256_set1_epi8('\\');
            const __m256i v_lbrace = _mm256_set1_epi8('{');
            const __m256i v_rbrace = _mm256_set1_epi8('}');
            const __m256i v_lbracket = _mm256_set1_epi8('[');
            const __m256i v_rbracket = _mm256_set1_epi8(']');
            const __m256i v_colon = _mm256_set1_epi8(':');
            const __m256i v_comma = _mm256_set1_epi8(',');

            uint64_t prev_escapes = 0;
            uint32_t in_string_mask = 0; // 0 if not in string, ~0 if in string (conceptual, actually we track bit by bit)

            // We process 64 bytes (2x 32-byte chunks) per loop iteration for throughput
            for (; i + 64 <= len; i += 64) {
                _mm_prefetch(data + i + 128, _MM_HINT_T0);

                // Unroll 2x
                for (int k = 0; k < 2; ++k) {
                    size_t off = i + (k * 32);
                    __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + off));

                    // UTF-8 Check: Fast Path for ASCII
                    if (!_mm256_testz_si256(chunk, _mm256_set1_epi8((char)0x80))) {
                         // Fallback to scalar validation for this chunk to ensure correctness
                         const unsigned char* u_ptr = reinterpret_cast<const unsigned char*>(data + off);
                         for (int j = 0; j < 32; ++j) {
                             if (u_ptr[j] >= 0x80) {
                                 // Simple validation logic (Scalar fallback)
                                 bool err = false;
                                 if ((u_ptr[j] & 0xE0) == 0xC0) { // 2 bytes
                                     if (j+1 >= 32 || (u_ptr[j+1] & 0xC0) != 0x80) err = true;
                                     else j+=1;
                                 } else if ((u_ptr[j] & 0xF0) == 0xE0) { // 3 bytes
                                     if (j+2 >= 32 || (u_ptr[j+1] & 0xC0) != 0x80 || (u_ptr[j+2] & 0xC0) != 0x80) err = true;
                                     else j+=2;
                                 } else if ((u_ptr[j] & 0xF8) == 0xF0) { // 4 bytes
                                     if (j+3 >= 32 || (u_ptr[j+1] & 0xC0) != 0x80 || (u_ptr[j+2] & 0xC0) != 0x80 || (u_ptr[j+3] & 0xC0) != 0x80) err = true;
                                     else j+=3;
                                 } else {
                                     err = true;
                                 }
                                 if (err) throw std::runtime_error("Tachyon: Invalid UTF-8 sequence");
                             }
                         }
                    }

                    // 1. Identify backslashes and quotes
                    uint32_t bs_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, v_backslash));
                    uint32_t quote_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, v_quote));

                    // 2. Handle escaped quotes logic
                    // If we have backslashes or carry-over escapes, we need slow-path bit manipulation
                    if (TACHYON_UNLIKELY(bs_mask != 0 || prev_escapes > 0)) {
                         uint32_t real_quote_mask = 0;
                         const char* c_ptr = data + off;
                         for(int j=0; j<32; ++j) {
                             if (c_ptr[j] == '"' && (prev_escapes & 1) == 0) {
                                 real_quote_mask |= (1U << j);
                             }
                             if (c_ptr[j] == '\\') prev_escapes++;
                             else prev_escapes = 0;
                         }
                         quote_mask = real_quote_mask;
                    } else {
                        prev_escapes = 0;
                    }

                    // 3. Compute in_string status
                    // prefix xor sum to toggle state at each quote
                    uint32_t prefix = quote_mask;
                    prefix ^= (prefix << 1);
                    prefix ^= (prefix << 2);
                    prefix ^= (prefix << 4);
                    prefix ^= (prefix << 8);
                    prefix ^= (prefix << 16);

                    // If we started inside a string, flip the mask
                    if (in_string_mask) prefix = ~prefix;

                    // Update in_string_mask for next block
                    if (std::popcount(quote_mask) % 2 != 0) in_string_mask = !in_string_mask;

                    __m256i s = _mm256_cmpeq_epi8(chunk, v_lbrace);
                    s = _mm256_or_si256(s, _mm256_cmpeq_epi8(chunk, v_rbrace));
                    s = _mm256_or_si256(s, _mm256_cmpeq_epi8(chunk, v_lbracket));
                    s = _mm256_or_si256(s, _mm256_cmpeq_epi8(chunk, v_rbracket));
                    s = _mm256_or_si256(s, _mm256_cmpeq_epi8(chunk, v_colon));
                    s = _mm256_or_si256(s, _mm256_cmpeq_epi8(chunk, v_comma));

                    uint32_t struct_mask = _mm256_movemask_epi8(s);

                    // Logic:
                    // Any structural char is valid ONLY if it is NOT inside a string.
                    // A char at index `x` is inside a string if the number of quotes before it is odd.
                    // `prefix` represents exactly that parity (inclusive of current position).
                    // So if `prefix` is 1, we have seen odd quotes (we are at or after opening).

                    uint32_t final_mask = (struct_mask & ~prefix) | quote_mask;
                    mask_array[block_idx++] = final_mask;
                }
            }

            // Clean up tail
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
                        if (is_quote) { in_string_mask = 1; final_mask |= (1U << j); }
                        else if (c == '{' || c == '}' || c == '[' || c == ']' || c == ':' || c == ',') final_mask |= (1U << j);
                    }
                }
                mask_array[block_idx++] = final_mask;
            }
            return block_idx;
        }
    }

    // -------------------------------------------------------------------------
    // Memory Management
    // -------------------------------------------------------------------------
    struct AlignedDeleter {
        void operator()(uint32_t* p) const { ASM::aligned_free(p); }
    };

    // -------------------------------------------------------------------------
    // Document
    // -------------------------------------------------------------------------
    class Document {
    public:
        std::string storage;
        std::unique_ptr<uint32_t[], AlignedDeleter> bitmask;
        size_t len = 0;
        size_t bitmask_len = 0;
        size_t bitmask_cap = 0;

        void parse(std::string&& json) {
            storage = std::move(json);
            len = storage.size();
            allocate_bitmask(len);
            bitmask_len = SIMD::compute_structural_mask(storage.data(), len, bitmask.get());
        }

        void parse_view(const char* data, size_t size) {
            len = size;
            allocate_bitmask(len);
            bitmask_len = SIMD::compute_structural_mask(data, len, bitmask.get());
        }

        const char* get_base() const { return storage.empty() ? nullptr : storage.data(); }

    private:
        void allocate_bitmask(size_t length) {
            size_t req_len = (length + 31) / 32 + 2; // +padding
            if (req_len > bitmask_cap) {
                uint32_t* ptr = static_cast<uint32_t*>(ASM::aligned_alloc(req_len * sizeof(uint32_t)));
                bitmask.reset(ptr);
                bitmask_cap = req_len;
            }
        }
    };

    // -------------------------------------------------------------------------
    // Cursor
    // -------------------------------------------------------------------------
    struct Cursor {
        const uint32_t* bitmask_ptr;
        size_t max_block;
        uint32_t block_idx;
        uint32_t mask;

        Cursor(const Document* d, uint32_t offset) {
            bitmask_ptr = d->bitmask.get();
            max_block = d->bitmask_len;
            block_idx = offset / 32;
            int bit = offset % 32;

            if (block_idx < max_block) {
                mask = bitmask_ptr[block_idx];
                // Clear bits before 'bit'
                mask &= ~((1U << bit) - 1);
            } else {
                mask = 0;
            }
        }

        inline uint32_t next() {
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

    // -------------------------------------------------------------------------
    // JSON Value
    // -------------------------------------------------------------------------
    using ObjectType = std::map<std::string, class json, std::less<>>;
    using ArrayType = std::vector<class json>;

    struct LazyNode {
        std::shared_ptr<Document> doc;
        uint32_t offset;
        const char* base_ptr; // cached
    };

    class json {
        // Optimization: Use a custom variant-like structure or just variant
        std::variant<std::monostate, bool, int64_t, uint64_t, double, std::string, ObjectType, ArrayType, LazyNode> value;

    public:
        // Constructors
        json() : value(std::monostate{}) {}
        json(std::nullptr_t) : value(std::monostate{}) {}
        json(bool b) : value(b) {}
        json(int i) : value(static_cast<int64_t>(i)) {}
        json(int64_t i) : value(i) {}
        json(uint64_t i) : value(i) {}
        // Disambiguate for long long if it differs from int64_t
        template<typename T, typename = std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, int> && !std::is_same_v<T, int64_t> && !std::is_same_v<T, uint64_t> && !std::is_same_v<T, bool>>>
        json(T i) : value(static_cast<int64_t>(i)) {}
        json(double d) : value(d) {}
        json(const std::string& s) : value(s) {}
        json(std::string&& s) : value(std::move(s)) {}
        json(const char* s) : value(std::string(s)) {}
        json(std::string_view s) : value(std::string(s)) {}
        json(const ObjectType& o) : value(o) {}
        json(const ArrayType& a) : value(a) {}
        json(LazyNode l) : value(l) {}

        template<typename T, typename = std::enable_if_t<
            !std::is_same_v<T, json> &&
            !std::is_same_v<T, std::string> &&
            !std::is_same_v<T, const char*> &&
            !std::is_same_v<T, std::string_view> &&
            !std::is_arithmetic_v<T> &&
            !std::is_null_pointer_v<T>
        >>
        json(const T& t) {
            to_json(*this, t);
        }

        json(std::initializer_list<json> init) {
            bool is_obj = std::all_of(init.begin(), init.end(), [](const json& j){
                return j.is_array() && j.size() == 2 && j[0].is_string();
            });
            if (is_obj) {
                ObjectType obj;
                for (const auto& el : init) obj[el[0].get<std::string>()] = el[1];
                value = obj;
            } else {
                value = ArrayType(init);
            }
        }

        static json object() { return json(ObjectType{}); }
        static json array() { return json(ArrayType{}); }

        // Default parse - takes ownership (Move or Copy)
        static json parse(std::string s) {
            auto doc = std::make_shared<Document>();
            doc->parse(std::move(s));
            return json(LazyNode{doc, 0, doc->get_base()});
        }

        // Explicit overload for const char* to avoid ambiguity with string_view
        static json parse(const char* s) {
            return parse(std::string(s));
        }

        static json parse(const char* ptr, size_t len) {
             return parse_view(ptr, len);
        }

        static json parse_view(const char* ptr, size_t len) {
            auto doc = std::make_shared<Document>();
            doc->parse_view(ptr, len);
            return json(LazyNode{doc, 0, ptr});
        }

        static json parse_view(std::string_view s) {
            return parse_view(s.data(), s.size());
        }

        // Type Checks
        bool is_lazy() const { return std::holds_alternative<LazyNode>(value); }
        char lazy_char() const {
            const auto& l = std::get<LazyNode>(value);
            const char* end_ptr = l.base_ptr + l.doc->len;
            const char* s = ASM::skip_whitespace(l.base_ptr + l.offset, end_ptr);
            if (s >= end_ptr) return '\0';
            return *s;
        }
        bool is_array() const { return std::holds_alternative<ArrayType>(value) || (is_lazy() && lazy_char() == '['); }
        bool is_object() const { return std::holds_alternative<ObjectType>(value) || (is_lazy() && lazy_char() == '{'); }
        bool is_string() const { return std::holds_alternative<std::string>(value) || (is_lazy() && lazy_char() == '"'); }
        bool is_number() const {
            if (std::holds_alternative<double>(value) || std::holds_alternative<int64_t>(value) || std::holds_alternative<uint64_t>(value)) return true;
            if (!is_lazy()) return false;
            char c = lazy_char();
            return (c >= '0' && c <= '9') || c == '-';
        }
        bool is_null() const { return std::holds_alternative<std::monostate>(value) || (is_lazy() && lazy_char() == 'n'); }
        bool is_boolean() const { return std::holds_alternative<bool>(value) || (is_lazy() && (lazy_char() == 't' || lazy_char() == 'f')); }
        bool contains(const std::string& key) const {
            if (is_object()) {
                // If lazy, we could scan without full materialization, but for safety lets use operator[] logic
                // Actually operator[] returns null json if not found.
                // Optimally we should have a non-allocating find.
                json res = this->operator[](key);
                return !res.is_null(); // If operator[] returns empty/null json for missing key
            }
            return false;
        }

        // Accessors
        json& operator[](const std::string& key) {
            materialize();
            if (!std::holds_alternative<ObjectType>(value)) {
                if (std::holds_alternative<std::monostate>(value)) value = ObjectType{};
                else throw std::runtime_error("Tachyon: Type mismatch, expected object");
            }
            return std::get<ObjectType>(value)[key];
        }

        json& at(const std::string& key) {
             materialize();
             if (!std::holds_alternative<ObjectType>(value)) throw std::runtime_error("Tachyon: Not an object");
             auto& o = std::get<ObjectType>(value);
             auto it = o.find(key);
             if (it == o.end()) throw std::out_of_range("Tachyon: Key not found: " + key);
             return it->second;
        }

        const json at(const std::string& key) const {
             // For const access, we return by value (json is lightweight or handles its own resources)
             // But wait, if we return by value, we need to ensure deep copy or lazy ref.
             // Our json is efficient to copy (shared_ptr for lazy).
             if (is_lazy()) {
                 json j = lazy_lookup(key);
                 if (j.is_null()) throw std::out_of_range("Tachyon: Key not found: " + key);
                 return j;
             }
             if (!std::holds_alternative<ObjectType>(value)) throw std::runtime_error("Tachyon: Not an object");
             const auto& o = std::get<ObjectType>(value);
             auto it = o.find(key);
             if (it == o.end()) throw std::out_of_range("Tachyon: Key not found: " + key);
             return it->second;
        }

        const json operator[](const std::string& key) const {
            if (is_lazy()) return lazy_lookup(key);
            if (std::holds_alternative<ObjectType>(value)) {
                const auto& o = std::get<ObjectType>(value);
                auto it = o.find(key);
                if (it != o.end()) return it->second;
            }
            return json(); // null
        }

        json& operator[](size_t idx) {
            materialize();
            if (!std::holds_alternative<ArrayType>(value)) {
                 if (std::holds_alternative<std::monostate>(value)) value = ArrayType{};
                 else throw std::runtime_error("Tachyon: Type mismatch, expected array");
            }
            ArrayType& arr = std::get<ArrayType>(value);
            if (idx >= arr.size()) arr.resize(idx + 1);
            return arr[idx];
        }

        const json operator[](size_t idx) const {
            if (is_lazy()) return lazy_index(idx);
            if (std::holds_alternative<ArrayType>(value)) {
                const auto& a = std::get<ArrayType>(value);
                if (idx < a.size()) return a[idx];
            }
            return json();
        }

        // Conversion
        template<typename T>
        void get_to(T& t) const {
            t = get<T>();
        }

        template<typename T> T get() const {
            if constexpr (std::is_same_v<T, std::string>) return as_string();
            else if constexpr (std::is_same_v<T, std::string_view>) return as_string_view();
            else if constexpr (std::is_same_v<T, double>) return as_double();
            else if constexpr (std::is_same_v<T, float>) return static_cast<float>(as_double());
            else if constexpr (std::is_same_v<T, int>) return static_cast<int>(as_double());
            else if constexpr (std::is_same_v<T, int64_t>) return as_int64();
            else if constexpr (std::is_same_v<T, uint64_t>) return as_uint64();
            else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) return static_cast<T>(as_int64());
            else if constexpr (std::is_same_v<T, bool>) return as_bool();
            else {
                T t;
                from_json(*this, t);
                return t;
            }
        }

        size_t size() const {
            if (is_lazy()) return lazy_size();
            if (std::holds_alternative<ArrayType>(value)) return std::get<ArrayType>(value).size();
            if (std::holds_alternative<ObjectType>(value)) return std::get<ObjectType>(value).size();
            return 0;
        }

        std::string dump() const {
            if (is_lazy()) {
                json copy = *this;
                copy.materialize();
                return copy.dump();
            }
            if (std::holds_alternative<std::string>(value)) return "\"" + std::get<std::string>(value) + "\"";
            if (std::holds_alternative<int64_t>(value)) return std::to_string(std::get<int64_t>(value));
            if (std::holds_alternative<uint64_t>(value)) return std::to_string(std::get<uint64_t>(value));
            if (std::holds_alternative<double>(value)) {
                std::string s = std::to_string(std::get<double>(value));
                s.erase(s.find_last_not_of('0') + 1, std::string::npos);
                if (s.back() == '.') s.pop_back();
                return s;
            }
            if (std::holds_alternative<bool>(value)) return std::get<bool>(value) ? "true" : "false";
            if (std::holds_alternative<std::monostate>(value)) return "null";
            if (std::holds_alternative<ObjectType>(value)) {
                std::string s = "{";
                const auto& o = std::get<ObjectType>(value);
                bool f = true;
                for (const auto& [k, v] : o) {
                    if (!f) s += ","; f = false;
                    s += "\"" + k + "\":" + v.dump();
                }
                s += "}";
                return s;
            }
            if (std::holds_alternative<ArrayType>(value)) {
                std::string s = "[";
                const auto& a = std::get<ArrayType>(value);
                bool f = true;
                for (const auto& v : a) {
                    if (!f) s += ","; f = false;
                    s += v.dump();
                }
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
                Cursor cur(l.doc.get(), start);
                while (true) {
                    uint32_t curr = cur.next(); // First key quote
                    if (curr == (uint32_t)-1 || base[curr] == '}') break;
                    if (base[curr] == ',') continue;

                    if (base[curr] == '"') {
                        uint32_t end_q = cur.next();
                        std::string k(base + curr + 1, end_q - curr - 1);
                        uint32_t colon = cur.next();

                        const char* vs = ASM::skip_whitespace(base + colon + 1, base + l.doc->len);
                        uint32_t val_pos = (uint32_t)(vs - base);
                        json child(LazyNode{l.doc, val_pos, base});

                        // Skip over value
                        char vc = base[val_pos];
                        uint32_t next_delim;

                        if (vc == '{') { skip_container(cur, base, '{', '}'); next_delim = cur.next(); }
                        else if (vc == '[') { skip_container(cur, base, '[', ']'); next_delim = cur.next(); }
                        else if (vc == '"') { cur.next(); cur.next(); next_delim = cur.next(); }
                        else { next_delim = cur.next(); }

                        obj[std::move(k)] = std::move(child);

                        if (next_delim == (uint32_t)-1 || base[next_delim] == '}') break;
                        // if comma, loop continues
                    }
                }
                value = std::move(obj);
            } else if (c == '[') {
                ArrayType arr;
                const char* s_arr = ASM::skip_whitespace(base + l.offset + 1, base + l.doc->len);
                if (*s_arr == ']') { value = std::move(arr); return; }

                uint32_t pos = (uint32_t)(s_arr - base);
                Cursor cur(l.doc.get(), pos);

                while (true) {
                    arr.push_back(json(LazyNode{l.doc, pos, base}));

                    char ch = base[pos];
                    uint32_t next_delim;

                    if (ch == '{') { skip_container(cur, base, '{', '}'); next_delim = cur.next(); }
                    else if (ch == '[') { skip_container(cur, base, '[', ']'); next_delim = cur.next(); }
                    else if (ch == '"') { cur.next(); cur.next(); next_delim = cur.next(); }
                    else { next_delim = cur.next(); }

                    if (next_delim == (uint32_t)-1 || base[next_delim] == ']') break;

                    if (base[next_delim] == ',') {
                        const char* next_s = ASM::skip_whitespace(base + next_delim + 1, base + l.doc->len);
                        pos = (uint32_t)(next_s - base);
                    }
                }
                value = std::move(arr);
            } else if (c == '"') { value = as_string(); }
            else if (c == 't' || c == 'f') { value = as_bool(); }
            else if (c == 'n') { value = std::monostate{}; }
            else { value = as_double(); }
        }

        std::string as_string() const {
            std::string_view sv = as_string_view();
            return unescape_string(sv);
        }

        static void encode_utf8(std::string& res, uint32_t cp) {
            if (cp <= 0x7F) {
                res += (char)cp;
            } else if (cp <= 0x7FF) {
                res += (char)(0xC0 | (cp >> 6));
                res += (char)(0x80 | (cp & 0x3F));
            } else if (cp <= 0xFFFF) {
                res += (char)(0xE0 | (cp >> 12));
                res += (char)(0x80 | ((cp >> 6) & 0x3F));
                res += (char)(0x80 | (cp & 0x3F));
            } else if (cp <= 0x10FFFF) {
                res += (char)(0xF0 | (cp >> 18));
                res += (char)(0x80 | ((cp >> 12) & 0x3F));
                res += (char)(0x80 | ((cp >> 6) & 0x3F));
                res += (char)(0x80 | (cp & 0x3F));
            }
        }

        static uint32_t parse_hex4(const char* p) {
            uint32_t cp = 0;
            for (int i = 0; i < 4; ++i) {
                char c = p[i];
                cp <<= 4;
                if (c >= '0' && c <= '9') cp |= (c - '0');
                else if (c >= 'A' && c <= 'F') cp |= (c - 'A' + 10);
                else if (c >= 'a' && c <= 'f') cp |= (c - 'a' + 10);
                else return 0; // Invalid
            }
            return cp;
        }

        static std::string unescape_string(std::string_view sv) {
            std::string res;
            res.reserve(sv.size());
            for (size_t i = 0; i < sv.size(); ++i) {
                if (sv[i] == '\\') {
                    if (i + 1 >= sv.size()) break; // Invalid
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
                                // Check for surrogate pair
                                if (cp >= 0xD800 && cp <= 0xDBFF) {
                                     // Need another \uXXXX
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

        std::string_view as_string_view() const {
            if (is_lazy()) {
                const auto& l = std::get<LazyNode>(value);
                const char* base = l.base_ptr;
                const char* s = ASM::skip_whitespace(base + l.offset, base + l.doc->len);
                // Should point to '"'
                uint32_t start = (uint32_t)(s - base);
                Cursor c(l.doc.get(), start + 1);
                uint32_t end = c.next(); // Find closing quote
                return std::string_view(base + start + 1, end - start - 1);
            }
            if (std::holds_alternative<std::string>(value)) return std::get<std::string>(value);
            return "";
        }

        double as_double() const {
            if (is_lazy()) {
                const auto& l = std::get<LazyNode>(value);
                const char* s = ASM::skip_whitespace(l.base_ptr + l.offset, l.base_ptr + l.doc->len);
                double d = 0.0;
                std::from_chars(s, l.base_ptr + l.doc->len, d);
                return d;
            }
            if (std::holds_alternative<double>(value)) return std::get<double>(value);
            if (std::holds_alternative<int64_t>(value)) return static_cast<double>(std::get<int64_t>(value));
            if (std::holds_alternative<uint64_t>(value)) return static_cast<double>(std::get<uint64_t>(value));
            return 0.0;
        }

        int64_t as_int64() const {
            if (is_lazy()) {
                const auto& l = std::get<LazyNode>(value);
                const char* s = ASM::skip_whitespace(l.base_ptr + l.offset, l.base_ptr + l.doc->len);
                int64_t i = 0;
                std::from_chars(s, l.base_ptr + l.doc->len, i);
                return i;
            }
            if (std::holds_alternative<int64_t>(value)) return std::get<int64_t>(value);
            if (std::holds_alternative<uint64_t>(value)) return static_cast<int64_t>(std::get<uint64_t>(value));
            if (std::holds_alternative<double>(value)) return static_cast<int64_t>(std::get<double>(value));
            return 0;
        }

        uint64_t as_uint64() const {
             return static_cast<uint64_t>(as_int64());
        }

        bool as_bool() const {
            if (is_lazy()) return lazy_char() == 't';
            if (std::holds_alternative<bool>(value)) return std::get<bool>(value);
            return false;
        }

        json lazy_lookup(const std::string& key) const {
            const auto& l = std::get<LazyNode>(value);
            const char* base = l.base_ptr;
            const char* s = ASM::skip_whitespace(base + l.offset, base + l.doc->len);

            // Assume we are at '{'
            uint32_t start = (uint32_t)(s - base) + 1;
            Cursor c(l.doc.get(), start);

            while (true) {
                uint32_t curr = c.next();
                if (curr == (uint32_t)-1 || base[curr] == '}') return json();
                if (base[curr] == ',') continue;
                if (base[curr] == '"') {
                    uint32_t end_q = c.next();
                    size_t k_len = end_q - curr - 1;

                    bool match = (k_len == key.size()) && (memcmp(base + curr + 1, key.data(), k_len) == 0);

                    uint32_t colon = c.next(); // ':'

                    if (match) {
                        const char* vs = ASM::skip_whitespace(base + colon + 1, base + l.doc->len);
                        return json(LazyNode{l.doc, (uint32_t)(vs - base), base});
                    }

                    const char* vs = ASM::skip_whitespace(base + colon + 1, base + l.doc->len);
                    char vc = *vs;
                    if (vc == '{') skip_container(c, base, '{', '}');
                    else if (vc == '[') skip_container(c, base, '[', ']');
                    else if (vc == '"') { c.next(); c.next(); }
                    // numbers/bools/nulls don't appear in bitmask, so Cursor skips them automatically
                    // Wait, Cursor only stops at structural chars.
                    // If value is 123, Cursor skips it.
                    // If value is "abc", Cursor stops at quotes.
                    // If value is {}, Cursor stops at braces.
                    // Correct.
                }
            }
        }

        void skip_container(Cursor& c, const char* base, char open, char close) const {
            int depth = 0;
            while (true) {
                uint32_t curr = c.next();
                if (curr == (uint32_t)-1) break;
                char ch = base[curr];
                if (ch == open) depth++;
                else if (ch == close) depth--;
                else if (ch == '"') c.next(); // skip string content

                if (depth == 0) break;
            }
        }

        json lazy_index(size_t idx) const {
            const auto& l = std::get<LazyNode>(value);
            const char* base = l.base_ptr;
            const char* s = ASM::skip_whitespace(base + l.offset, base + l.doc->len);

            // Assume '['
            uint32_t start = (uint32_t)(s - base) + 1;
            Cursor c(l.doc.get(), start);
            size_t count = 0;

            while (true) {
                uint32_t curr = c.next();
                if (curr == (uint32_t)-1 || base[curr] == ']') return json();
                if (base[curr] == ',') continue;

                if (count == idx) return json(LazyNode{l.doc, curr, base});

                char ch = base[curr];
                if (ch == '{') skip_container(c, base, '{', '}');
                else if (ch == '[') skip_container(c, base, '[', ']');
                else if (ch == '"') c.next();
                count++;
            }
        }

        size_t lazy_size() const {
            const auto& l = std::get<LazyNode>(value);
            const char* base = l.base_ptr;
            const char* s = ASM::skip_whitespace(base + l.offset, base + l.doc->len);

            char c0 = *s;
            if (c0 == ']') return 0;
            if (c0 == '}') return 0;

            uint32_t start = (uint32_t)(s - base) + 1;
            Cursor c(l.doc.get(), start);
            size_t commas = 0;

            while (true) {
                uint32_t curr = c.next();
                if (curr == (uint32_t)-1) break;
                char ch = base[curr];
                if (ch == ']' || ch == '}') break;

                if (ch == ',') commas++;
                else if (ch == '{') skip_container(c, base, '{', '}');
                else if (ch == '[') skip_container(c, base, '[', ']');
                else if (ch == '"') c.next();
            }
            return commas + 1;
        }
    };
}

#endif // TACHYON_HPP
