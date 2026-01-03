#ifndef TACHYON_HPP
#define TACHYON_HPP

/*
 * Tachyon v5.3 "Turbo"
 * The World's Fastest JSON Library
 */

#include <algorithm>
#include <bit>
#include <cassert>
#include <charconv>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <format>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

// Platform & SIMD Checks
#if defined(__x86_64__) || defined(_M_X64)
    #include <immintrin.h>
    #define TACHYON_X64 1
    #define TACHYON_HAS_AVX2 1
    #ifdef _MSC_VER
        #include <intrin.h>
        #define TACHYON_MSVC 1
    #else
        #define TACHYON_MSVC 0
    #endif
#else
    #define TACHYON_X64 0
    #define TACHYON_HAS_AVX2 0
    #define TACHYON_MSVC 0
#endif

namespace Tachyon {

// -----------------------------------------------------------------------------
// Configuration
// -----------------------------------------------------------------------------

class Json;
class ObjectMap;

struct ParseOptions {
    bool allow_comments = true;
    bool allow_trailing_commas = true;
    bool fast_float = true;
};

struct DumpOptions {
    int indent = -1;
    char indent_char = ' ';
    bool sort_keys = false;
    bool ascii_only = false;
};

enum class Type : uint8_t {
    Null, Boolean, NumberInt, NumberFloat, String, Array, Object, Binary
};

// -----------------------------------------------------------------------------
// ASM / SIMD Internals
// -----------------------------------------------------------------------------

namespace ASM {
    inline const char* skip_whitespace_asm(const char* ptr) {
#if TACHYON_X64 && !TACHYON_MSVC
        // GCC/Clang Inline Assembly
        const char* result;
        __asm__ volatile (
            "1:\n\t"
            "movzbl (%1), %%eax\n\t"
            "cmp $0x20, %%al\n\t" "je 2f\n\t"
            "cmp $0x0A, %%al\n\t" "je 2f\n\t"
            "cmp $0x0D, %%al\n\t" "je 2f\n\t"
            "cmp $0x09, %%al\n\t" "je 2f\n\t"
            "jmp 3f\n"
            "2:\n\t"
            "inc %1\n\t"
            "jmp 1b\n"
            "3:\n\t"
            "mov %1, %0"
            : "=r" (result) : "r" (ptr) : "rax", "cc"
        );
        return result;
#else
        // Scalar Fallback (also used for MSVC where inline asm is not supported x64)
        while (*ptr == ' ' || *ptr == '\n' || *ptr == '\r' || *ptr == '\t') ++ptr;
        return ptr;
#endif
    }

    inline const char* skip_whitespace_simd(const char* ptr, const char* end) {
#if TACHYON_HAS_AVX2
        const __m256i spaces = _mm256_set1_epi8(' ');
        const __m256i newlines = _mm256_set1_epi8('\n');
        const __m256i crs = _mm256_set1_epi8('\r');
        const __m256i tabs = _mm256_set1_epi8('\t');

        // Safety: ensure we have 32 bytes
        while (ptr + 32 <= end) {
            __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr));
            __m256i s = _mm256_cmpeq_epi8(chunk, spaces);
            __m256i n = _mm256_cmpeq_epi8(chunk, newlines);
            __m256i c = _mm256_cmpeq_epi8(chunk, crs);
            __m256i t = _mm256_cmpeq_epi8(chunk, tabs);
            __m256i mask_vec = _mm256_or_si256(_mm256_or_si256(s, n), _mm256_or_si256(c, t));
            unsigned int mask = _mm256_movemask_epi8(mask_vec);

            if (mask == 0xFFFFFFFF) {
                ptr += 32;
            } else {
                int offset = std::countr_one(mask);
                return ptr + offset;
            }
        }
#endif
        // Tail processing scalar
        while (ptr < end && (*ptr == ' ' || *ptr == '\n' || *ptr == '\r' || *ptr == '\t')) ++ptr;
        return ptr;
    }
}

