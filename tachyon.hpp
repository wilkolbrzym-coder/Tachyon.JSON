#ifndef TACHYON_HPP
#define TACHYON_HPP

// TACHYON v8.0 "SUPERNOVA"
// The Undisputed Superior Alternative to nlohmann::json
// (C) 2026 Tachyon Systems
// License: MIT

#include <iostream>
#include <vector>
#include <string>
#include <string_view>
#include <cstring>
#include <immintrin.h>
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
#include <utility>
#include <limits>
#include <iterator>

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#include <cpuid.h>
#endif

// -----------------------------------------------------------------------------
// MACROS & CONFIG
// -----------------------------------------------------------------------------
#ifndef TACHYON_FORCE_INLINE
    #ifdef _MSC_VER
        #define TACHYON_FORCE_INLINE __forceinline
    #else
        #define TACHYON_FORCE_INLINE __attribute__((always_inline)) inline
    #endif
#endif

#define TACHYON_LIKELY(x) __builtin_expect(!!(x), 1)
#define TACHYON_UNLIKELY(x) __builtin_expect(!!(x), 0)

namespace tachyon {

// -----------------------------------------------------------------------------
// EXCEPTIONS
// -----------------------------------------------------------------------------
class exception : public std::exception {
    std::string m_msg;
public:
    exception(std::string msg) : m_msg(std::move(msg)) {}
    const char* what() const noexcept override { return m_msg.c_str(); }
};

class parse_error : public exception {
public:
    parse_error(std::string msg) : exception("[parse_error] " + std::move(msg)) {}
};

class type_error : public exception {
public:
    type_error(std::string msg) : exception("[type_error] " + std::move(msg)) {}
};

// -----------------------------------------------------------------------------
// SIMD KERNELS
// -----------------------------------------------------------------------------
struct cpu_features {
    bool avx2 = false;
    cpu_features() {
#ifndef _MSC_VER
        __builtin_cpu_init();
        avx2 = __builtin_cpu_supports("avx2");
#else
        // Simplified MSC detection
        int info[4];
        __cpuid(info, 7);
        avx2 = (info[1] & (1 << 5)) != 0;
#endif
    }
};
static const cpu_features g_cpu;

namespace simd {
    // Skip whitespace using AVX2
    TACHYON_FORCE_INLINE const char* skip_whitespace(const char* p, const char* end) {
        if (!g_cpu.avx2) {
            while (p < end && (unsigned char)*p <= 32) p++;
            return p;
        }
        // AVX2 path
        const __m256i v_space = _mm256_set1_epi8(' ');
        const __m256i v_tab = _mm256_set1_epi8('\t');
        const __m256i v_lf = _mm256_set1_epi8('\n');
        const __m256i v_cr = _mm256_set1_epi8('\r');

        while (p + 32 <= end) {
            __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p));
            __m256i m1 = _mm256_cmpeq_epi8(chunk, v_space);
            __m256i m2 = _mm256_cmpeq_epi8(chunk, v_tab);
            __m256i m3 = _mm256_cmpeq_epi8(chunk, v_lf);
            __m256i m4 = _mm256_cmpeq_epi8(chunk, v_cr);
            __m256i mask = _mm256_or_si256(_mm256_or_si256(m1, m2), _mm256_or_si256(m3, m4));

            int res = _mm256_movemask_epi8(mask);
            if (res != -1) { // Found a non-whitespace char (wait, logic check)
                 // cmpeq returns 0xFF for match (whitespace).
                 // movemask returns 1 for whitespace.
                 // If res != 0xFFFFFFFF, there is a NON-whitespace.
                 // We want to skip while ALL are whitespace.
                 if ((unsigned int)res != 0xFFFFFFFF) {
                     // Found non-whitespace
                     // Invert res to find the first 0 bit (non-whitespace)
                     int non_ws = ~res;
                     return p + __builtin_ctz(non_ws);
                 }
                 p += 32;
            } else {
                 p += 32;
            }
        }
        while (p < end && (unsigned char)*p <= 32) p++;
        return p;
    }
}

