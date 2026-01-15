#ifndef TACHYON_HPP
#define TACHYON_HPP

// TACHYON v8.0 "SUPERNOVA"
// The Ultimate Hybrid JSON Library (C++11/C++17)
// (C) 2026 Tachyon Systems
// License: MIT

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <memory>
#include <map>
#include <initializer_list>
#include <functional>
#include <type_traits>
#include <sstream>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <utility>
#include <iterator>
#include <limits>
#include <cassert>

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#include <cpuid.h>
#endif

// Hybrid Number Parsing Headers
#if __cplusplus >= 201703L
#include <charconv>
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
// STRING VIEW (C++11)
// -----------------------------------------------------------------------------
class string_view {
    const char* m_data;
    size_t m_size;
public:
    string_view() : m_data(nullptr), m_size(0) {}
    string_view(const char* data, size_t size) : m_data(data), m_size(size) {}
    string_view(const char* data) : m_data(data), m_size(data ? std::strlen(data) : 0) {}
    string_view(const std::string& s) : m_data(s.data()), m_size(s.size()) {}

    const char* data() const { return m_data; }
    size_t size() const { return m_size; }
    const char* begin() const { return m_data; }
    const char* end() const { return m_data + m_size; }
    char operator[](size_t i) const { return m_data[i]; }

    std::string to_string() const { return std::string(m_data, m_size); }
};

inline bool operator==(const string_view& lhs, const string_view& rhs) {
    return lhs.size() == rhs.size() && std::memcmp(lhs.data(), rhs.data(), lhs.size()) == 0;
}
inline bool operator!=(const string_view& lhs, const string_view& rhs) { return !(lhs == rhs); }

inline std::ostream& operator<<(std::ostream& os, const string_view& sv) {
    os.write(sv.data(), sv.size());
    return os;
}

// -----------------------------------------------------------------------------
// EXCEPTIONS
// -----------------------------------------------------------------------------
class exception : public std::exception {
    std::string m_msg;
public:
    exception(const std::string& msg) : m_msg(msg) {}
    virtual const char* what() const noexcept { return m_msg.c_str(); }
    virtual ~exception() noexcept {}
};

class parse_error : public exception {
public:
    parse_error(const std::string& msg) : exception("[parse_error] " + msg) {}
};

class type_error : public exception {
public:
    type_error(const std::string& msg) : exception("[type_error] " + msg) {}
};

// -----------------------------------------------------------------------------
// FORWARD DECLS & TYPES
// -----------------------------------------------------------------------------
class json;

using string_t = std::string;
using number_integer_t = int64_t;
using number_unsigned_t = uint64_t;
using number_float_t = double;
using boolean_t = bool;
using object_t = std::vector<std::pair<string_t, json>>;
using array_t = std::vector<json>;

enum class value_t : uint8_t {
    null, object, array, string, boolean, number_integer, number_unsigned, number_float, discarded
};

// -----------------------------------------------------------------------------
// SIMD (C++11 Compatible)
// -----------------------------------------------------------------------------
namespace simd {
    struct cpu_features {
        bool avx2;
        bool avx512;
        cpu_features() : avx2(false), avx512(false) {
#ifndef _MSC_VER
            __builtin_cpu_init();
            avx2 = __builtin_cpu_supports("avx2");
            avx512 = __builtin_cpu_supports("avx512f") && __builtin_cpu_supports("avx512bw");
#endif
        }
    };
    static const cpu_features g_cpu;