// -----------------------------------------------------------------------------
// Internal Buffer for Serialization
// -----------------------------------------------------------------------------

struct Buffer {
    std::vector<char> data;
    Buffer() { data.reserve(65536); }
    void append(char c) { data.push_back(c); }
    void append(const char* s, size_t len) { data.insert(data.end(), s, s + len); }
    void append(std::string_view s) { append(s.data(), s.size()); }
    std::string str() const { return std::string(data.data(), data.size()); }
};

// -----------------------------------------------------------------------------
// ObjectMap Definition
// -----------------------------------------------------------------------------

class ObjectMap {
public:
    using Member = std::pair<std::string, Json>;
    using Container = std::vector<Member>;
    using iterator = Container::iterator;
    using const_iterator = Container::const_iterator;

    Container m_data;
    bool m_sorted = false;

    ObjectMap() = default;
    void sort();

    Json& operator[](std::string_view key);
    const Json& at(std::string_view key) const;
    bool contains(std::string_view key) const;
    void insert(std::string key, Json value);
    void erase(std::string_view key);

    iterator begin() { return m_data.begin(); }
    iterator end() { return m_data.end(); }
    const_iterator begin() const { return m_data.begin(); }
    const_iterator end() const { return m_data.end(); }
    size_t size() const { return m_data.size(); }
    bool empty() const { return m_data.empty(); }

private:
    iterator find_impl(std::string_view key);
    const_iterator find_impl(std::string_view key) const;
};

// -----------------------------------------------------------------------------
// Json Class Definition
// -----------------------------------------------------------------------------

class Json {
public:
    using array_t = std::vector<Json>;
    using object_t = ObjectMap;
    using binary_t = std::vector<uint8_t>;

    friend class Parser;
    friend void flatten_impl(std::string prefix, const Json& j, Json& res);

private:
    using Variant = std::variant<std::monostate, bool, int64_t, double, std::string, std::shared_ptr<array_t>, std::shared_ptr<object_t>, binary_t>;
    Variant m_data;

public:
    Json() : m_data(std::monostate{}) {}
    Json(std::nullptr_t) : m_data(std::monostate{}) {}
    Json(bool v) : m_data(v) {}
    Json(int v) : m_data(static_cast<int64_t>(v)) {}
    Json(int64_t v) : m_data(v) {}
    Json(double v) : m_data(v) {}
    Json(const char* s) : m_data(std::string(s)) {}
    Json(std::string s) : m_data(std::move(s)) {}
    Json(std::string_view s) : m_data(std::string(s)) {}
    Json(const array_t& arr) : m_data(std::make_shared<array_t>(arr)) {}
    Json(const object_t& obj) : m_data(std::make_shared<object_t>(obj)) {}
    Json(array_t&& arr) : m_data(std::make_shared<array_t>(std::move(arr))) {}
    Json(object_t&& obj) : m_data(std::make_shared<object_t>(std::move(obj))) {}

    Json(std::initializer_list<Json> init);

    // Type
    Type type() const;
    bool is_null() const { return type() == Type::Null; }
    bool is_boolean() const { return type() == Type::Boolean; }
    bool is_number_int() const { return type() == Type::NumberInt; }
    bool is_number_float() const { return type() == Type::NumberFloat; }
    bool is_string() const { return type() == Type::String; }
    bool is_array() const { return type() == Type::Array; }
    bool is_object() const { return type() == Type::Object; }

    // Getters
    template <typename T> T get() const;

    // Accessors
    Json& operator[](size_t index);
    const Json& operator[](size_t index) const;
    Json& operator[](std::string_view key);
    const Json& operator[](std::string_view key) const;
    bool contains(std::string_view key) const;

    size_t size() const;
    bool empty() const;
    void push_back(Json val);
    void clear();

    // Advanced API
    Json* pointer(std::string_view path);
    const Json* pointer(std::string_view path) const;
    void merge_patch(const Json& patch);
    Json flatten() const;
    static Json unflatten(const Json& flat);