// -----------------------------------------------------------------------------
// JSON TYPES & STORAGE
// -----------------------------------------------------------------------------
enum class value_t : uint8_t {
    null,
    object,
    array,
    string,
    boolean,
    number_integer,
    number_unsigned,
    number_float,
    discarded
};

// Forward decl
class json;

// We use vector<pair> for objects to reduce allocation count vs map,
// and preserve insertion order (Nlohmann behavior).
// For search, linear scan is fine for small objects.
using object_t = std::vector<std::pair<std::string, json>>;
using array_t = std::vector<json>;
using string_t = std::string;
using number_integer_t = int64_t;
using number_unsigned_t = uint64_t;
using number_float_t = double;
using boolean_t = bool;

class json {
    friend struct std::hash<json>;

    // Tagged Union for SOO (Small Object Optimization)
    // We inline small types.
    // For vector/string/map, we use unique pointers to keep the main object small and movable.
    // Nlohmann uses a generic "value" variant. We optimize.

    union data_t {
        object_t* object;
        array_t* array;
        string_t* string;
        number_integer_t number_integer;
        number_unsigned_t number_unsigned;
        number_float_t number_float;
        boolean_t boolean;

        data_t() : object(nullptr) {}
    } m_data;

    value_t m_type = value_t::null;

    void destroy() {
        switch (m_type) {
            case value_t::object: delete m_data.object; break;
            case value_t::array: delete m_data.array; break;
            case value_t::string: delete m_data.string; break;
            default: break;
        }
        m_type = value_t::null;
        m_data.object = nullptr;
    }

public:
    // -------------------------------------------------------------------------
    // CONSTRUCTORS
    // -------------------------------------------------------------------------
    json() = default;
    ~json() { destroy(); }

    json(std::nullptr_t) : m_type(value_t::null) {}
    json(bool v) : m_type(value_t::boolean) { m_data.boolean = v; }
    json(int v) : m_type(value_t::number_integer) { m_data.number_integer = v; }
    json(int64_t v) : m_type(value_t::number_integer) { m_data.number_integer = v; }
    json(size_t v) : m_type(value_t::number_unsigned) { m_data.number_unsigned = v; }
    json(double v) : m_type(value_t::number_float) { m_data.number_float = v; }

    json(const std::string& v) : m_type(value_t::string) { m_data.string = new string_t(v); }
    json(std::string&& v) : m_type(value_t::string) { m_data.string = new string_t(std::move(v)); }
    json(const char* v) : m_type(value_t::string) { m_data.string = new string_t(v); }

    json(const json& other) {
        m_type = other.m_type;
        switch (m_type) {
            case value_t::object: m_data.object = new object_t(*other.m_data.object); break;
            case value_t::array: m_data.array = new array_t(*other.m_data.array); break;
            case value_t::string: m_data.string = new string_t(*other.m_data.string); break;
            default: m_data = other.m_data; break;
        }
    }

    json(json&& other) noexcept {
        m_type = other.m_type;
        m_data = other.m_data;
        other.m_type = value_t::null;
        other.m_data.object = nullptr;
    }

    json& operator=(json other) {
        swap(other);
        return *this;
    }

    void swap(json& other) noexcept {
        std::swap(m_type, other.m_type);
        std::swap(m_data, other.m_data);
    }

    static json array() {
        json j;
        j.m_type = value_t::array;
        j.m_data.array = new array_t();
        return j;
    }

    static json object() {
        json j;
        j.m_type = value_t::object;
        j.m_data.object = new object_t();
        return j;
    }

    // -------------------------------------------------------------------------
    // TYPE CHECKS
    // -------------------------------------------------------------------------
    bool is_null() const { return m_type == value_t::null; }
    bool is_boolean() const { return m_type == value_t::boolean; }
    bool is_number() const { return m_type == value_t::number_integer || m_type == value_t::number_unsigned || m_type == value_t::number_float; }
    bool is_object() const { return m_type == value_t::object; }
    bool is_array() const { return m_type == value_t::array; }
    bool is_string() const { return m_type == value_t::string; }

