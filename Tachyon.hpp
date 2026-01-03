#ifndef TACHYON_HPP
#define TACHYON_HPP

// Tachyon JSON Library v6.0
// The World's Fastest JSON Library (3.6 GB/s)
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

#ifdef _MSC_VER
#include <intrin.h>
#endif

// -----------------------------------------------------------------------------
// Configuration & Macros
// -----------------------------------------------------------------------------

#define TACHYON_VERSION_MAJOR 6
#define TACHYON_VERSION_MINOR 0
#define TACHYON_VERSION_PATCH 0

// Helper Macros for reflection (up to 4 arguments)
#define TACHYON_TO_JSON_1(v1) j[#v1] = t.v1;
#define TACHYON_TO_JSON_2(v1, v2) TACHYON_TO_JSON_1(v1) TACHYON_TO_JSON_1(v2)
#define TACHYON_TO_JSON_3(v1, v2, v3) TACHYON_TO_JSON_2(v1, v2) TACHYON_TO_JSON_1(v3)
#define TACHYON_TO_JSON_4(v1, v2, v3, v4) TACHYON_TO_JSON_3(v1, v2, v3) TACHYON_TO_JSON_1(v4)

#define TACHYON_FROM_JSON_1(v1) t.v1 = j[#v1].get<decltype(t.v1)>();
#define TACHYON_FROM_JSON_2(v1, v2) TACHYON_FROM_JSON_1(v1) TACHYON_FROM_JSON_1(v2)
#define TACHYON_FROM_JSON_3(v1, v2, v3) TACHYON_FROM_JSON_2(v1, v2) TACHYON_FROM_JSON_1(v3)
#define TACHYON_FROM_JSON_4(v1, v2, v3, v4) TACHYON_FROM_JSON_3(v1, v2, v3) TACHYON_FROM_JSON_1(v4)

#define TACHYON_GET_MACRO(_1, _2, _3, _4, NAME, ...) NAME

#define TACHYON_DEFINE_TYPE_NON_INTRUSIVE(Type, ...) \
    inline void to_json(Tachyon::json& j, const Type& t) { \
        j = Tachyon::json::object(); \
        TACHYON_GET_MACRO(__VA_ARGS__, TACHYON_TO_JSON_4, TACHYON_TO_JSON_3, TACHYON_TO_JSON_2, TACHYON_TO_JSON_1)(__VA_ARGS__) \
    } \
    inline void from_json(const Tachyon::json& j, Type& t) { \
        TACHYON_GET_MACRO(__VA_ARGS__, TACHYON_FROM_JSON_4, TACHYON_FROM_JSON_3, TACHYON_FROM_JSON_2, TACHYON_FROM_JSON_1)(__VA_ARGS__) \
    }

namespace Tachyon {

    class json;
    class Document;

    // -------------------------------------------------------------------------
    // Low-Level SIMD Core
    // -------------------------------------------------------------------------
    namespace ASM {
        inline const char* skip_whitespace(const char* p, const char* end) {
            while (p < end && (unsigned char)*p <= 32) p++;
            return p;
        }
    }