    // IO
    static Json parse(std::string_view json, const ParseOptions& opts = {});
    std::string dump(const DumpOptions& opts = {}) const;
    void dump_to(Buffer& buf, const DumpOptions& opts) const;

    bool operator==(const Json& other) const;
};

// -----------------------------------------------------------------------------
// Parser Definition
// -----------------------------------------------------------------------------

class Parser {
    const char* m_ptr;
    const char* m_end;
    ParseOptions m_opts;

public:
    Parser(std::string_view json, const ParseOptions& opts)
        : m_ptr(json.data()), m_end(json.data() + json.size()), m_opts(opts) {}

    Json parse();

private:
    Json parse_value();
    Json parse_object();
    Json parse_array();
    std::string parse_string_raw();
    Json parse_string();
    Json parse_number();
    Json parse_true();
    Json parse_false();
    Json parse_null();
};

// -----------------------------------------------------------------------------
// Implementation: ObjectMap
// -----------------------------------------------------------------------------

inline void ObjectMap::sort() {
    if (m_sorted) return;
    std::sort(m_data.begin(), m_data.end(), [](const Member& a, const Member& b) {
        return a.first < b.first;
    });
    m_sorted = true;
}

inline ObjectMap::iterator ObjectMap::find_impl(std::string_view key) {
    if (m_sorted) {
        auto it = std::lower_bound(m_data.begin(), m_data.end(), key,
            [](const Member& m, std::string_view k) { return m.first < k; });
        if (it != m_data.end() && it->first == key) return it;
        return m_data.end();
    }
    return std::find_if(m_data.begin(), m_data.end(), [&](const Member& m) { return m.first == key; });
}

inline ObjectMap::const_iterator ObjectMap::find_impl(std::string_view key) const {
    if (m_sorted) {
        auto it = std::lower_bound(m_data.begin(), m_data.end(), key,
            [](const Member& m, std::string_view k) { return m.first < k; });
        if (it != m_data.end() && it->first == key) return it;
        return m_data.end();
    }
    return std::find_if(m_data.begin(), m_data.end(), [&](const Member& m) { return m.first == key; });
}

inline Json& ObjectMap::operator[](std::string_view key) {
    auto it = find_impl(key);
    if (it != m_data.end()) return it->second;
    m_data.emplace_back(std::string(key), Json());
    m_sorted = false;
    return m_data.back().second;
}

inline const Json& ObjectMap::at(std::string_view key) const {
    auto it = find_impl(key);
    if (it == m_data.end()) throw std::runtime_error("Key not found");
    return it->second;
}

inline bool ObjectMap::contains(std::string_view key) const {
    return find_impl(key) != m_data.end();
}

inline void ObjectMap::insert(std::string key, Json value) {
    auto it = find_impl(key);
    if (it != m_data.end()) it->second = std::move(value);
    else {
        m_data.emplace_back(std::move(key), std::move(value));
        m_sorted = false;
    }
}

inline void ObjectMap::erase(std::string_view key) {
    auto it = find_impl(key);
    if (it != m_data.end()) {
        m_data.erase(it);
    }
}

// -----------------------------------------------------------------------------
// Implementation: Json
// -----------------------------------------------------------------------------

inline Json::Json(std::initializer_list<Json> init) {
    bool is_obj = std::all_of(init.begin(), init.end(), [](const Json& j) {
        return j.is_array() && j.size() == 2 && j[0].is_string();
    });

    if (is_obj && init.size() > 0) {
        auto obj = std::make_shared<object_t>();
        for (const auto& el : init) {
            obj->insert(el[0].get<std::string>(), el[1]);
        }
        obj->sort();
        m_data = obj;
    } else {
        auto arr = std::make_shared<array_t>(init);
        m_data = arr;
    }
}

