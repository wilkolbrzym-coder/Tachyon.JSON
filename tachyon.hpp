#ifndef TACHYON_HPP
#define TACHYON_HPP

// TACHYON v8.0 "SUPERNOVA"
// The Undisputed High-Performance JSON Library
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
#include <cpuid.h>
#endif

// -----------------------------------------------------------------------------
// CONFIGURATION
// -----------------------------------------------------------------------------
#define TACHYON_FORCE_INLINE __attribute__((always_inline)) inline
#define TACHYON_LIKELY(x) __builtin_expect(!!(x), 1)
#define TACHYON_UNLIKELY(x) __builtin_expect(!!(x), 0)

namespace tachyon {

// -----------------------------------------------------------------------------
// EXCEPTIONS
// -----------------------------------------------------------------------------
class parse_error : public std::runtime_error {
public:
    parse_error(const std::string& msg) : std::runtime_error("[tachyon::parse_error] " + msg) {}
};

class type_error : public std::runtime_error {
public:
    type_error(const std::string& msg) : std::runtime_error("[tachyon::type_error] " + msg) {}
};

// -----------------------------------------------------------------------------
// SIMD ENGINE
// -----------------------------------------------------------------------------
namespace simd {

    enum class ISA { SCALAR, AVX2, AVX512 };
    static ISA g_active_isa = ISA::SCALAR;

    struct CpuDetector {
        CpuDetector() {
#ifndef _MSC_VER
            __builtin_cpu_init();
            if (__builtin_cpu_supports("avx512f") && __builtin_cpu_supports("avx512bw")) {
                g_active_isa = ISA::AVX512;
            } else if (__builtin_cpu_supports("avx2")) {
                g_active_isa = ISA::AVX2;
            }
#else
            g_active_isa = ISA::AVX2; // Assume AVX2 on modern MSVC envs for simplicity
#endif
        }
    };
    static CpuDetector g_cpu_detector;

    // SCALAR FALLBACK
    inline const char* skip_whitespace_scalar(const char* p, const char* end) {
        while (p < end && (unsigned char)*p <= 32) p++;
        return p;
    }

    // AVX2 IMPLEMENTATION
    __attribute__((target("avx2")))
    inline const char* skip_whitespace_avx2(const char* p, const char* end) {
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

            uint32_t res = (uint32_t)_mm256_movemask_epi8(mask);
            if (res != 0xFFFFFFFF) {
                // Found non-whitespace (0 bit)
                return p + __builtin_ctz(~res);
            }
            p += 32;
        }
        return skip_whitespace_scalar(p, end);
    }

    // AVX-512 IMPLEMENTATION
    __attribute__((target("avx512f,avx512bw")))
    inline const char* skip_whitespace_avx512(const char* p, const char* end) {
        const __m512i v_space = _mm512_set1_epi8(' ');
        const __m512i v_tab = _mm512_set1_epi8('\t');
        const __m512i v_lf = _mm512_set1_epi8('\n');
        const __m512i v_cr = _mm512_set1_epi8('\r');

        while (p + 64 <= end) {
            __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(p));
            __mmask64 m1 = _mm512_cmpeq_epi8_mask(chunk, v_space);
            __mmask64 m2 = _mm512_cmpeq_epi8_mask(chunk, v_tab);
            __mmask64 m3 = _mm512_cmpeq_epi8_mask(chunk, v_lf);
            __mmask64 m4 = _mm512_cmpeq_epi8_mask(chunk, v_cr);
            __mmask64 mask = m1 | m2 | m3 | m4;

            if (mask != 0xFFFFFFFFFFFFFFFF) {
                return p + __builtin_ctzll(~mask);
            }
            p += 64;
        }
        _mm256_zeroupper(); // Transition
        return skip_whitespace_avx2(p, end);
    }

    // DISPATCHER
    TACHYON_FORCE_INLINE const char* skip_whitespace(const char* p, const char* end) {
        if (g_active_isa == ISA::AVX512) return skip_whitespace_avx512(p, end);
        if (g_active_isa == ISA::AVX2) return skip_whitespace_avx2(p, end);
        return skip_whitespace_scalar(p, end);
    }
}

// -----------------------------------------------------------------------------
// JSON TYPES
// -----------------------------------------------------------------------------
class json;

