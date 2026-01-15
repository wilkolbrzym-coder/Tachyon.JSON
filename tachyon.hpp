#ifndef TACHYON_HPP
#define TACHYON_HPP

// TACHYON v8.1 "SUPERNOVA"
// The Ultimate Hybrid JSON Library (C++11/C++17)
// (C) 2026 Tachyon Systems
// License: GNU GPL v3

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <utility>
#include <stdexcept>
#include <iterator>
#include <new>
#include <functional>

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

#if __cplusplus >= 201703L
#include <charconv>
#include <string_view>
#endif

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

class Arena {
public:
    static const size_t BLOCK_SIZE = 1024 * 1024 * 512; // 512MB Block

    Arena() : m_buffer(nullptr), m_offset(0), m_capacity(0) {}

    ~Arena() {
        if (m_buffer) std::free(m_buffer);
    }

    TACHYON_FORCE_INLINE void* allocate(size_t n) {
        size_t aligned_n = (n + 7) & ~7;
        if (TACHYON_UNLIKELY(m_offset + aligned_n > m_capacity)) {
            grow();
        }
        void* ptr = m_buffer + m_offset;
        m_offset += aligned_n;
        return ptr;
    }

    void reset() {
        m_offset = 0;
    }

    static Arena& get() {
        static thread_local Arena instance;
        return instance;
    }

private:
    void grow() {
        if (!m_buffer) {
            m_capacity = BLOCK_SIZE;
            m_buffer = (char*)std::malloc(m_capacity);
        } else {
            m_capacity *= 2;
            m_buffer = (char*)std::realloc(m_buffer, m_capacity);
        }
        if (!m_buffer) throw std::bad_alloc();
    }

    char* m_buffer;
    size_t m_offset;
    size_t m_capacity;
};

namespace simd {
    struct cpu_features {
        bool avx2;
        cpu_features() {
#ifndef _MSC_VER
            __builtin_cpu_init();
            avx2 = __builtin_cpu_supports("avx2");
#else
            avx2 = true;
#endif
        }
    };
    static const cpu_features g_cpu;

    TACHYON_FORCE_INLINE const char* skip_whitespace(const char* p) {
#if defined(__AVX2__)
        if (g_cpu.avx2) {
            const __m256i v_space = _mm256_set1_epi8(' ');
            const __m256i v_tab = _mm256_set1_epi8('\t');
            const __m256i v_lf = _mm256_set1_epi8('\n');
            const __m256i v_cr = _mm256_set1_epi8('\r');

            while (true) {
                __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p));
                __m256i m1 = _mm256_cmpeq_epi8(chunk, v_space);
                __m256i m2 = _mm256_cmpeq_epi8(chunk, v_tab);
                __m256i m3 = _mm256_cmpeq_epi8(chunk, v_lf);
                __m256i m4 = _mm256_cmpeq_epi8(chunk, v_cr);
                __m256i mask = _mm256_or_si256(_mm256_or_si256(m1, m2), _mm256_or_si256(m3, m4));

                int res = _mm256_movemask_epi8(mask);
                if (res != -1) {
                    return p + __builtin_ctz(~res);
                }
                p += 32;
            }
        }
#endif
        while ((unsigned char)*p <= 32) p++;
        return p;
    }

    TACHYON_FORCE_INLINE const char* scan_string(const char* p) {
#if defined(__AVX2__)
        if (g_cpu.avx2) {
            const __m256i v_quote = _mm256_set1_epi8('"');
            const __m256i v_slash = _mm256_set1_epi8('\\');

            while (true) {
                __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p));
                __m256i m1 = _mm256_cmpeq_epi8(chunk, v_quote);
                __m256i m2 = _mm256_cmpeq_epi8(chunk, v_slash);
                __m256i mask = _mm256_or_si256(m1, m2);

                int res = _mm256_movemask_epi8(mask);
                if (res != 0) {
                    return p + __builtin_ctz(res);
                }
                p += 32;
            }
        }
#endif
        while (*p != '"' && *p != '\\') p++;
        return p;
    }
}

struct TachyonString {
    const char* ptr;
    uint32_t len;