inline Type Json::type() const {
    if (std::holds_alternative<std::monostate>(m_data)) return Type::Null;
    if (std::holds_alternative<bool>(m_data)) return Type::Boolean;
    if (std::holds_alternative<int64_t>(m_data)) return Type::NumberInt;
    if (std::holds_alternative<double>(m_data)) return Type::NumberFloat;
    if (std::holds_alternative<std::string>(m_data)) return Type::String;
    if (std::holds_alternative<std::shared_ptr<array_t>>(m_data)) return Type::Array;
    if (std::holds_alternative<std::shared_ptr<object_t>>(m_data)) return Type::Object;
    return Type::Null;
}

template <typename T>
T Json::get() const {
    if constexpr (std::is_same_v<T, bool>) {
        if (is_boolean()) return std::get<bool>(m_data);
    } else if constexpr (std::integral<T>) {
        if (is_number_int()) return static_cast<T>(std::get<int64_t>(m_data));
        if (is_number_float()) return static_cast<T>(std::get<double>(m_data));
    } else if constexpr (std::floating_point<T>) {
        if (is_number_float()) return static_cast<T>(std::get<double>(m_data));
        if (is_number_int()) return static_cast<T>(std::get<int64_t>(m_data));
    } else if constexpr (std::is_same_v<T, std::string>) {
        if (is_string()) return std::get<std::string>(m_data);
    }
    throw std::runtime_error("Type mismatch");
}

inline Json& Json::operator[](size_t index) {
    if (is_null()) m_data = std::make_shared<array_t>();
    if (!is_array()) throw std::runtime_error("Not an array");
    auto& arr = *std::get<std::shared_ptr<array_t>>(m_data);
    if (index >= arr.size()) arr.resize(index + 1);
    return arr[index];
}

inline const Json& Json::operator[](size_t index) const {
    if (!is_array()) throw std::runtime_error("Not an array");
    return (*std::get<std::shared_ptr<array_t>>(m_data))[index];
}

inline Json& Json::operator[](std::string_view key) {
    if (is_null()) m_data = std::make_shared<object_t>();
    if (!is_object()) throw std::runtime_error("Not an object");
    return (*std::get<std::shared_ptr<object_t>>(m_data))[key];
}

inline const Json& Json::operator[](std::string_view key) const {
    if (!is_object()) throw std::runtime_error("Not an object");
    return std::get<std::shared_ptr<object_t>>(m_data)->at(key);
}

inline bool Json::contains(std::string_view key) const {
    if (!is_object()) return false;
    return std::get<std::shared_ptr<object_t>>(m_data)->contains(key);
}

inline size_t Json::size() const {
    if (is_array()) return std::get<std::shared_ptr<array_t>>(m_data)->size();
    if (is_object()) return std::get<std::shared_ptr<object_t>>(m_data)->size();
    if (is_string()) return std::get<std::string>(m_data).size();
    return 1;
}

inline bool Json::empty() const { return size() == 0; }

inline void Json::push_back(Json val) {
    if (is_null()) m_data = std::make_shared<array_t>();
    if (!is_array()) throw std::runtime_error("Not an array");
    std::get<std::shared_ptr<array_t>>(m_data)->push_back(std::move(val));
}

inline void Json::clear() {
    m_data = std::monostate{};
}

inline bool Json::operator==(const Json& other) const {
    if (type() != other.type()) return false;
    if (is_number_int()) return get<int64_t>() == other.get<int64_t>();
    if (is_string()) return get<std::string>() == other.get<std::string>();
    return true;
}

// -----------------------------------------------------------------------------
// Advanced Features Implementation
// -----------------------------------------------------------------------------