    TACHYON_FORCE_INLINE const char* skip_whitespace(const char* p, const char* end) {
#if defined(__AVX512F__) && defined(__AVX512BW__)
        if (g_cpu.avx512 && p + 64 <= end) {
             const __m512i v_space = _mm512_set1_epi8(' ');
             const __m512i v_tab = _mm512_set1_epi8('\t');
             const __m512i v_lf = _mm512_set1_epi8('\n');
             const __m512i v_cr = _mm512_set1_epi8('\r');
             while (p + 64 <= end) {
                 __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(p));
                 __mmask64 m = _mm512_cmpeq_epi8_mask(chunk, v_space) | _mm512_cmpeq_epi8_mask(chunk, v_tab) |
                               _mm512_cmpeq_epi8_mask(chunk, v_lf) | _mm512_cmpeq_epi8_mask(chunk, v_cr);
                 if (m != 0xFFFFFFFFFFFFFFFF) {
                     return p + __builtin_ctzll(~m);
                 }
                 p += 64;
             }
             _mm256_zeroupper();
        }
#endif
#if defined(__AVX2__)
        if (g_cpu.avx2 && p + 32 <= end) {
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
                if ((unsigned int)res != 0xFFFFFFFF) {
                    int non_ws = ~res;
                    return p + __builtin_ctz(non_ws);
                }
                p += 32;
            }
        }
#endif
        while (p < end && (unsigned char)*p <= 32) p++;
        return p;
    }
}

// -----------------------------------------------------------------------------
// ADL HOOKS
// -----------------------------------------------------------------------------
template<typename T> void to_json(json& j, const T& t);
template<typename T> void from_json(const json& j, T& t);

// -----------------------------------------------------------------------------
// JSON CLASS
// -----------------------------------------------------------------------------
class json {
public:
    union json_value {
        object_t* object;
        array_t* array;
        string_t* string;
        boolean_t boolean;
        number_integer_t number_integer;
        number_unsigned_t number_unsigned;
        number_float_t number_float;

        json_value() : object(nullptr) {}
        json_value(boolean_t v) : boolean(v) {}
        json_value(number_integer_t v) : number_integer(v) {}
        json_value(number_unsigned_t v) : number_unsigned(v) {}
        json_value(number_float_t v) : number_float(v) {}
    };

    value_t m_type;
    json_value m_value;

    void destroy() {
        switch (m_type) {
            case value_t::object: delete m_value.object; break;
            case value_t::array: delete m_value.array; break;
            case value_t::string: delete m_value.string; break;
            default: break;
        }
        m_type = value_t::null;
        m_value.object = nullptr;
    }

public:
    // Constructors
    json() : m_type(value_t::null) {}
    json(std::nullptr_t) : m_type(value_t::null) {}
    json(boolean_t v) : m_type(value_t::boolean), m_value(v) {}
    json(int v) : m_type(value_t::number_integer), m_value((number_integer_t)v) {}
    json(int64_t v) : m_type(value_t::number_integer), m_value(v) {}
    json(size_t v) : m_type(value_t::number_unsigned), m_value((number_unsigned_t)v) {}
    json(double v) : m_type(value_t::number_float), m_value(v) {}

    json(const std::string& v) : m_type(value_t::string) { m_value.string = new string_t(v); }
    json(const char* v) : m_type(value_t::string) { m_value.string = new string_t(v); }

    template <typename T, typename std::enable_if<!std::is_convertible<T*, json*>::value &&
                                                  !std::is_convertible<T*, const char*>::value &&
                                                  !std::is_same<T, std::string>::value &&
                                                  !std::is_arithmetic<T>::value, int>::type = 0>
    json(const T& t) : m_type(value_t::null) {
        to_json(*this, t);
    }

    json(const json& other) : m_type(other.m_type) {
        switch (m_type) {
            case value_t::object: m_value.object = new object_t(*other.m_value.object); break;
            case value_t::array: m_value.array = new array_t(*other.m_value.array); break;
            case value_t::string: m_value.string = new string_t(*other.m_value.string); break;
            default: m_value = other.m_value; break;
        }
    }

    json(json&& other) noexcept : m_type(other.m_type), m_value(other.m_value) {
        other.m_type = value_t::null;
        other.m_value.object = nullptr;
    }

    ~json() { destroy(); }