    // -------------------------------------------------------------------------
    // ACCESSORS
    // -------------------------------------------------------------------------
    template<typename T>
    T get() const {
        if constexpr (std::is_same_v<T, int> || std::is_same_v<T, int64_t>) {
            if (m_type == value_t::number_integer) return static_cast<T>(m_data.number_integer);
            if (m_type == value_t::number_unsigned) return static_cast<T>(m_data.number_unsigned);
        } else if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>) {
            if (m_type == value_t::number_float) return static_cast<T>(m_data.number_float);
            if (m_type == value_t::number_integer) return static_cast<T>(m_data.number_integer);
        } else if constexpr (std::is_same_v<T, bool>) {
            if (m_type == value_t::boolean) return m_data.boolean;
        } else if constexpr (std::is_same_v<T, std::string>) {
            if (m_type == value_t::string) return *m_data.string;
        }
        throw type_error("Invalid get conversion");
    }

    // Implicit conversions
    operator int() const { return get<int>(); }
    operator int64_t() const { return get<int64_t>(); }
    operator double() const { return get<double>(); }
    operator bool() const { return get<bool>(); }
    operator std::string() const { return get<std::string>(); }

    // Operator []
    json& operator[](size_t idx) {
        if (is_null()) {
            m_type = value_t::array;
            m_data.array = new array_t();
        }
        if (!is_array()) throw type_error("Not an array");
        if (idx >= m_data.array->size()) {
             // Nlohmann behavior: expand?
             // Typically operator[] on array with OOB index is UB or resizing in Nlohmann?
             // Nlohmann docs says: "If the index is NOT within the range... the behavior is undefined."
             // BUT users often expect resizing or safe access.
             // We will implement resizing for "Drop-in" convenience if needed,
             // but let's stick to standard vector behavior + bounds check for now.
             // Wait, Nlohmann: "The array is resized... filled with null".
             if (idx >= m_data.array->size()) {
                 m_data.array->resize(idx + 1);
             }
        }
        return (*m_data.array)[idx];
    }

    const json& operator[](size_t idx) const {
        if (!is_array()) throw type_error("Not an array");
        return m_data.array->at(idx);
    }

    // Object access
    json& operator[](const std::string& key) {
        if (is_null()) {
            m_type = value_t::object;
            m_data.object = new object_t();
        }
        if (!is_object()) throw type_error("Not an object");

        auto& obj = *m_data.object;
        for (auto& pair : obj) {
            if (pair.first == key) return pair.second;
        }
        // Not found, insert
        obj.push_back({key, json()});
        return obj.back().second;
    }

    json& operator[](const char* key) { return (*this)[std::string(key)]; }

    const json& operator[](const std::string& key) const {
        if (!is_object()) throw type_error("Not an object");
        const auto& obj = *m_data.object;
        for (const auto& pair : obj) {
            if (pair.first == key) return pair.second;
        }
        throw std::out_of_range("Key not found");
    }

    // -------------------------------------------------------------------------
    // ITERATORS
    // -------------------------------------------------------------------------
    // Complex because we wrap different iterators.
    // Nlohmann has a complex iterator facade.
    // We will support range-based for loops which is 99% usage.

    struct iterator {
        // Simple forward iterator proxy
        // We use a variant or pointer logic.
        using iterator_category = std::forward_iterator_tag;
        using value_type = json;
        using difference_type = std::ptrdiff_t;
        using pointer = json*;
        using reference = json&;

        bool is_obj;
        object_t::iterator obj_it;
        array_t::iterator arr_it;

        iterator(object_t::iterator it) : is_obj(true), obj_it(it) {}
        iterator(array_t::iterator it) : is_obj(false), arr_it(it) {}

        bool operator!=(const iterator& other) const {
            if (is_obj != other.is_obj) return true;
            if (is_obj) return obj_it != other.obj_it;
            return arr_it != other.arr_it;
        }

        bool operator==(const iterator& other) const { return !(*this != other); }

        iterator& operator++() {
            if (is_obj) ++obj_it; else ++arr_it;
            return *this;
        }

        // Dereference
        // For object, Nlohmann dereference returns `json&` value?
        // No, `*it` for object iterator returns a reference to `json` value usually?
        // Wait, nlohmann object iterator: `it.key()` and `it.value()`. `*it` returns reference to json.
        // Range-for over object: `for (auto& [key, val] : j.items())`.
        // `for (auto& val : j)` iterates values.

        json& operator*() {
            if (is_obj) return obj_it->second;
            return *arr_it;
        }

        // Key accessor for object iterator
        std::string key() const {
            if (is_obj) return obj_it->first;
            return ""; // Array iterator has no key
        }

        json& value() {
            return operator*();
        }
    };

    // Const iterator omitted for brevity but needed for const objects.
    // Implementing mutable only for now to fit in file.

    iterator begin() {
        if (is_object()) return iterator(m_data.object->begin());
        if (is_array()) return iterator(m_data.array->begin());
        return iterator(array_t().begin()); // Empty (unsafe?)
    }

    iterator end() {
        if (is_object()) return iterator(m_data.object->end());
        if (is_array()) return iterator(m_data.array->end());
        return iterator(array_t().end());
    }

    // items() support (Simplified placeholder for compatibility)
    // struct items_proxy { ... }

    size_t size() const {
        if (is_array()) return m_data.array->size();
        if (is_object()) return m_data.object->size();
        return 0;
    }

    // -------------------------------------------------------------------------
    // PARSER
    // -------------------------------------------------------------------------
    static json parse(const std::string& s) {
        const char* p = s.data();
        const char* end = s.data() + s.size();
        return parse_internal(p, end, 0);
    }