    std::string to_std() const { return std::string(ptr, len); }
    bool operator==(const char* rhs) const { return strncmp(ptr, rhs, len) == 0 && rhs[len] == 0; }
    bool operator==(const std::string& rhs) const { return len == rhs.size() && memcmp(ptr, rhs.data(), len) == 0; }
};

struct TachyonObjectEntry_Real;

struct TachyonObject {
    void* ptr;
    uint32_t len;
    mutable bool sorted;
};

struct TachyonArray {
    void* ptr;
    uint32_t len;
};

enum class value_t : uint8_t {
    null, object, array, string, boolean, number_integer, number_unsigned, number_float, discarded
};

class json {
public:
    value_t m_type;
    union {
        TachyonObject object;
        TachyonArray array;
        TachyonString string;
        bool boolean;
        int64_t number_integer;
        uint64_t number_unsigned;
        double number_float;
    } m_value;

    json() : m_type(value_t::null) {}
    json(std::nullptr_t) : m_type(value_t::null) {}
    json(bool v) : m_type(value_t::boolean) { m_value.boolean = v; }

    // Implicit for compatibility
    json(int64_t v) : m_type(value_t::number_integer) { m_value.number_integer = v; }
    json(int v) : m_type(value_t::number_integer) { m_value.number_integer = v; }
    json(double v) : m_type(value_t::number_float) { m_value.number_float = v; }
    json(const char* s) {
        m_type = value_t::string;
        size_t len = std::strlen(s);
        char* d = (char*)Arena::get().allocate(len + 1);
        std::memcpy(d, s, len);
        d[len] = 0;
        m_value.string = {d, (uint32_t)len};
    }
    json(const std::string& s) {
        m_type = value_t::string;
        char* d = (char*)Arena::get().allocate(s.size() + 1);
        std::memcpy(d, s.data(), s.size());
        d[s.size()] = 0;
        m_value.string = {d, (uint32_t)s.size()};
    }

    // Generic constructor for custom types
    template <typename T, typename std::enable_if<!std::is_convertible<T*, json*>::value &&
                                                  !std::is_convertible<T*, const char*>::value &&
                                                  !std::is_same<T, std::string>::value &&
                                                  !std::is_arithmetic<T>::value, int>::type = 0>
    json(const T& t) : m_type(value_t::null) {
        to_json(*this, t);
    }

    // Copy/Move
    json(const json&) = default;
    json& operator=(const json&) = default;

    // Assignment operators
    json& operator=(int v) { m_type = value_t::number_integer; m_value.number_integer = v; return *this; }
    json& operator=(double v) { m_type = value_t::number_float; m_value.number_float = v; return *this; }
    json& operator=(bool v) { m_type = value_t::boolean; m_value.boolean = v; return *this; }
    json& operator=(const char* s) { return *this = json(s); }
    json& operator=(const std::string& s) { return *this = json(s); }

    // Accessors
    bool is_null() const { return m_type == value_t::null; }
    bool is_object() const { return m_type == value_t::object; }
    bool is_array() const { return m_type == value_t::array; }
    bool is_string() const { return m_type == value_t::string; }
    bool is_number() const { return m_type == value_t::number_integer || m_type == value_t::number_float; }
    bool is_boolean() const { return m_type == value_t::boolean; }

    template<typename T>
    typename std::enable_if<std::is_same<T, int>::value, T>::type get() const {
        return (int)m_value.number_integer;
    }
    template<typename T>
    typename std::enable_if<std::is_same<T, double>::value, T>::type get() const {
        return m_value.number_float;
    }
    template<typename T>
    typename std::enable_if<std::is_same<T, bool>::value, T>::type get() const {
        return m_value.boolean;
    }
    template<typename T>
    typename std::enable_if<std::is_same<T, std::string>::value, T>::type get() const {
        if (m_type == value_t::string) return m_value.string.to_std();
        return std::string();
    }
    template<typename T>
    typename std::enable_if<!std::is_arithmetic<T>::value && !std::is_same<T, std::string>::value, T>::type get() const {
        T t;
        from_json(*this, t);
        return t;
    }
    template<typename T> void get_to(T& t) const { t = get<T>(); }