inline Json* Json::pointer(std::string_view path) {
    if (path.empty()) return this;
    if (path[0] != '/') return nullptr;
    Json* current = this;
    size_t pos = 1;
    while (pos < path.size()) {
        size_t next = path.find('/', pos);
        if (next == std::string_view::npos) next = path.size();
        std::string_view token = path.substr(pos, next - pos);

        std::string decoded;
        decoded.reserve(token.size());
        for(size_t i=0; i<token.size(); ++i) {
            if(token[i] == '~' && i+1 < token.size()) {
                if(token[i+1]=='1') { decoded+='/'; i++; }
                else if(token[i+1]=='0') { decoded+='~'; i++; }
                else decoded += token[i];
            } else decoded += token[i];
        }

        if (current->is_array()) {
            int idx;
            auto res = std::from_chars(decoded.data(), decoded.data()+decoded.size(), idx);
            if (res.ec != std::errc() || idx < 0) return nullptr;
            if (static_cast<size_t>(idx) >= current->size()) return nullptr;
            current = &(*current)[idx];
        } else if (current->is_object()) {
             if (!current->contains(decoded)) return nullptr;
             current = &(*current)[decoded];
        }
        pos = next + 1;
    }
    return current;
}

inline const Json* Json::pointer(std::string_view path) const {
    return const_cast<Json*>(this)->pointer(path);
}

inline void Json::merge_patch(const Json& patch) {
    if (!patch.is_object()) {
        *this = patch;
        return;
    }
    if (!is_object()) *this = Json::object_t{};

    const auto& patch_obj = *std::get<std::shared_ptr<object_t>>(patch.m_data);
    auto& target_obj = *std::get<std::shared_ptr<object_t>>(m_data);

    for (const auto& [key, val] : patch_obj) {
        if (val.is_null()) {
            target_obj.erase(key);
        } else {
            target_obj[key].merge_patch(val);
        }
    }
}

inline void flatten_impl(std::string prefix, const Json& j, Json& res) {
    if (j.is_object()) {
        const auto& obj = *std::get<std::shared_ptr<ObjectMap>>(j.m_data);
        for (const auto& [k, v] : obj) {
            flatten_impl(prefix + (prefix.empty()?"":".") + k, v, res);
        }
    } else if (j.is_array()) {
        const auto& arr = *std::get<std::shared_ptr<std::vector<Json>>>(j.m_data);
        for (size_t i=0; i<arr.size(); ++i) {
            flatten_impl(prefix + (prefix.empty()?"":".") + std::to_string(i), arr[i], res);
        }
    } else {
        res[prefix] = j;
    }
}

inline Json Json::flatten() const {
    Json res = Json::object_t{};
    flatten_impl("", *this, res);
    return res;
}

inline Json Json::unflatten(const Json& flat) {
    if (!flat.is_object()) return flat;
    Json res = Json::object_t{};
    const auto& obj = *std::get<std::shared_ptr<ObjectMap>>(flat.m_data);

    // Sort keys to ensure array indices are processed in order if possible, though naive impl follows
    for (const auto& [key, val] : obj) {
        Json* curr = &res;
        std::string_view path = key;
        size_t pos = 0;

        while (pos < path.size()) {
            size_t dot = path.find('.', pos);
            if (dot == std::string_view::npos) dot = path.size();
            std::string_view token = path.substr(pos, dot - pos);

            bool is_array_idx = !token.empty() && std::all_of(token.begin(), token.end(), ::isdigit);

            if (dot == path.size()) {
                // Leaf
                if (curr->is_null()) {
                    if (is_array_idx) *curr = Json::array_t{}; else *curr = Json::object_t{};
                }
                if (curr->is_array()) {
                    int idx = 0; std::from_chars(token.data(), token.data()+token.size(), idx);
                    if ((size_t)idx >= curr->size()) (*curr)[idx] = val; // Auto-resize
                    else (*curr)[idx] = val;
                } else {
                    (*curr)[token] = val;
                }
            } else {
                // Node
                if (curr->is_null()) {
                    if (is_array_idx) *curr = Json::array_t{}; else *curr = Json::object_t{};
                }
                if (curr->is_array()) {
                     int idx = 0; std::from_chars(token.data(), token.data()+token.size(), idx);
                     curr = &(*curr)[idx];
                } else {
                     curr = &(*curr)[token];
                }
            }
            pos = dot + 1;
        }
    }
    return res;
}

// -----------------------------------------------------------------------------
// Parser Implementation
// -----------------------------------------------------------------------------