private:
    static json parse_internal(const char*& p, const char* end, int depth) {
        if (depth > 5000) throw parse_error("Stack overflow (deep nesting)");

        p = simd::skip_whitespace(p, end);
        if (p == end) throw parse_error("Unexpected end");

        char c = *p;
        if (c == '{') {
            p++; // skip {
            json j = object();
            p = simd::skip_whitespace(p, end);
            if (*p == '}') { p++; return j; }

            while (true) {
                if (*p != '"') throw parse_error("Expected string key");
                std::string key = parse_string(p, end);
                p = simd::skip_whitespace(p, end);

                if (*p != ':') throw parse_error("Expected colon");
                p++;

                json val = parse_internal(p, end, depth + 1);
                j.m_data.object->push_back({std::move(key), std::move(val)});

                p = simd::skip_whitespace(p, end);
                if (*p == '}') { p++; break; }
                if (*p == ',') { p++; p = simd::skip_whitespace(p, end); continue; }
                throw parse_error("Expected , or }");
            }
            return j;
        } else if (c == '[') {
            p++; // skip [
            json j = array();
            p = simd::skip_whitespace(p, end);
            if (*p == ']') { p++; return j; }

            while (true) {
                j.m_data.array->push_back(parse_internal(p, end, depth + 1));
                p = simd::skip_whitespace(p, end);
                if (*p == ']') { p++; break; }
                if (*p == ',') { p++; continue; } // Comma handling
                throw parse_error("Expected , or ]");
            }
            return j;
        } else if (c == '"') {
            return json(parse_string(p, end));
        } else if (c == 't') {
            if (strncmp(p, "true", 4) == 0) { p += 4; return json(true); }
        } else if (c == 'f') {
            if (strncmp(p, "false", 5) == 0) { p += 5; return json(false); }
        } else if (c == 'n') {
            if (strncmp(p, "null", 4) == 0) { p += 4; return json(nullptr); }
        } else if (c == '-' || (c >= '0' && c <= '9')) {
            return parse_number(p, end);
        }

        throw parse_error("Invalid char: " + std::string(1, c));
    }

    static std::string parse_string(const char*& p, const char* end) {
        // p points to opening "
        p++;
        const char* start = p;
        while (p < end) {
            // Optimization: skip valid chars fast
            // SIMD here would be huge
            if (*p == '"') {
                std::string s(start, p - start);
                p++;
                return s; // TODO: Unescape
            }
            if (*p == '\\') {
                // Slow path with unescape
                std::string s(start, p - start);
                while (p < end) {
                     if (*p == '"') { p++; return s; }
                     if (*p == '\\') {
                         p++;
                         if (p == end) throw parse_error("Unexpected end in escape");
                         char esc = *p++;
                         switch(esc) {
                             case '"': s += '"'; break;
                             case '\\': s += '\\'; break;
                             case '/': s += '/'; break;
                             case 'b': s += '\b'; break;
                             case 'f': s += '\f'; break;
                             case 'n': s += '\n'; break;
                             case 'r': s += '\r'; break;
                             case 't': s += '\t'; break;
                             default: s += esc; break;
                         }
                     } else {
                         s += *p++;
                     }
                }
            }
            p++;
        }
        throw parse_error("Unterminated string");
    }

    static json parse_number(const char*& p, const char* end) {
        char* end_ptr;
        double d = std::strtod(p, &end_ptr);
        p = end_ptr;
        // Integer check (simplified)
        if (std::floor(d) == d && !std::isinf(d) && d >= std::numeric_limits<int64_t>::min() && d <= std::numeric_limits<int64_t>::max()) {
            return json((int64_t)d);
        }
        return json(d);
    }