    json& operator=(json other) {
        std::swap(m_type, other.m_type);
        std::swap(m_value, other.m_value);
        return *this;
    }

    static json array() { json j; j.m_type = value_t::array; j.m_value.array = new array_t(); return j; }
    static json object() { json j; j.m_type = value_t::object; j.m_value.object = new object_t(); return j; }

    // Accessors
    bool is_null() const { return m_type == value_t::null; }
    bool is_boolean() const { return m_type == value_t::boolean; }
    bool is_number() const { return m_type == value_t::number_integer || m_type == value_t::number_unsigned || m_type == value_t::number_float; }
    bool is_string() const { return m_type == value_t::string; }
    bool is_array() const { return m_type == value_t::array; }
    bool is_object() const { return m_type == value_t::object; }

    template<typename T>
    typename std::enable_if<std::is_arithmetic<T>::value && !std::is_same<T, bool>::value, T>::type get() const {
        if (m_type == value_t::number_integer) return static_cast<T>(m_value.number_integer);
        if (m_type == value_t::number_unsigned) return static_cast<T>(m_value.number_unsigned);
        if (m_type == value_t::number_float) return static_cast<T>(m_value.number_float);
        throw type_error("Not a number");
    }

    template<typename T>
    typename std::enable_if<std::is_same<T, std::string>::value, T>::type get() const {
        if (m_type == value_t::string) return *m_value.string;
        throw type_error("Not a string");
    }

    template<typename T>
    typename std::enable_if<std::is_same<T, bool>::value, T>::type get() const {
        if (m_type == value_t::boolean) return m_value.boolean;
        throw type_error("Not a boolean");
    }

    template<typename T>
    typename std::enable_if<!std::is_arithmetic<T>::value && !std::is_same<T, std::string>::value && !std::is_same<T, bool>::value, T>::type get() const {
        T t;
        from_json(*this, t);
        return t;
    }

    operator int() const { return get<int>(); }
    operator int64_t() const { return get<int64_t>(); }
    operator double() const { return get<double>(); }
    operator std::string() const { return get<std::string>(); }
    operator bool() const { return get<bool>(); }

    json& operator[](size_t idx) {
        if (m_type == value_t::null) { m_type = value_t::array; m_value.array = new array_t(); }
        if (m_type != value_t::array) throw type_error("Not an array");
        if (idx >= m_value.array->size()) m_value.array->resize(idx + 1);
        return (*m_value.array)[idx];
    }
    const json& operator[](size_t idx) const {
        if (m_type != value_t::array) throw type_error("Not an array");
        return m_value.array->at(idx);
    }
    json& operator[](int idx) { return (*this)[static_cast<size_t>(idx)]; }
    const json& operator[](int idx) const { return (*this)[static_cast<size_t>(idx)]; }

    json& operator[](const std::string& key) {
        if (m_type == value_t::null) { m_type = value_t::object; m_value.object = new object_t(); }
        if (m_type != value_t::object) throw type_error("Not an object");
        for (auto& pair : *m_value.object) {
            if (pair.first == key) return pair.second;
        }
        m_value.object->push_back(std::make_pair(key, json()));
        return m_value.object->back().second;
    }
    json& operator[](const char* key) { return (*this)[std::string(key)]; }

    const json& operator[](const std::string& key) const {
        if (m_type != value_t::object) throw type_error("Not an object");
        for (const auto& pair : *m_value.object) {
            if (pair.first == key) return pair.second;
        }
        throw std::out_of_range("Key not found");
    }
    const json& operator[](const char* key) const { return (*this)[std::string(key)]; }

    size_t size() const {
        if (m_type == value_t::array) return m_value.array->size();
        if (m_type == value_t::object) return m_value.object->size();
        return 0;
    }

    // Iterators
    struct iterator {
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
        iterator& operator++() { if (is_obj) ++obj_it; else ++arr_it; return *this; }
        json& operator*() { if (is_obj) return obj_it->second; return *arr_it; }
    };