inline Json Parser::parse() {
    m_ptr = ASM::skip_whitespace_simd(m_ptr, m_end);
    if (m_ptr >= m_end) return Json();
    return parse_value();
}

inline Json Parser::parse_value() {
    char c = *m_ptr;
    switch(c) {
        case '{': return parse_object();
        case '[': return parse_array();
        case '"': return parse_string();
        case 't': return parse_true();
        case 'f': return parse_false();
        case 'n': return parse_null();
        default:
            if (c == '-' || (c >= '0' && c <= '9')) return parse_number();
            throw std::runtime_error(std::format("Unexpected char: {}", c));
    }
}

inline Json Parser::parse_object() {
    m_ptr++; // {
    auto obj = std::make_shared<ObjectMap>();
    m_ptr = ASM::skip_whitespace_simd(m_ptr, m_end);
    if (*m_ptr == '}') { m_ptr++; return Json(ObjectMap{}); }

    while (true) {
        if (*m_ptr != '"') throw std::runtime_error("Expected string key");
        std::string key = parse_string_raw();

        m_ptr = ASM::skip_whitespace_simd(m_ptr, m_end);
        if (*m_ptr != ':') throw std::runtime_error("Expected :");
        m_ptr++;
        m_ptr = ASM::skip_whitespace_simd(m_ptr, m_end);

        Json val = parse_value();
        obj->m_data.emplace_back(std::move(key), std::move(val));

        m_ptr = ASM::skip_whitespace_simd(m_ptr, m_end);
        if (*m_ptr == '}') { m_ptr++; break; }
        if (*m_ptr == ',') { m_ptr++; m_ptr = ASM::skip_whitespace_simd(m_ptr, m_end); continue; }
        throw std::runtime_error("Expected } or ,");
    }
    obj->sort();
    Json j; j.m_data = obj;
    return j;
}

inline Json Parser::parse_array() {
    m_ptr++; // [
    auto arr = std::make_shared<std::vector<Json>>();
    m_ptr = ASM::skip_whitespace_simd(m_ptr, m_end);
    if (*m_ptr == ']') { m_ptr++; return Json(std::vector<Json>{}); }

    while(true) {
        arr->push_back(parse_value());
        m_ptr = ASM::skip_whitespace_simd(m_ptr, m_end);
        if (*m_ptr == ']') { m_ptr++; break; }
        if (*m_ptr == ',') { m_ptr++; m_ptr = ASM::skip_whitespace_simd(m_ptr, m_end); continue; }
        throw std::runtime_error("Expected ] or ,");
    }
    Json j; j.m_data = arr;
    return j;
}

inline std::string Parser::parse_string_raw() {
    m_ptr++; // "
    const char* start = m_ptr;
    while(m_ptr < m_end) {
#if TACHYON_HAS_AVX2
        // Safety check: ensure 32 bytes
        if (m_ptr + 32 <= m_end) {
            __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(m_ptr));
            __m256i q = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('"'));
            __m256i s = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('\\'));
            int mask = _mm256_movemask_epi8(_mm256_or_si256(q, s));
            if (mask) {
                m_ptr += std::countr_zero((unsigned int)mask);
                break;
            }
            m_ptr += 32;
            continue;
        }
#endif
        // Scalar fallback for tail or non-AVX
        if (*m_ptr == '"' || *m_ptr == '\\') break;
        m_ptr++;
    }

    if (m_ptr < m_end && *m_ptr == '"') {
        std::string res(start, m_ptr - start);
        m_ptr++;
        return res;
    }

    // Escapes
    std::string res;
    res.append(start, m_ptr - start);
    while (m_ptr < m_end) {
        char c = *m_ptr++;
        if (c == '"') return res;
        if (c == '\\') {
            char e = *m_ptr++;
            switch(e) {
                case '"': res += '"'; break;
                case '\\': res += '\\'; break;
                case '/': res += '/'; break;
                case 'b': res += '\b'; break;
                case 'f': res += '\f'; break;
                case 'n': res += '\n'; break;
                case 'r': res += '\r'; break;
                case 't': res += '\t'; break;
                case 'u': {
                     // TODO: Proper Unicode
                     m_ptr += 4;
                     res += '?';
                     break;
                }
                default: res += e;
            }
        } else res += c;
    }
    throw std::runtime_error("Unterminated string");
}