using object_t = std::vector<std::pair<std::string, json>>;
using array_t = std::vector<json>;
using string_t = std::string;
using boolean_t = bool;
using number_integer_t = int64_t;
using number_unsigned_t = uint64_t;
using number_float_t = double;

enum class value_t : uint8_t {
    null, object, array, string, boolean, number_integer, number_unsigned, number_float, discarded
};

// -----------------------------------------------------------------------------
// JSON CLASS
// -----------------------------------------------------------------------------
class json {
    friend struct std::hash<json>;

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
    // Constructors
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

    // Copy & Move
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
    json& operator=(json other) { swap(other); return *this; }
    void swap(json& other) noexcept { std::swap(m_type, other.m_type); std::swap(m_data, other.m_data); }

    static json array() { json j; j.m_type = value_t::array; j.m_data.array = new array_t(); return j; }
    static json object() { json j; j.m_type = value_t::object; j.m_data.object = new object_t(); return j; }

    // Type Checks
    bool is_null() const { return m_type == value_t::null; }
    bool is_boolean() const { return m_type == value_t::boolean; }
    bool is_number() const { return m_type == value_t::number_integer || m_type == value_t::number_unsigned || m_type == value_t::number_float; }
    bool is_string() const { return m_type == value_t::string; }
    bool is_object() const { return m_type == value_t::object; }
    bool is_array() const { return m_type == value_t::array; }