    iterator begin() {
        if (m_type == value_t::object) return iterator(m_value.object->begin());
        if (m_type == value_t::array) return iterator(m_value.array->begin());
        return iterator(array_t().begin());
    }
    iterator end() {
        if (m_type == value_t::object) return iterator(m_value.object->end());
        if (m_type == value_t::array) return iterator(m_value.array->end());
        return iterator(array_t().end());
    }

    // items() proxy for structured binding iteration (C++17) or pair iteration
    // Nlohmann returns an iterable that yields proxy objects with .key() and .value()
    // For drop-in, we simulate this.
    struct item_proxy {
        std::string key() const { return k; }
        json& value() { return v; }
        std::string k;
        json& v;
    };

    struct items_view {
        json& j;
        struct iterator {
            bool is_obj;
            object_t::iterator obj_it;
            array_t::iterator arr_it;
            size_t idx;

            iterator(object_t::iterator it) : is_obj(true), obj_it(it), idx(0) {}
            iterator(array_t::iterator it) : is_obj(false), arr_it(it), idx(0) {}

            bool operator!=(const iterator& other) const {
                if (is_obj != other.is_obj) return true;
                if (is_obj) return obj_it != other.obj_it;
                return arr_it != other.arr_it;
            }
            void operator++() { if (is_obj) ++obj_it; else { ++arr_it; ++idx; } }

            item_proxy operator*() {
                if (is_obj) return {obj_it->first, obj_it->second};
                return {std::to_string(idx), *arr_it};
            }
        };
        iterator begin() {
            if (j.is_object()) return iterator(j.m_value.object->begin());
            return iterator(j.m_value.array->begin());
        }
        iterator end() {
            if (j.is_object()) return iterator(j.m_value.object->end());
            return iterator(j.m_value.array->end());
        }
    };
    items_view items() { return items_view{*this}; }

    // Parse
    static json parse(const std::string& s) {
        const char* p = s.data();
        const char* end = s.data() + s.size();
        return parse_recursive(p, end, 0);
    }

private:
    static json parse_recursive(const char*& p, const char* end, int depth) {
        if (depth > 2000) throw parse_error("Deep nesting");

        p = simd::skip_whitespace(p, end);
        if (p == end) throw parse_error("Unexpected end");

        char c = *p;
        if (c == '{') {
            p++;
            json j = object();
            p = simd::skip_whitespace(p, end);
            if (*p == '}') { p++; return j; }
            while(true) {
                if (*p != '"') throw parse_error("Expected string key");
                std::string key = parse_string(p, end);
                p = simd::skip_whitespace(p, end);
                if (*p != ':') throw parse_error("Expected colon");
                p++;
                json val = parse_recursive(p, end, depth + 1);
                j.m_value.object->push_back(std::make_pair(std::move(key), std::move(val)));
                p = simd::skip_whitespace(p, end);
                if (*p == '}') { p++; break; }
                if (*p == ',') { p++; p = simd::skip_whitespace(p, end); continue; }
                throw parse_error("Expected , or }");
            }
            return j;
        } else if (c == '[') {
            p++;
            json j = array();
            p = simd::skip_whitespace(p, end);
            if (*p == ']') { p++; return j; }
            while(true) {
                j.m_value.array->push_back(parse_recursive(p, end, depth + 1));
                p = simd::skip_whitespace(p, end);
                if (*p == ']') { p++; break; }
                if (*p == ',') { p++; continue; }
                throw parse_error("Expected , or ]");
            }
            return j;
        } else if (c == '"') {
            return json(parse_string(p, end));
        } else if (c == 't') {
            if (p+4 <= end && std::memcmp(p, "true", 4) == 0) { p += 4; return json(true); }
        } else if (c == 'f') {
            if (p+5 <= end && std::memcmp(p, "false", 5) == 0) { p += 5; return json(false); }
        } else if (c == 'n') {
            if (p+4 <= end && std::memcmp(p, "null", 4) == 0) { p += 4; return json(nullptr); }
        } else if (c == '-' || (c >= '0' && c <= '9')) {
            return parse_number(p, end);
        }
        throw parse_error("Invalid syntax");
    }