    // Comparisons
    bool operator==(const char* rhs) const {
        if (m_type == value_t::string) return m_value.string == rhs;
        return false;
    }
    bool operator==(const std::string& rhs) const {
        if (m_type == value_t::string) return m_value.string == rhs;
        return false;
    }
    bool operator==(int rhs) const {
        if (m_type == value_t::number_integer) return m_value.number_integer == rhs;
        if (m_type == value_t::number_float) return m_value.number_float == (double)rhs;
        return false;
    }
    bool operator==(double rhs) const {
        if (m_type == value_t::number_float) return m_value.number_float == rhs;
        if (m_type == value_t::number_integer) return (double)m_value.number_integer == rhs;
        return false;
    }

    // Conversions
    operator int() const { return (int)m_value.number_integer; }
    operator double() const { return m_value.number_float; }
    operator bool() const { return m_value.boolean; }
    operator std::string() const { return m_value.string.to_std(); }

    // Operator []
    json& operator[](size_t idx);
    const json& operator[](size_t idx) const;
    json& operator[](int idx);
    const json& operator[](int idx) const;
    json& operator[](const std::string& key);
    const json& operator[](const std::string& key) const;
    json& operator[](const char* key);
    const json& operator[](const char* key) const;

    size_t size() const {
        if (m_type == value_t::array) return m_value.array.len;
        if (m_type == value_t::object) return m_value.object.len;
        return 0;
    }

    // Items iterator
    struct item_proxy {
        TachyonString k;
        json& v;
        std::string key() const { return k.to_std(); }
        json& value() { return v; }
    };
    struct iterator;
    struct items_view {
        json& j;
        iterator begin();
        iterator end();
    };
    items_view items() { return {*this}; }

    // Serialization
    std::string dump() const;
    void dump_internal(std::string& s) const;

    // Static
    static json parse(const std::string& s);
    static json object();
};

struct TachyonObjectEntry_Real {
    TachyonString key;
    json value;
};

struct json::iterator {
    TachyonObjectEntry_Real* ptr;
    bool operator!=(const iterator& other) const { return ptr != other.ptr; }
    void operator++() { ptr++; }
    item_proxy operator*() { return {ptr->key, ptr->value}; }
};


inline json& json::operator[](size_t idx) { return ((json*)m_value.array.ptr)[idx]; }
inline const json& json::operator[](size_t idx) const { return ((json*)m_value.array.ptr)[idx]; }
inline json& json::operator[](int idx) { return operator[]((size_t)idx); }
inline const json& json::operator[](int idx) const { return operator[]((size_t)idx); }

inline void ensure_sorted(TachyonObject& obj) {
    if (!obj.sorted && obj.len > 0) {
        auto* base = reinterpret_cast<TachyonObjectEntry_Real*>(obj.ptr);
        std::sort(base, base + obj.len, [](const TachyonObjectEntry_Real& a, const TachyonObjectEntry_Real& b) {
            uint32_t len = std::min(a.key.len, b.key.len);
            int cmp = strncmp(a.key.ptr, b.key.ptr, len);
            if (cmp == 0) return a.key.len < b.key.len;
            return cmp < 0;
        });
        obj.sorted = true;
    }
}

inline json& json::operator[](const char* key) {
    if (m_type != value_t::object) throw std::runtime_error("Not object");
    auto* base = reinterpret_cast<TachyonObjectEntry_Real*>(m_value.object.ptr);
    if (m_value.object.len >= 16) {
        ensure_sorted(m_value.object);
        TachyonObjectEntry_Real target; target.key = {key, (uint32_t)strlen(key)};
        auto it = std::lower_bound(base, base + m_value.object.len, target, [](const TachyonObjectEntry_Real& a, const TachyonObjectEntry_Real& b) {
            uint32_t len = std::min(a.key.len, b.key.len);
            int cmp = strncmp(a.key.ptr, b.key.ptr, len);
            if (cmp == 0) return a.key.len < b.key.len;
            return cmp < 0;
        });
        if (it != base + m_value.object.len && it->key == key) return it->value;
    } else {
        for (uint32_t i=0; i<m_value.object.len; ++i) { if (base[i].key == key) return base[i].value; }
    }
    throw std::out_of_range("Key not found");
}