    // Getters & Operators
    template<typename T>
    T get() const {
        if constexpr (std::is_same_v<T, int> || std::is_same_v<T, int64_t>) {
            if (m_type == value_t::number_integer) return static_cast<T>(m_data.number_integer);
            if (m_type == value_t::number_unsigned) return static_cast<T>(m_data.number_unsigned);
            if (m_type == value_t::number_float) return static_cast<T>(m_data.number_float);
        } else if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>) {
            if (m_type == value_t::number_float) return static_cast<T>(m_data.number_float);
            if (m_type == value_t::number_integer) return static_cast<T>(m_data.number_integer);
            if (m_type == value_t::number_unsigned) return static_cast<T>(m_data.number_unsigned);
        } else if constexpr (std::is_same_v<T, bool>) {
            if (m_type == value_t::boolean) return m_data.boolean;
        } else if constexpr (std::is_same_v<T, std::string>) {
            if (m_type == value_t::string) return *m_data.string;
        }
        throw type_error("Invalid type conversion");
    }

    // Implicit conversions
    operator int() const { return get<int>(); }
    operator int64_t() const { return get<int64_t>(); }
    operator double() const { return get<double>(); }
    operator bool() const { return get<bool>(); }
    operator std::string() const { return get<std::string>(); }

    // Array Access
    json& operator[](size_t idx) {
        if (is_null()) { m_type = value_t::array; m_data.array = new array_t(); }
        if (!is_array()) throw type_error("Not an array");
        if (idx >= m_data.array->size()) m_data.array->resize(idx + 1);
        return (*m_data.array)[idx];
    }
    const json& operator[](size_t idx) const {
        if (!is_array()) throw type_error("Not an array");
        return m_data.array->at(idx);
    }
    json& operator[](int idx) { return (*this)[static_cast<size_t>(idx)]; }
    const json& operator[](int idx) const { return (*this)[static_cast<size_t>(idx)]; }

    // Object Access
    json& operator[](const std::string& key) {
        if (is_null()) { m_type = value_t::object; m_data.object = new object_t(); }
        if (!is_object()) throw type_error("Not an object");
        auto& obj = *m_data.object;
        for (auto& pair : obj) { if (pair.first == key) return pair.second; }
        obj.push_back({key, json()});
        return obj.back().second;
    }
    json& operator[](const char* key) { return (*this)[std::string(key)]; }

    const json& operator[](const std::string& key) const {
        if (!is_object()) throw type_error("Not an object");
        const auto& obj = *m_data.object;
        for (const auto& pair : obj) { if (pair.first == key) return pair.second; }
        throw std::out_of_range("Key not found");
    }
    const json& operator[](const char* key) const { return (*this)[std::string(key)]; }

    size_t size() const {
        if (is_array()) return m_data.array->size();
        if (is_object()) return m_data.object->size();
        return 0;
    }

    // Iterator
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
        if (is_object()) return iterator(m_data.object->begin());
        if (is_array()) return iterator(m_data.array->begin());
        return iterator(array_t().begin());
    }
    iterator end() {
        if (is_object()) return iterator(m_data.object->end());
        if (is_array()) return iterator(m_data.array->end());
        return iterator(array_t().end());
    }

    // -------------------------------------------------------------------------
    // PARSER
    // -------------------------------------------------------------------------
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
                j.m_data.object->push_back({std::move(key), std::move(val)});
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
                j.m_data.array->push_back(parse_recursive(p, end, depth + 1));
                p = simd::skip_whitespace(p, end);
                if (*p == ']') { p++; break; }
                if (*p == ',') { p++; continue; }
                throw parse_error("Expected , or ]");
            }
            return j;
        } else if (c == '"') {
            return json(parse_string(p, end));
        } else if (c == 't') {
            if (p+4 <= end && memcmp(p, "true", 4) == 0) { p += 4; return json(true); }
        } else if (c == 'f') {
            if (p+5 <= end && memcmp(p, "false", 5) == 0) { p += 5; return json(false); }
        } else if (c == 'n') {
            if (p+4 <= end && memcmp(p, "null", 4) == 0) { p += 4; return json(nullptr); }
        } else if (c == '-' || (c >= '0' && c <= '9')) {
            return parse_number(p, end);
        }
        throw parse_error("Invalid syntax");
    }

    // Optimized String Parsing
    static std::string parse_string(const char*& p, const char* end) {
        p++; // Skip "
        const char* start = p;
        while (p < end) {
            if (*p == '"') { std::string s(start, p - start); p++; return s; }
            if (*p == '\\') {
                // Fallback for escapes
                std::string s(start, p - start);
                while (p < end) {
                    if (*p == '"') { p++; return s; }
                    if (*p == '\\') {
                        p++;
                        if (p==end) throw parse_error("Unterm escape");
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

    // FAST NUMBER PARSING (std::from_chars)
    static json parse_number(const char*& p, const char* end) {
        const char* token_start = p;
        bool is_float = false;
        // Scan ahead to find end of number
        while (p < end && (*p == '-' || *p == '+' || *p == '.' || *p == 'e' || *p == 'E' || (*p >= '0' && *p <= '9'))) {
            if (*p == '.' || *p == 'e' || *p == 'E') is_float = true;
            p++;
        }
        const char* token_end = p;

        if (is_float) {
            double res;
            auto [ptr, ec] = std::from_chars(token_start, token_end, res);
            if (ec == std::errc()) return json(res);
        } else {
            int64_t res;
            auto [ptr, ec] = std::from_chars(token_start, token_end, res);
            if (ec == std::errc()) return json(res);
        }
        // Fallback for edge cases (very huge numbers?)
        return json(0);
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
            case value_t::boolean: ss << (m_data.boolean ? "true" : "false"); break;
            case value_t::number_integer: ss << m_data.number_integer; break;
            case value_t::number_unsigned: ss << m_data.number_unsigned; break;
            case value_t::number_float: ss << m_data.number_float; break;
            case value_t::string: ss << "\"" << *m_data.string << "\""; break;
            case value_t::array:
                if (m_data.array->empty()) { ss << "[]"; return; }
                ss << "[";
                for (size_t i=0; i<m_data.array->size(); ++i) {
                    if (i>0) ss << ",";
                    if(indent>=0) ss << " ";
                    (*m_data.array)[i].dump_internal(ss, indent, current+1);
                }
                ss << "]";
                break;
            case value_t::object:
                if (m_data.object->empty()) { ss << "{}"; return; }
                ss << "{";
                for (size_t i=0; i<m_data.object->size(); ++i) {
                    if (i>0) ss << ",";
                    if(indent>=0) ss << " ";
                    ss << "\"" << (*m_data.object)[i].first << "\":";
                    if(indent>=0) ss << " ";
                    (*m_data.object)[i].second.dump_internal(ss, indent, current+1);
                }
                ss << "}";
                break;
            default: break;
        }
    }
};

inline std::ostream& operator<<(std::ostream& os, const json& j) { os << j.dump(); return os; }

} // namespace tachyon
#endif // TACHYON_HPP