    static std::string parse_string(const char*& p, const char* end) {
        p++; // skip "
        const char* start = p;
        while (p < end) {
            if (*p == '"') {
                std::string s(start, p - start);
                p++;
                return s;
            }
            if (*p == '\\') {
                std::string s(start, p - start);
                while (p < end) {
                    if (*p == '"') { p++; return s; }
                    if (*p == '\\') {
                        p++;
                        char esc = *p++;
                        if(esc=='"') s+='"'; else if(esc=='\\') s+='\\'; else if(esc=='/') s+='/';
                        else if(esc=='b') s+='\b'; else if(esc=='f') s+='\f'; else if(esc=='n') s+='\n';
                        else if(esc=='r') s+='\r'; else if(esc=='t') s+='\t'; else s+=esc;
                    } else s += *p++;
                }
            }
            p++;
        }
        throw parse_error("Unterm string");
    }

    static json parse_number(const char*& p, const char* end) {
        // HYBRID NUMBER PARSING
        const char* start = p;
        bool is_float = false;
        // Scan
        if (*p == '-') p++;
        while (p < end && (*p >= '0' && *p <= '9')) p++;
        if (p < end && (*p == '.' || *p == 'e' || *p == 'E')) {
            is_float = true;
            if (*p == '.') {
                p++;
                while (p < end && (*p >= '0' && *p <= '9')) p++;
            }
            if (p < end && (*p == 'e' || *p == 'E')) {
                p++;
                if (p < end && (*p == '+' || *p == '-')) p++;
                while (p < end && (*p >= '0' && *p <= '9')) p++;
            }
        }

#if __cplusplus >= 201703L
        // C++17 Fast Path
        if (is_float) {
            double res;
            auto r = std::from_chars(start, p, res);
            if (r.ec == std::errc()) return json(res);
        } else {
            int64_t res;
            auto r = std::from_chars(start, p, res);
            if (r.ec == std::errc()) return json(res);
        }
        // Fallback or error?
        return json(0);
#else
        // C++11 Legacy Path
        char* end_ptr;
        if (is_float) {
            double res = std::strtod(start, &end_ptr);
            return json(res);
        } else {
            int64_t res = std::strtoll(start, &end_ptr, 10);
            return json(res);
        }
#endif
    }

public:
    std::string dump(int indent = -1) const {
        std::string s;
        if (m_type == value_t::array || m_type == value_t::object) s.reserve(256);
        dump_internal(s, indent, 0);
        return s;
    }

private:
    void dump_internal(std::string& s, int indent, int current) const {
        switch(m_type) {
            case value_t::null: s += "null"; break;
            case value_t::boolean: s += (m_value.boolean ? "true" : "false"); break;
            case value_t::number_integer: {
                char buf[32];
#if __cplusplus >= 201703L
                auto r = std::to_chars(buf, buf + 32, m_value.number_integer);
                s.append(buf, r.ptr - buf);
#else
                s += std::to_string(m_value.number_integer);
#endif
                break;
            }
            case value_t::number_unsigned: {
                char buf[32];
#if __cplusplus >= 201703L
                auto r = std::to_chars(buf, buf + 32, m_value.number_unsigned);
                s.append(buf, r.ptr - buf);
#else
                s += std::to_string(m_value.number_unsigned);
#endif
                break;
            }
            case value_t::number_float: {
                char buf[64];
#if __cplusplus >= 201703L
                auto r = std::to_chars(buf, buf + 64, m_value.number_float);
                s.append(buf, r.ptr - buf);
#else
                s += std::to_string(m_value.number_float);
#endif
                break;
            }
            case value_t::string: s += "\""; s += *m_value.string; s += "\""; break;
            case value_t::array:
                if (m_value.array->empty()) { s += "[]"; return; }
                s += "[";
                for (size_t i=0; i<m_value.array->size(); ++i) {
                    if (i>0) s += ",";
                    if(indent>=0) s += " ";
                    (*m_value.array)[i].dump_internal(s, indent, current+1);
                }
                s += "]";
                break;
            case value_t::object:
                if (m_value.object->empty()) { s += "{}"; return; }
                s += "{";
                for (size_t i=0; i<m_value.object->size(); ++i) {
                    if (i>0) s += ",";
                    if(indent>=0) s += " ";
                    s += "\""; s += (*m_value.object)[i].first; s += "\":";
                    if(indent>=0) s += " ";
                    (*m_value.object)[i].second.dump_internal(s, indent, current+1);
                }
                s += "}";
                break;
            default: break;
        }
    }
};