inline const json& json::operator[](const char* key) const {
    if (m_type != value_t::object) throw std::runtime_error("Not object");
    auto* base = reinterpret_cast<TachyonObjectEntry_Real*>(m_value.object.ptr);
    if (m_value.object.len >= 16) {
        ensure_sorted(const_cast<TachyonObject&>(m_value.object));
        TachyonObjectEntry_Real target; target.key = {key, (uint32_t)strlen(key)};
        auto it = std::lower_bound(base, base + m_value.object.len, target, [](const TachyonObjectEntry_Real& a, const TachyonObjectEntry_Real& b) {
            uint32_t len = std::min(a.key.len, b.key.len);
            int cmp = strncmp(a.key.ptr, b.key.ptr, len);
            if (cmp == 0) return a.key.len < b.key.len;
            return cmp < 0;
        });
        if (it != base + m_value.object.len && it->key == key) return it->value;
    } else {
        for (uint32_t i=0; i<m_value.object.len; ++i) { if (base[i].key == key) return base[i].value; }
    }
    throw std::out_of_range("Key not found");
}

inline json& json::operator[](const std::string& key) { return (*this)[key.c_str()]; }
inline const json& json::operator[](const std::string& key) const { return (*this)[key.c_str()]; }

inline json::iterator json::items_view::begin() { return {reinterpret_cast<TachyonObjectEntry_Real*>(j.m_value.object.ptr)}; }
inline json::iterator json::items_view::end() { return {reinterpret_cast<TachyonObjectEntry_Real*>(j.m_value.object.ptr) + j.m_value.object.len}; }

inline json json::object() { json j; j.m_type = value_t::object; j.m_value.object = {nullptr, 0, false}; return j; }

inline std::string json::dump() const { std::string s; s.reserve(4096); dump_internal(s); return s; }

inline void json::dump_internal(std::string& s) const {
    switch(m_type) {
        case value_t::null: s += "null"; break;
        case value_t::boolean: s += (m_value.boolean ? "true" : "false"); break;
        case value_t::number_integer: s += std::to_string(m_value.number_integer); break;
        case value_t::number_float: s += std::to_string(m_value.number_float); break;
        case value_t::string: s += '"'; s.append(m_value.string.ptr, m_value.string.len); s += '"'; break;
        case value_t::array:
            s += '[';
            for(uint32_t i=0; i<m_value.array.len; ++i) {
                if(i>0) s += ',';
                ((json*)m_value.array.ptr)[i].dump_internal(s);
            }
            s += ']';
            break;
        case value_t::object:
            s += '{';
            {
                auto* base = reinterpret_cast<TachyonObjectEntry_Real*>(m_value.object.ptr);
                for(uint32_t i=0; i<m_value.object.len; ++i) {
                    if(i>0) s += ',';
                    s += '"'; s.append(base[i].key.ptr, base[i].key.len); s += "\":";
                    base[i].value.dump_internal(s);
                }
            }
            s += '}';
            break;
        default: break;
    }
}

inline std::ostream& operator<<(std::ostream& os, const json& j) { os << j.dump(); return os; }

} // namespace tachyon