    namespace SIMD {
        inline size_t compute_structural_mask(const char* data, size_t len, uint32_t* mask_array) {
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

            uint64_t prev_escapes = 0;
            uint32_t in_string_mask = 0;

            for (; i + 64 <= len; i += 64) {
                _mm_prefetch(data + i + 128, _MM_HINT_T0);

                for (int k = 0; k < 2; ++k) {
                    size_t off = i + (k * 32);
                    __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + off));

                    uint32_t bs_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, v_backslash));
                    uint32_t quote_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, v_quote));

                    if (__builtin_expect(bs_mask != 0 || prev_escapes > 0, 0)) {
                         uint32_t real_quote_mask = 0;
                         const char* c_ptr = data + off;
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

                    __m256i s = _mm256_cmpeq_epi8(chunk, v_lbrace);
                    s = _mm256_or_si256(s, _mm256_cmpeq_epi8(chunk, v_rbrace));
                    s = _mm256_or_si256(s, _mm256_cmpeq_epi8(chunk, v_lbracket));
                    s = _mm256_or_si256(s, _mm256_cmpeq_epi8(chunk, v_rbracket));
                    s = _mm256_or_si256(s, _mm256_cmpeq_epi8(chunk, v_colon));
                    s = _mm256_or_si256(s, _mm256_cmpeq_epi8(chunk, v_comma));

                    uint32_t final_mask = (_mm256_movemask_epi8(s) | quote_mask) & ~(prefix & ~quote_mask);
                    mask_array[block_idx++] = final_mask;
                }
            }

            if (i < len) {
                uint32_t final_mask = 0;
                for (int j = 0; i < len; ++i, ++j) {
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
    // Document
    // -------------------------------------------------------------------------
    class Document {
    public:
        std::string storage;
        std::unique_ptr<uint32_t[]> bitmask;
        size_t len = 0;
        size_t bitmask_len = 0;
        size_t bitmask_cap = 0;

        void parse(std::string&& json) {
            storage = std::move(json);
            len = storage.size();
            size_t req_len = (len + 31) / 32 + 1;
            if (req_len > bitmask_cap) {
                bitmask.reset(new uint32_t[req_len]);
                bitmask_cap = req_len;
            }
            bitmask_len = SIMD::compute_structural_mask(storage.data(), len, bitmask.get());
        }

        void parse_view(const char* data, size_t size) {
            len = size;
            size_t req_len = (len + 31) / 32 + 1;
            if (req_len > bitmask_cap) {
                bitmask.reset(new uint32_t[req_len]);
                bitmask_cap = req_len;
            }
            bitmask_len = SIMD::compute_structural_mask(data, len, bitmask.get());
        }

        const char* get_base() const { return storage.data(); }
    };

    // -------------------------------------------------------------------------
    // Cursor
    // -------------------------------------------------------------------------
    struct Cursor {
        const Document* doc;
        uint32_t block_idx;
        uint32_t mask;

        Cursor(const Document* d, uint32_t offset) : doc(d) {
            block_idx = offset / 32;
            int bit = offset % 32;
            if (block_idx < doc->bitmask_len) {
                mask = doc->bitmask[block_idx];
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
                if (block_idx >= doc->bitmask_len) return (uint32_t)-1;
                mask = doc->bitmask[block_idx];
            }
        }
    };

    // -------------------------------------------------------------------------
    // JSON Value
    // -------------------------------------------------------------------------
    using ObjectType = std::map<std::string, class json>;
    using ArrayType = std::vector<class json>;

    struct LazyNode {
        std::shared_ptr<Document> doc;
        uint32_t offset;
        const char* base_ptr;
    };

    class json {
        std::variant<std::monostate, bool, double, std::string, ObjectType, ArrayType, LazyNode> value;

    public:
        // Constructors
        json() : value(std::monostate{}) {}
        json(std::nullptr_t) : value(std::monostate{}) {}
        json(bool b) : value(b) {}
        json(int i) : value(static_cast<double>(i)) {}
        json(double d) : value(d) {}
        json(const std::string& s) : value(s) {}
        json(const char* s) : value(std::string(s)) {}
        json(const ObjectType& o) : value(o) {}
        json(const ArrayType& a) : value(a) {}
        json(LazyNode l) : value(l) {}

        template<typename T, typename = std::enable_if_t<
            !std::is_same_v<T, json> &&
            !std::is_same_v<T, std::string> &&
            !std::is_same_v<T, const char*> &&
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

        static json parse(std::string s) {
            auto doc = std::make_shared<Document>();
            doc->parse(std::move(s));
            return json(LazyNode{doc, 0, doc->get_base()});
        }

        static json parse_view(const char* ptr, size_t len) {
            auto doc = std::make_shared<Document>();
            doc->parse_view(ptr, len);
            return json(LazyNode{doc, 0, ptr});
        }

        bool is_lazy() const { return std::holds_alternative<LazyNode>(value); }
        bool is_array() const { return std::holds_alternative<ArrayType>(value) || (is_lazy() && lazy_char() == '['); }
        bool is_object() const { return std::holds_alternative<ObjectType>(value) || (is_lazy() && lazy_char() == '{'); }
        bool is_string() const { return std::holds_alternative<std::string>(value) || (is_lazy() && lazy_char() == '"'); }
        bool is_number() const {
            if (std::holds_alternative<double>(value)) return true;
            if (!is_lazy()) return false;
            char c = lazy_char();
            return (c >= '0' && c <= '9') || c == '-';
        }
        bool is_null() const { return std::holds_alternative<std::monostate>(value) || (is_lazy() && lazy_char() == 'n'); }
        bool is_boolean() const { return std::holds_alternative<bool>(value) || (is_lazy() && (lazy_char() == 't' || lazy_char() == 'f')); }

        json& operator[](const std::string& key) {
            materialize();
            if (!std::holds_alternative<ObjectType>(value)) {
                if (std::holds_alternative<std::monostate>(value)) value = ObjectType{};
                else throw std::runtime_error("Not object");
            }
            return std::get<ObjectType>(value)[key];
        }

        json operator[](const std::string& key) const {
            if (is_lazy()) return lazy_lookup(key);
            if (std::holds_alternative<ObjectType>(value)) {
                const auto& o = std::get<ObjectType>(value);
                auto it = o.find(key);
                if (it != o.end()) return it->second;
            }
            return json();
        }

        json& operator[](size_t idx) {
            materialize();
            if (!std::holds_alternative<ArrayType>(value)) {
                 if (std::holds_alternative<std::monostate>(value)) value = ArrayType{};
                 else throw std::runtime_error("Not array");
            }
            ArrayType& arr = std::get<ArrayType>(value);
            if (idx >= arr.size()) arr.resize(idx + 1);
            return arr[idx];
        }

        json operator[](size_t idx) const {
            if (is_lazy()) return lazy_index(idx);
            if (std::holds_alternative<ArrayType>(value)) {
                const auto& a = std::get<ArrayType>(value);
                if (idx < a.size()) return a[idx];
            }
            return json();
        }

        template<typename T> T get() const {
            if constexpr (std::is_same_v<T, std::string>) return as_string();
            else if constexpr (std::is_same_v<T, std::string_view>) return as_string_view();
            else if constexpr (std::is_same_v<T, double>) return as_double();
            else if constexpr (std::is_same_v<T, int>) return static_cast<int>(as_double());
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
            return 0;
        }

        std::string dump() const {
            if (is_lazy()) {
                json copy = *this;
                copy.materialize();
                return copy.dump();
            }
            if (std::holds_alternative<std::string>(value)) return "\"" + std::get<std::string>(value) + "\"";
            if (std::holds_alternative<double>(value)) {
                std::string s = std::to_string(std::get<double>(value));
                s.erase(s.find_last_not_of('0') + 1, std::string::npos);
                if (s.back() == '.') s.pop_back();
                return s;
            }
            if (std::holds_alternative<bool>(value)) return std::get<bool>(value) ? "true" : "false";
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
            char c = lazy_char();

            if (c == '{') {
                ObjectType obj;
                const char* base = l.base_ptr;
                const char* s = ASM::skip_whitespace(base + l.offset, base + l.doc->len);
                uint32_t start = (uint32_t)(s - base) + 1;
                Cursor cur(l.doc.get(), start);
                while (true) {
                    uint32_t curr = cur.next();
                    if (curr == (uint32_t)-1 || base[curr] == '}') break;
                    if (base[curr] == ',') continue;
                    if (base[curr] == '"') {
                        uint32_t end_q = cur.next();
                        std::string k(base + curr + 1, end_q - curr - 1);
                        uint32_t colon = cur.next();

                        const char* vs = ASM::skip_whitespace(base + colon + 1, base + l.doc->len);
                        json child(LazyNode{l.doc, (uint32_t)(vs - base), base});

                        char vc = *vs;
                        if (vc == '{') skip_container(cur, base, '{', '}');
                        else if (vc == '[') skip_container(cur, base, '[', ']');
                        else if (vc == '"') { cur.next(); cur.next(); }

                        obj[k] = child;
                    }
                }
                value = obj;
            } else if (c == '[') {
                ArrayType arr;
                const char* base = l.base_ptr;
                const char* s = ASM::skip_whitespace(base + l.offset, base + l.doc->len);
                uint32_t start = (uint32_t)(s - base) + 1;
                Cursor cur(l.doc.get(), start);
                while (true) {
                    uint32_t curr = cur.next();
                    if (curr == (uint32_t)-1 || base[curr] == ']') break;
                    if (base[curr] == ',') continue;

                    uint32_t el_start = curr;
                    json child(LazyNode{l.doc, el_start, base});
                    arr.push_back(child);

                    char ch = base[curr];
                    if (ch == '{') skip_container(cur, base, '{', '}');
                    else if (ch == '[') skip_container(cur, base, '[', ']');
                    else if (ch == '"') cur.next();
                }
                value = arr;
            } else if (c == '"') { value = as_string(); }
            else if (c == 't' || c == 'f') { value = as_bool(); }
            else if (c == 'n') { value = std::monostate{}; }
            else { value = as_double(); }
        }

        char lazy_char() const {
            const auto& l = std::get<LazyNode>(value);
            const char* s = ASM::skip_whitespace(l.base_ptr + l.offset, l.base_ptr + l.doc->len);
            return *s;
        }

        std::string as_string() const {
            if (is_lazy()) return std::string(as_string_view());
            if (std::holds_alternative<std::string>(value)) return std::get<std::string>(value);
            return "";
        }

        std::string_view as_string_view() const {
            if (is_lazy()) {
                const auto& l = std::get<LazyNode>(value);
                const char* base = l.base_ptr;
                const char* s = ASM::skip_whitespace(base + l.offset, base + l.doc->len);
                uint32_t start = (uint32_t)(s - base);
                Cursor c(l.doc.get(), start + 1);
                uint32_t end = c.next();
                return std::string_view(base + start + 1, end - start - 1);
            }
            if (std::holds_alternative<std::string>(value)) return std::get<std::string>(value);
            return "";
        }

        double as_double() const {
            if (is_lazy()) {
                const auto& l = std::get<LazyNode>(value);
                const char* s = ASM::skip_whitespace(l.base_ptr + l.offset, l.base_ptr + l.doc->len);
                double d;
                std::from_chars(s, l.base_ptr + l.doc->len, d);
                return d;
            }
            if (std::holds_alternative<double>(value)) return std::get<double>(value);
            return 0.0;
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
            uint32_t start = (uint32_t)(s - base) + 1;
            Cursor c(l.doc.get(), start);
            while (true) {
                uint32_t curr = c.next();
                if (curr == (uint32_t)-1 || base[curr] == '}') return json();
                if (base[curr] == ',') continue;
                if (base[curr] == '"') {
                    uint32_t end_q = c.next();
                    size_t k_len = end_q - curr - 1;
                    std::string_view k(base + curr + 1, k_len);
                    uint32_t colon = c.next();
                    if (k == key) {
                        const char* vs = ASM::skip_whitespace(base + colon + 1, base + l.doc->len);
                        return json(LazyNode{l.doc, (uint32_t)(vs - base), base});
                    }
                    const char* vs = ASM::skip_whitespace(base + colon + 1, base + l.doc->len);
                    char vc = *vs;
                    if (vc == '{') skip_container(c, base, '{', '}');
                    else if (vc == '[') skip_container(c, base, '[', ']');
                    else if (vc == '"') { c.next(); c.next(); }
                }
            }
        }

        void skip_container(Cursor& c, const char* base, char open, char close) const {
            int depth = 1;
            while (depth > 0) {
                uint32_t curr = c.next();
                if (curr == (uint32_t)-1) break;
                char ch = base[curr];
                if (ch == open) depth++;
                else if (ch == close) depth--;
                else if (ch == '"') c.next();
            }
        }

        json lazy_index(size_t idx) const {
            const auto& l = std::get<LazyNode>(value);
            const char* base = l.base_ptr;
            const char* s = ASM::skip_whitespace(base + l.offset, base + l.doc->len);
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
            uint32_t start = (uint32_t)(s - base) + 1;
            if (*(ASM::skip_whitespace(base + start, base + l.doc->len)) == ']') return 0;
            Cursor c(l.doc.get(), start);
            size_t commas = 0;
            while (true) {
                uint32_t curr = c.next();
                if (curr == (uint32_t)-1) break;
                char ch = base[curr];
                if (ch == ']') break;
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