inline std::ostream& operator<<(std::ostream& os, const json& j) { os << j.dump(); return os; }

// COMPARISON OPERATORS
inline bool operator==(const json& lhs, const json& rhs) { return lhs.dump() == rhs.dump(); } // Slow check
inline bool operator!=(const json& lhs, const json& rhs) { return !(lhs == rhs); }

inline bool operator==(const json& lhs, std::nullptr_t) { return lhs.is_null(); }
inline bool operator==(std::nullptr_t, const json& rhs) { return rhs.is_null(); }
inline bool operator!=(const json& lhs, std::nullptr_t) { return !lhs.is_null(); }
inline bool operator!=(std::nullptr_t, const json& rhs) { return !rhs.is_null(); }

inline bool operator==(const json& lhs, bool rhs) { return lhs.is_boolean() && (bool)lhs == rhs; }
inline bool operator==(bool lhs, const json& rhs) { return rhs == lhs; }
inline bool operator!=(const json& lhs, bool rhs) { return !(lhs == rhs); }
inline bool operator!=(bool lhs, const json& rhs) { return !(lhs == rhs); }

inline bool operator==(const json& lhs, const char* rhs) { return lhs.is_string() && (std::string)lhs == rhs; }
inline bool operator==(const char* lhs, const json& rhs) { return rhs == lhs; }
inline bool operator!=(const json& lhs, const char* rhs) { return !(lhs == rhs); }
inline bool operator!=(const char* lhs, const json& rhs) { return !(lhs == rhs); }

inline bool operator==(const json& lhs, const std::string& rhs) { return lhs.is_string() && (std::string)lhs == rhs; }
inline bool operator==(const std::string& lhs, const json& rhs) { return rhs == lhs; }
inline bool operator!=(const json& lhs, const std::string& rhs) { return !(lhs == rhs); }
inline bool operator!=(const std::string& lhs, const json& rhs) { return !(lhs == rhs); }

template<typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
inline bool operator==(const json& lhs, T rhs) {
    if (lhs.is_number()) {
        if (lhs.m_type == value_t::number_float) return lhs.get<double>() == (double)rhs;
        if (lhs.m_type == value_t::number_integer) return lhs.get<int64_t>() == (int64_t)rhs;
        if (lhs.m_type == value_t::number_unsigned) return lhs.get<uint64_t>() == (uint64_t)rhs;
    }
    return false;
}
template<typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
inline bool operator==(T lhs, const json& rhs) { return rhs == lhs; }
template<typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
inline bool operator!=(const json& lhs, T rhs) { return !(lhs == rhs); }
template<typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
inline bool operator!=(T lhs, const json& rhs) { return !(lhs == rhs); }

} // namespace tachyon

// Drop-in compatibility alias
#ifndef TACHYON_SKIP_NLOHMANN_ALIAS
namespace nlohmann = tachyon;
#endif

#endif // TACHYON_HPP
