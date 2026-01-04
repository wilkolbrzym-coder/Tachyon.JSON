#ifndef TACHYON_HPP
#define TACHYON_HPP

// Tachyon JSON Library v6.0
// The World's Fastest JSON Library

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

#ifdef _MSC_VER
#include <intrin.h>
#include <malloc.h>
#else
#include <immintrin.h>
#endif

#ifndef _MSC_VER
#define TACHYON_LIKELY(x) __builtin_expect(!!(x), 1)
#define TACHYON_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define TACHYON_LIKELY(x) (x)
#define TACHYON_UNLIKELY(x) (x)
#endif

namespace Tachyon {
    class json;
    template<typename T> void to_json(json& j, const T& t);
    template<typename T> void from_json(const json& j, T& t);
}

// -----------------------------------------------------------------------------
// Reflection Macros
// -----------------------------------------------------------------------------
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
        [[nodiscard]] inline const char* skip_whitespace(const char* p, const char* end) {
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
    }

    namespace SIMD {
        static const __m256i v_lo_tbl = _mm256_broadcastsi128_si256(_mm_setr_epi8(0, 0, 0x40, 0, 0, 0, 0, 0, 0, 0, 0x80, 0x80, 0xA0, 0x80, 0, 0x80));
        static const __m256i v_hi_tbl = _mm256_broadcastsi128_si256(_mm_setr_epi8(0, 0, 0xC0, 0x80, 0, 0xA0, 0, 0x80, 0, 0, 0, 0, 0, 0, 0, 0));
        static const __m256i v_0f = _mm256_set1_epi8(0x0F);

        inline size_t compute_structural_mask(const char* data, size_t len, uint32_t* mask_array) {
            size_t i = 0;
            size_t block_idx = 0;
            uint64_t prev_escapes = 0;
            uint32_t in_string_mask = 0;
            for (; i + 128 <= len; i += 128) {
                uint32_t final_masks[4];
                for (int k = 0; k < 4; ++k) {
                    size_t off = i + (k * 32);
                    __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + off));
                    __m256i lo = _mm256_and_si256(chunk, v_0f);
                    __m256i hi = _mm256_and_si256(_mm256_srli_epi16(chunk, 4), v_0f);
                    __m256i char_class = _mm256_and_si256(_mm256_shuffle_epi8(v_lo_tbl, lo), _mm256_shuffle_epi8(v_hi_tbl, hi));
                    uint32_t struct_mask = _mm256_movemask_epi8(char_class);
                    uint32_t quote_mask = _mm256_movemask_epi8(_mm256_slli_epi16(char_class, 1));
                    uint32_t bs_mask = _mm256_movemask_epi8(_mm256_slli_epi16(char_class, 2));
                    if (TACHYON_UNLIKELY(bs_mask != 0 || prev_escapes > 0)) {
                         uint32_t real_quote_mask = 0;
                         const char* c_ptr = data + off;
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
                    final_masks[k] = (struct_mask & ~p) | quote_mask;
                }
                mask_array[block_idx] = final_masks[0];
                mask_array[block_idx+1] = final_masks[1];
                mask_array[block_idx+2] = final_masks[2];
                mask_array[block_idx+3] = final_masks[3];
                block_idx += 4;
            }
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
    }

    struct AlignedDeleter { void operator()(uint32_t* p) const { ASM::aligned_free(p); } };

    class Document {
    public:
        std::string storage;
        std::unique_ptr<uint32_t[], AlignedDeleter> bitmask;
        size_t len = 0;
        size_t bitmask_len = 0;
        size_t bitmask_cap = 0;
        void parse(std::string&& json) { storage = std::move(json); parse_view(storage.data(), storage.size()); }
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

        inline uint32_t next() {
            while (true) {
                if (mask != 0) {
                    int bit = std::countr_zero(mask);
                    uint32_t offset = block_idx * 32 + bit;
                    mask &= (mask - 1);
                    if (base[offset] == '/') {
                         if (base + offset + 1 >= end_ptr) return (uint32_t)-1;
                         const char* p = base + offset + 2;
                         if (base[offset+1] == '/') {
                             while(p < end_ptr && *p != '\n') p++;
                             uint32_t new_off = (uint32_t)(p - base);
                             block_idx = new_off / 32;
                             int b = new_off % 32;
                             if (block_idx < max_block) { mask = bitmask_ptr[block_idx]; mask &= ~((1U << b) - 1); }
                             else return (uint32_t)-1;
                             continue;
                         } else if (base[offset+1] == '*') {
                             while(p < end_ptr - 1 && !(*p == '*' && *(p+1) == '/')) p++;
                             uint32_t new_off = (uint32_t)(p - base) + 2;
                             block_idx = new_off / 32;
                             int b = new_off % 32;
                             if (block_idx < max_block) { mask = bitmask_ptr[block_idx]; mask &= ~((1U << b) - 1); }
                             else return (uint32_t)-1;
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
    };

    using ObjectType = std::map<std::string, class json, std::less<>>;
    using ArrayType = std::vector<class json>;
    struct LazyNode { std::shared_ptr<Document> doc; uint32_t offset; const char* base_ptr; };

    class json {
        std::variant<std::monostate, bool, int64_t, uint64_t, double, std::string, ObjectType, ArrayType, LazyNode> value;

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

        template<typename T, typename = std::enable_if_t<
            !std::is_same_v<T, json> && !std::is_same_v<T, std::string> && !std::is_same_v<T, const char*> &&
            !std::is_arithmetic_v<T> && !std::is_null_pointer_v<T>>>
        json(const T& t) { to_json(*this, t); }

        static json object() { return json(ObjectType{}); }
        static json array() { return json(ArrayType{}); }

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

        static json from_cbor(const std::vector<uint8_t>& b) { throw std::runtime_error("CBOR not fully implemented"); }
        static json from_msgpack(const std::vector<uint8_t>& b) { throw std::runtime_error("MsgPack not fully implemented"); }

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

        size_t size() const {
             if (is_lazy()) return lazy_size();
             if (std::holds_alternative<ArrayType>(value)) return std::get<ArrayType>(value).size();
             if (std::holds_alternative<ObjectType>(value)) return std::get<ObjectType>(value).size();
             return 0;
        }

        template<typename T> void get_to(T& t) const {
            if constexpr (std::is_same_v<T, int>) t = (int)as_int64();
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

        const json operator[](const std::string& key) const {
            if (is_lazy()) return lazy_lookup(key);
            if (std::holds_alternative<ObjectType>(value)) {
                const auto& o = std::get<ObjectType>(value);
                auto it = o.find(key);
                if (it != o.end()) return it->second;
            }
            return json();
        }

        const json operator[](size_t idx) const {
            if (is_lazy()) return lazy_index(idx);
            if (std::holds_alternative<ArrayType>(value)) {
                const auto& a = std::get<ArrayType>(value);
                if (idx < a.size()) return a[idx];
            }
            return json();
        }

        std::string as_string() const {
            if (is_lazy()) {
                const auto& l = std::get<LazyNode>(value);
                const char* s = ASM::skip_whitespace(l.base_ptr + l.offset, l.base_ptr + l.doc->len);
                if (*s != '"') return "";
                uint32_t start = (uint32_t)(s - l.base_ptr);
                Cursor c(l.doc.get(), start + 1, l.base_ptr);
                uint32_t end = c.next();
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
            else { value = as_int64(); }
        }

        json lazy_lookup(const std::string& key) const {
            const auto& l = std::get<LazyNode>(value);
            const char* base = l.base_ptr;
            const char* s = ASM::skip_whitespace(base + l.offset, base + l.doc->len);
            uint32_t start = (uint32_t)(s - base) + 1;
            Cursor c(l.doc.get(), start, base);
            while (true) {
                uint32_t curr = c.next();
                if (curr == (uint32_t)-1 || base[curr] == '}') return json();
                if (base[curr] == ',') continue;
                if (base[curr] == '"') {
                    uint32_t end_q = c.next();
                    size_t k_len = end_q - curr - 1;
                    bool match = (k_len == key.size()) && (memcmp(base + curr + 1, key.data(), k_len) == 0);
                    uint32_t colon = c.next();
                    const char* vs = ASM::skip_whitespace(base + colon + 1, base + l.doc->len);
                    if (match) return json(LazyNode{l.doc, (uint32_t)(vs - base), base});
                    char vc = *vs;
                    if (vc == '{') skip_container(c, base, '{', '}');
                    else if (vc == '[') skip_container(c, base, '[', ']');
                    else if (vc == '"') { c.next(); c.next(); }
                }
            }
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

        size_t lazy_size() const {
            const auto& l = std::get<LazyNode>(value);
            const char* base = l.base_ptr;
            const char* s = ASM::skip_whitespace(base + l.offset, base + l.doc->len);
            if (*s != '[') return 0;
            const char* p = ASM::skip_whitespace(s + 1, base + l.doc->len);
            if (*p == ']') return 0;
            uint32_t start = (uint32_t)(s - base) + 1;
            Cursor c(l.doc.get(), start, base);
            size_t count = 1;
            while (true) {
                char ch = *p;
                uint32_t next_delim;
                if (ch == '{') { skip_container(c, base, '{', '}'); next_delim = c.next(); }
                else if (ch == '[') { skip_container(c, base, '[', ']'); next_delim = c.next(); }
                else if (ch == '"') { c.next(); c.next(); next_delim = c.next(); }
                else { next_delim = c.next(); }
                if (next_delim == (uint32_t)-1 || base[next_delim] == ']') break;
                if (base[next_delim] == ',') count++;
                p = ASM::skip_whitespace(base + next_delim + 1, base + l.doc->len);
            }
            return count;
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
    };
}
#endif