namespace tachyon {
namespace parser {

struct StackItem { value_t type; size_t start_idx; };

static thread_local std::vector<json> val_stack;
static thread_local std::vector<TachyonString> key_stack;
static thread_local std::vector<StackItem> scope_stack;

inline void init() {
    if (val_stack.capacity() < 200000) val_stack.reserve(200000);
    if (key_stack.capacity() < 200000) key_stack.reserve(200000);
    if (scope_stack.capacity() < 2000) scope_stack.reserve(2000);
    val_stack.clear(); key_stack.clear(); scope_stack.clear();
    Arena::get().reset();
}

TACHYON_FORCE_INLINE void push_val(const json& j) { val_stack.push_back(j); }
TACHYON_FORCE_INLINE void push_key(const TachyonString& k) { key_stack.push_back(k); }
TACHYON_FORCE_INLINE void open_scope(value_t type) { scope_stack.push_back({type, val_stack.size()}); }

TACHYON_FORCE_INLINE json close_scope() {
    StackItem& s = scope_stack.back();
    size_t count = val_stack.size() - s.start_idx;
    json j;
    if (s.type == value_t::array) {
        j.m_type = value_t::array;
        if (count > 0) {
            json* ptr = (json*)Arena::get().allocate(count * sizeof(json));
            std::memcpy(ptr, &val_stack[s.start_idx], count * sizeof(json));
            j.m_value.array = {ptr, (uint32_t)count};
        } else { j.m_value.array = {nullptr, 0}; }
        val_stack.resize(s.start_idx);
    } else {
        j.m_type = value_t::object;
        if (count > 0) {
            size_t key_start = key_stack.size() - count;
            TachyonObjectEntry_Real* ptr = (TachyonObjectEntry_Real*)Arena::get().allocate(count * sizeof(TachyonObjectEntry_Real));
            for(size_t i=0; i<count; ++i) {
                ptr[i].key = key_stack[key_start + i];
                ptr[i].value = val_stack[s.start_idx + i];
            }
            j.m_value.object = {ptr, (uint32_t)count, false};
            val_stack.resize(s.start_idx);
            key_stack.resize(key_start);
        } else { j.m_value.object = {nullptr, 0, false}; }
    }
    scope_stack.pop_back();
    return j;
}

TACHYON_FORCE_INLINE const char* parse_number(const char* p, json& j) {
    const char* start = p;
    bool is_float = false;
    if (*p == '-') p++;
    while (*p >= '0' && *p <= '9') p++;
    if (*p == '.' || *p == 'e' || *p == 'E') {
        is_float = true;
        while ((*p >= '0' && *p <= '9') || *p=='.' || *p=='e' || *p=='E' || *p=='+' || *p=='-') p++;
    }
    if (is_float) {
        j.m_type = value_t::number_float;
        #if __cplusplus >= 201703L
        std::from_chars(start, p, j.m_value.number_float);
        #else
        j.m_value.number_float = std::strtod(start, nullptr);
        #endif
    } else {
        j.m_type = value_t::number_integer;
        int64_t v = 0; int sign = 1; const char* s = start;
        if (*s == '-') { sign = -1; s++; }
        while (s < p) { v = v * 10 + (*s - '0'); s++; }
        j.m_value.number_integer = v * sign;
    }
    return p;
}

TACHYON_FORCE_INLINE const char* parse_string(const char* p, TachyonString& out) {
    p++; const char* start = p;
    const char* end = tachyon::simd::scan_string(p);
    if (*end == '"') { out.ptr = start; out.len = (uint32_t)(end - start); return end + 1; }
    p = end + 1;
    while(*p != '"') { if (*p == '\\') p++; p++; }
    size_t len = p - start;
    char* d = (char*)Arena::get().allocate(len + 1);
    std::memcpy(d, start, len); d[len]=0;
    out.ptr = d; out.len = (uint32_t)len;
    return p + 1;
}

struct ParserImpl {
    static void parse_one(const char*& curr) {
        curr = tachyon::simd::skip_whitespace(curr);
        char c = *curr;
        switch (c) {
            case '{': parse_object(curr); break;
            case '[': parse_array(curr); break;
            case '"': {
                TachyonString s;
                curr = parse_string(curr, s);
                json j; j.m_type = value_t::string; j.m_value.string = s;
                push_val(j);
                break;
            }
            case 't': curr+=4; push_val(json(true)); break;
            case 'f': curr+=5; push_val(json(false)); break;
            case 'n': curr+=4; push_val(json(nullptr)); break;
            case '-': case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': {
                json j;
                curr = parse_number(curr, j);
                push_val(j);
                break;
            }
            default: {
                std::string msg = "Syntax Error at '";
                msg += c;
                msg += "' (";
                msg += std::to_string((int)c);
                msg += ")";
                throw std::runtime_error(msg);
            }
        }
    }

