#ifndef TACHYON_HPP
#define TACHYON_HPP

// TACHYON "LEGACY" (C++11)
// The High-Performance JSON Library for Legacy Systems
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

// -----------------------------------------------------------------------------
// MACROS & HELPERS
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
        cpu_features() : avx2(false) {
#ifndef _MSC_VER
            __builtin_cpu_init();
            avx2 = __builtin_cpu_supports("avx2");
#endif
        }
    };
    static const cpu_features g_cpu;

    TACHYON_FORCE_INLINE const char* skip_whitespace(const char* p, const char* end) {
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
        while (p < end && (unsigned char)*p <= 32) p++;
        return p;
    }
}

// -----------------------------------------------------------------------------
// ADL HOOKS
// -----------------------------------------------------------------------------
template<typename T>
void to_json(json& j, const T& t);
template<typename T>
void from_json(const json& j, T& t);

// -----------------------------------------------------------------------------
// JSON CLASS
// -----------------------------------------------------------------------------
class json {
public:
    // Tagged Union
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

    // SFINAE for ADL
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

    static json array() {
        json j; j.m_type = value_t::array; j.m_value.array = new array_t(); return j;
    }
    static json object() {
        json j; j.m_type = value_t::object; j.m_value.object = new object_t(); return j;
    }

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
        if (m_type == value_t::boolean) return static_cast<T>(m_value.boolean);
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

    // Implicit conversions
    operator int() const { return get<int>(); }
    operator int64_t() const { return get<int64_t>(); }
    operator double() const { return get<double>(); }
    operator std::string() const { return get<std::string>(); }
    operator bool() const { return get<bool>(); }

    // Operator []
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

    // Items proxy for iteration: for (auto& item : j.items())
    struct items_proxy {
        const json& j;
        struct item_iterator {
            bool is_obj;
            object_t::const_iterator obj_it;
            array_t::const_iterator arr_it;
            size_t idx;

            item_iterator(object_t::const_iterator it) : is_obj(true), obj_it(it), idx(0) {}
            item_iterator(array_t::const_iterator it) : is_obj(false), arr_it(it), idx(0) {}

            bool operator!=(const item_iterator& other) const {
                if (is_obj != other.is_obj) return true;
                if (is_obj) return obj_it != other.obj_it;
                return arr_it != other.arr_it;
            }
            void operator++() { if (is_obj) ++obj_it; else { ++arr_it; ++idx; } }

            // Nlohmann items() iterator returns an object with .key() and .value()
            struct entry {
                std::string k;
                const json& v;
                std::string key() const { return k; }
                const json& value() const { return v; }
            };

            entry operator*() {
                if (is_obj) return {obj_it->first, obj_it->second};
                return {std::to_string(idx), *arr_it};
            }
        };

        item_iterator begin() const {
            if (j.is_object()) return item_iterator(j.m_value.object->begin());
            return item_iterator(j.m_value.array->begin());
        }
        item_iterator end() const {
            if (j.is_object()) return item_iterator(j.m_value.object->end());
            return item_iterator(j.m_value.array->end());
        }
    };

    items_proxy items() const { return items_proxy{*this}; }

    // Parse
    static json parse(const std::string& s) {
        const char* p = s.data();
        const char* end = s.data() + s.size();
        return parse_recursive(p, end, 0);
    }

private:
    static json parse_recursive(const char*& p, const char* end, int depth) {
        if (depth > 1000) throw parse_error("Deep nesting");

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
        char* end_ptr;
        // Optimization: Fast Integer Path
        bool is_float = false;
        const char* temp = p;
        if (*temp == '-') temp++;
        while (temp < end && (*temp >= '0' && *temp <= '9')) temp++;
        if (temp < end && (*temp == '.' || *temp == 'e' || *temp == 'E')) is_float = true;

        if (!is_float) {
            int64_t val = std::strtoll(p, &end_ptr, 10);
            p = end_ptr;
            return json(val);
        } else {
            double val = std::strtod(p, &end_ptr);
            p = end_ptr;
            return json(val);
        }
    }

public:
    std::string dump(int indent = -1) const {
        std::stringstream ss;
        dump_internal(ss, indent, 0);
        return ss.str();
    }

private:
    void dump_internal(std::stringstream& ss, int indent, int current) const {
        switch(m_type) {
            case value_t::null: ss << "null"; break;
            case value_t::boolean: ss << (m_value.boolean ? "true" : "false"); break;
            case value_t::number_integer: ss << m_value.number_integer; break;
            case value_t::number_unsigned: ss << m_value.number_unsigned; break;
            case value_t::number_float: ss << m_value.number_float; break;
            case value_t::string: ss << "\"" << *m_value.string << "\""; break;
            case value_t::array:
                if (m_value.array->empty()) { ss << "[]"; return; }
                ss << "[";
                for (size_t i=0; i<m_value.array->size(); ++i) {
                    if (i>0) ss << ",";
                    if(indent>=0) ss << " ";
                    (*m_value.array)[i].dump_internal(ss, indent, current+1);
                }
                ss << "]";
                break;
            case value_t::object:
                if (m_value.object->empty()) { ss << "{}"; return; }
                ss << "{";
                for (size_t i=0; i<m_value.object->size(); ++i) {
                    if (i>0) ss << ",";
                    if(indent>=0) ss << " ";
                    ss << "\"" << (*m_value.object)[i].first << "\":";
                    if(indent>=0) ss << " ";
                    (*m_value.object)[i].second.dump_internal(ss, indent, current+1);
                }
                ss << "}";
                break;
            default: break;
        }
    }
};

inline std::ostream& operator<<(std::ostream& os, const json& j) { os << j.dump(); return os; }

} // namespace tachyon

// Drop-in compatibility alias
#ifndef TACHYON_SKIP_NLOHMANN_ALIAS
namespace nlohmann = tachyon;
#endif

#endif // TACHYON_HPP