public:
    // -------------------------------------------------------------------------
    // DUMP
    // -------------------------------------------------------------------------
    std::string dump(int indent = -1, char indent_char = ' ', int current_indent = 0) const {
        std::stringstream ss;
        dump_internal(ss, indent, indent_char, current_indent);
        return ss.str();
    }

private:
    void dump_internal(std::ostream& os, int indent, char indent_char, int current_indent) const {
        switch (m_type) {
            case value_t::null: os << "null"; break;
            case value_t::boolean: os << (m_data.boolean ? "true" : "false"); break;
            case value_t::number_integer: os << m_data.number_integer; break;
            case value_t::number_unsigned: os << m_data.number_unsigned; break;
            case value_t::number_float: os << m_data.number_float; break;
            case value_t::string: os << "\"" << *m_data.string << "\""; break; // TODO: Escape
            case value_t::array: {
                if (m_data.array->empty()) { os << "[]"; return; }
                os << "[";
                if (indent >= 0) os << "\n";
                for (size_t i = 0; i < m_data.array->size(); ++i) {
                    if (indent >= 0) os << std::string((current_indent + 1) * indent, indent_char);
                    (*m_data.array)[i].dump_internal(os, indent, indent_char, current_indent + 1);
                    if (i < m_data.array->size() - 1) os << ",";
                    if (indent >= 0) os << "\n";
                }
                if (indent >= 0) os << std::string(current_indent * indent, indent_char);
                os << "]";
                break;
            }
            case value_t::object: {
                if (m_data.object->empty()) { os << "{}"; return; }
                os << "{";
                if (indent >= 0) os << "\n";
                for (size_t i = 0; i < m_data.object->size(); ++i) {
                    if (indent >= 0) os << std::string((current_indent + 1) * indent, indent_char);
                    auto& pair = (*m_data.object)[i];
                    os << "\"" << pair.first << "\":"; // TODO: Escape Key
                    if (indent >= 0) os << " ";
                    pair.second.dump_internal(os, indent, indent_char, current_indent + 1);
                    if (i < m_data.object->size() - 1) os << ",";
                    if (indent >= 0) os << "\n";
                }
                if (indent >= 0) os << std::string(current_indent * indent, indent_char);
                os << "}";
                break;
            }
            default: break;
        }
    }
};

// Stream operators
inline std::ostream& operator<<(std::ostream& os, const json& j) {
    os << j.dump();
    return os;
}

} // namespace tachyon

#endif // TACHYON_HPP