    static void parse_object(const char*& curr) {
        open_scope(value_t::object);
        curr++; curr = tachyon::simd::skip_whitespace(curr);
        if (*curr == '}') { curr++; push_val(close_scope()); return; }
        while(true) {
            if (TACHYON_UNLIKELY(*curr != '"')) throw std::runtime_error("Key");
            TachyonString key; curr = parse_string(curr, key); push_key(key);
            curr = tachyon::simd::skip_whitespace(curr);
            if (TACHYON_UNLIKELY(*curr != ':')) throw std::runtime_error("Col");
            curr++;
            parse_one(curr);
            curr = tachyon::simd::skip_whitespace(curr);
            if (*curr == '}') { curr++; break; }
            if (*curr == ',') { curr++; curr = tachyon::simd::skip_whitespace(curr); continue; }
            throw std::runtime_error("Comma");
        }
        push_val(close_scope());
    }

    static void parse_array(const char*& curr) {
        open_scope(value_t::array);
        curr++; curr = tachyon::simd::skip_whitespace(curr);
        if (*curr == ']') { curr++; push_val(close_scope()); return; }
        while(true) {
            parse_one(curr);
            curr = tachyon::simd::skip_whitespace(curr);
            if (*curr == ']') { curr++; break; }
            if (*curr == ',') { curr++; curr = tachyon::simd::skip_whitespace(curr); continue; }
            throw std::runtime_error("Comma");
        }
        push_val(close_scope());
    }
};

} // namespace parser

inline json json::parse(const std::string& s) {
    parser::init();
    const char* p = s.data();
    parser::ParserImpl::parse_one(p);
    return parser::val_stack.back();
}

} // namespace tachyon

// -----------------------------------------------------------------------------
// COMPATIBILITY MACROS
// -----------------------------------------------------------------------------
#ifdef TACHYON_COMPATIBILITY_MODE
namespace nlohmann = tachyon;
#define NLOHMANN_JSON_TPL tachyon::json
#endif

// Macros
#define NLOHMANN_JSON_PASTE(func, ...) NLOHMANN_JSON_PASTE_IMP(func, __VA_ARGS__)
#define NLOHMANN_JSON_PASTE_IMP(func, ...) func(__VA_ARGS__)
#define NLOHMANN_JSON_TO(v1) nlohmann_json_j[#v1] = nlohmann_json_t.v1;
#define NLOHMANN_JSON_FROM(v1) nlohmann_json_j[#v1].get_to(nlohmann_json_t.v1);

#define NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Type, ...) \
    friend void to_json(nlohmann::json& nlohmann_json_j, const Type& nlohmann_json_t) { \
        nlohmann_json_j = nlohmann::json::object(); \
        NLOHMANN_JSON_PASTE_ARGS(NLOHMANN_JSON_TO, __VA_ARGS__) \
    } \
    friend void from_json(const nlohmann::json& nlohmann_json_j, Type& nlohmann_json_t) { \
        NLOHMANN_JSON_PASTE_ARGS(NLOHMANN_JSON_FROM, __VA_ARGS__) \
    }

#define NLOHMANN_JSON_EXPAND( x ) x
#define NLOHMANN_JSON_GET_MACRO(_1, _2, _3, _4, _5, NAME,...) NAME
#define NLOHMANN_JSON_PASTE2(func, v1) func(v1)
#define NLOHMANN_JSON_PASTE3(func, v1, v2) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE2(func, v2)
#define NLOHMANN_JSON_PASTE4(func, v1, v2, v3) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE3(func, v2, v3)
#define NLOHMANN_JSON_PASTE5(func, v1, v2, v3, v4) NLOHMANN_JSON_PASTE2(func, v1) NLOHMANN_JSON_PASTE4(func, v2, v3, v4)

#define NLOHMANN_JSON_PASTE_ARGS(...) NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_GET_MACRO(__VA_ARGS__, \
        NLOHMANN_JSON_PASTE5, \
        NLOHMANN_JSON_PASTE4, \
        NLOHMANN_JSON_PASTE3, \
        NLOHMANN_JSON_PASTE2)(__VA_ARGS__))

#endif // TACHYON_HPP