inline Json Parser::parse_string() { return Json(parse_string_raw()); }

inline Json Parser::parse_number() {
    const char* start = m_ptr;
    bool is_float = false;
    if (*m_ptr == '-') m_ptr++;
    while (isdigit(*m_ptr)) m_ptr++;
    if (*m_ptr == '.') { is_float = true; m_ptr++; while (isdigit(*m_ptr)) m_ptr++; }
    if (*m_ptr == 'e' || *m_ptr == 'E') { is_float = true; m_ptr++; if(*m_ptr=='+'||*m_ptr=='-')m_ptr++; while(isdigit(*m_ptr))m_ptr++; }

    std::string_view sv(start, m_ptr - start);
    if (is_float) {
        double v; std::from_chars(sv.data(), sv.data()+sv.size(), v);
        return Json(v);
    } else {
        int64_t v; std::from_chars(sv.data(), sv.data()+sv.size(), v);
        return Json(v);
    }
}

inline Json Parser::parse_true() { m_ptr += 4; return Json(true); }
inline Json Parser::parse_false() { m_ptr += 5; return Json(false); }
inline Json Parser::parse_null() { m_ptr += 4; return Json(nullptr); }

// -----------------------------------------------------------------------------
// IO Implementation
// -----------------------------------------------------------------------------

inline Json Json::parse(std::string_view json, const ParseOptions& opts) {
    Parser p(json, opts);
    return p.parse();
}

inline void Json::dump_to(Buffer& buf, const DumpOptions& opts) const {
    if (is_null()) { buf.append("null", 4); return; }
    if (is_boolean()) {
        if(get<bool>()) buf.append("true", 4); else buf.append("false", 5);
        return;
    }
    if (is_number_int()) {
        std::string s = std::to_string(get<int64_t>());
        buf.append(s);
        return;
    }
    if (is_number_float()) {
        std::string s = std::to_string(get<double>());
        buf.append(s);
        return;
    }
    if (is_string()) {
        buf.append('"');
        const std::string& s = get<std::string>();
        // Safe escaping
        for (char c : s) {
            switch(c) {
                case '"': buf.append("\\\"", 2); break;
                case '\\': buf.append("\\\\", 2); break;
                case '\b': buf.append("\\b", 2); break;
                case '\f': buf.append("\\f", 2); break;
                case '\n': buf.append("\\n", 2); break;
                case '\r': buf.append("\\r", 2); break;
                case '\t': buf.append("\\t", 2); break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                         // Hex escape (simplified)
                         char hex[7];
                         snprintf(hex, 7, "\\u%04x", c);
                         buf.append(hex, 6);
                    } else {
                        buf.append(c);
                    }
            }
        }
        buf.append('"');
        return;
    }
    if (is_array()) {
        buf.append('[');
        const auto& arr = *std::get<std::shared_ptr<array_t>>(m_data);
        for (size_t i=0; i<arr.size(); ++i) {
            arr[i].dump_to(buf, opts);
            if (i < arr.size()-1) buf.append(',');
        }
        buf.append(']');
        return;
    }
    if (is_object()) {
        buf.append('{');
        const auto& obj = *std::get<std::shared_ptr<object_t>>(m_data);
        size_t i = 0;
        for (const auto& [k, v] : obj) {
            buf.append('"');
            buf.append(k);
            buf.append('"');
            buf.append(':');
            v.dump_to(buf, opts);
            if (i < obj.size()-1) buf.append(',');
            i++;
        }
        buf.append('}');
        return;
    }
}

inline std::string Json::dump(const DumpOptions& opts) const {
    Buffer buf;
    dump_to(buf, opts);
    return buf.str();
}

} // namespace Tachyon

#endif // TACHYON_HPP
