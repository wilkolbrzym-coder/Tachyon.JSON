#ifndef TACHYON_HPP
#define TACHYON_HPP

// TACHYON v8.3 "SINGULARITY"
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
#include <type_traits>

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

// -----------------------------------------------------------------------------
// MEMORY: PAGED ARENA
// -----------------------------------------------------------------------------
class PagedArena {
public:
    static const size_t PAGE_SIZE = 64 * 1024;

    struct Page {
        Page* next;
        char data[1];
    };

    PagedArena() : head(nullptr), current(nullptr), offset(0), end_offset(0) {
        grow(PAGE_SIZE);
    }

    ~PagedArena() {
        while (head) {
            Page* next = head->next;
            std::free(head);
            head = next;
        }
    }

    TACHYON_FORCE_INLINE void* allocate(size_t n) {
        size_t aligned_n = (n + 7) & ~7;
        if (TACHYON_UNLIKELY(aligned_n > end_offset - offset)) {
             if (aligned_n > PAGE_SIZE) {
                 Page* huge = (Page*)std::malloc(sizeof(Page*) + aligned_n);
                 huge->next = head->next;
                 head->next = huge;
                 return huge->data;
             }
             grow(PAGE_SIZE);
        }
        void* ptr = current->data + offset;
        offset += aligned_n;
        return ptr;
    }

    static PagedArena& tls_instance() {
        static thread_local PagedArena instance;
        return instance;
    }

private:
    void grow(size_t size) {
        Page* p = (Page*)std::malloc(sizeof(Page*) + size);
        p->next = nullptr;
        if (current) current->next = p;
        else head = p;
        current = p;
        offset = 0;
        end_offset = size;
    }

    Page* head;
    Page* current;
    size_t offset;
    size_t end_offset;
};

// -----------------------------------------------------------------------------
// CORE TYPES
// -----------------------------------------------------------------------------
struct TachyonString {
    const char* ptr;
    uint32_t len;

    std::string to_std() const { return std::string(ptr, len); }
    bool operator==(const char* rhs) const { return strncmp(ptr, rhs, len) == 0 && rhs[len] == 0; }
    bool operator==(const std::string& rhs) const { return len == rhs.size() && memcmp(ptr, rhs.data(), len) == 0; }
};

struct TachyonObject {
    void* entries; // TachyonObjectEntry*
    uint32_t len;
    uint32_t cap;
    bool sorted;
};

struct TachyonArray {
    void* ptr; // json*
    uint32_t len;
    uint32_t cap;
};

enum class value_t : uint8_t {
    null, object, array, string, boolean, number_integer, number_unsigned, number_float, discarded
};

class json;

// Generic getter helper
template<typename T> struct Getter;

// ADL hook
template<typename T>
void to_json(json& j, const T& t);

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
    json(int64_t v) : m_type(value_t::number_integer) { m_value.number_integer = v; }
    json(uint64_t v) : m_type(value_t::number_integer) { m_value.number_integer = (int64_t)v; }
    json(int v) : m_type(value_t::number_integer) { m_value.number_integer = v; }
    json(double v) : m_type(value_t::number_float) { m_value.number_float = v; }

    json(const char* s) { set_string(s, strlen(s)); }
    json(const std::string& s) { set_string(s.data(), s.size()); }

    template <typename T, typename = typename std::enable_if<!std::is_convertible<T, json>::value>::type>
    json(const T& t) {
        m_type = value_t::null;
        to_json(*this, t);
    }

    static json object() {
        json j;
        j.m_type = value_t::object;
        j.m_value.object = {nullptr, 0, 0, false};
        return j;
    }

    void set_string(const char* s, size_t len) {
        m_type = value_t::string;
        char* d = (char*)PagedArena::tls_instance().allocate(len + 1);
        memcpy(d, s, len); d[len] = 0;
        m_value.string = {d, (uint32_t)len};
    }

    bool is_null() const { return m_type == value_t::null; }
    bool is_object() const { return m_type == value_t::object; }
    bool is_array() const { return m_type == value_t::array; }
    bool is_string() const { return m_type == value_t::string; }
    bool is_number() const { return m_type == value_t::number_integer || m_type == value_t::number_float; }
    bool is_boolean() const { return m_type == value_t::boolean; }

    size_t size() const {
        if (m_type == value_t::array) return m_value.array.len;
        if (m_type == value_t::object) return m_value.object.len;
        return 0;
    }

    // Accessors
    json& operator[](const char* key);
    json& operator[](const std::string& key) { return (*this)[key.c_str()]; }
    const json& operator[](const char* key) const;
    const json& operator[](const std::string& key) const { return (*this)[key.c_str()]; }

    json& operator[](size_t idx);
    const json& operator[](size_t idx) const;
    json& operator[](int idx) { return (*this)[(size_t)idx]; }

    template<typename T> T get() const;

    operator int() const { return (int)m_value.number_integer; }
    operator double() const { return m_value.number_float; }
    operator bool() const { return m_value.boolean; }
    operator std::string() const { return m_value.string.to_std(); }

    std::string dump() const;
    static json parse(const std::string& s);
    static json parse(const char* s, size_t len);

    // Friendly stream operator inside class to help ADL
    friend std::ostream& operator<<(std::ostream& os, const json& j) {
        os << j.dump();
        return os;
    }

    // Comparison
    friend bool operator==(const json& lhs, const json& rhs);
    friend bool operator==(const json& lhs, const char* rhs) { return lhs.m_type == value_t::string && lhs.m_value.string == rhs; }
    friend bool operator==(const json& lhs, const std::string& rhs) { return lhs.m_type == value_t::string && lhs.m_value.string == rhs; }
    friend bool operator==(const json& lhs, int rhs) { return lhs.m_type == value_t::number_integer && lhs.m_value.number_integer == rhs; }
    friend bool operator==(const json& lhs, double rhs) { return lhs.m_type == value_t::number_float && lhs.m_value.number_float == rhs; }
    friend bool operator==(const json& lhs, bool rhs) { return lhs.m_type == value_t::boolean && lhs.m_value.boolean == rhs; }
    friend bool operator==(const json& lhs, std::nullptr_t) { return lhs.m_type == value_t::null; }

    struct item_proxy;
    struct iterator;
    struct items_view {
        json* j;
        iterator begin();
        iterator end();
    };
    items_view items() { return {this}; }
};

struct TachyonObjectEntry {
    TachyonString key;
    json val_; // Renamed to avoid confusion/clashes
};

// Implementations

inline json& json::operator[](size_t idx) {
    return ((json*)m_value.array.ptr)[idx];
}
inline const json& json::operator[](size_t idx) const {
    return ((json*)m_value.array.ptr)[idx];
}

inline json& json::operator[](const char* key) {
    if (m_type == value_t::null) {
        m_type = value_t::object;
        m_value.object = {nullptr, 0, 0, false};
    }
    if (m_type != value_t::object) throw std::runtime_error("Not object");

    TachyonObjectEntry* entries = (TachyonObjectEntry*)m_value.object.entries;
    for(size_t i=0; i<m_value.object.len; ++i) {
        if (entries[i].key == key) return entries[i].val_;
    }

    size_t new_len = m_value.object.len + 1;
    TachyonObjectEntry* new_entries = (TachyonObjectEntry*)PagedArena::tls_instance().allocate(new_len * sizeof(TachyonObjectEntry));
    if (m_value.object.len > 0) std::memcpy(new_entries, entries, m_value.object.len * sizeof(TachyonObjectEntry));

    size_t klen = strlen(key);
    char* d = (char*)PagedArena::tls_instance().allocate(klen+1);
    memcpy(d, key, klen); d[klen]=0;

    new_entries[m_value.object.len].key = {d, (uint32_t)klen};
    new_entries[m_value.object.len].val_ = json();

    m_value.object.entries = new_entries;
    m_value.object.len = new_len;

    return new_entries[new_len-1].val_;
}

inline const json& json::operator[](const char* key) const {
    if (m_type != value_t::object) throw std::runtime_error("Not object");
    TachyonObjectEntry* entries = (TachyonObjectEntry*)m_value.object.entries;
    for(size_t i=0; i<m_value.object.len; ++i) {
        if (entries[i].key == key) return entries[i].val_;
    }
    throw std::out_of_range("Key not found");
}

struct json::item_proxy {
    TachyonString k;
    json* v;
    std::string key() const { return k.to_std(); }
    json& value() { return *v; }
};

struct json::iterator {
    void* ptr;
    bool is_obj;
    bool operator!=(const iterator& other) const { return ptr != other.ptr; }
    void operator++() {
        if (is_obj) ptr = (char*)ptr + sizeof(TachyonObjectEntry);
        else ptr = (char*)ptr + sizeof(json);
    }
    item_proxy operator*() {
        if (is_obj) {
            TachyonObjectEntry* e = (TachyonObjectEntry*)ptr;
            return {e->key, &e->val_};
        } else {
            json* j = (json*)ptr;
            return {{"",0}, j};
        }
    }
};

inline json::iterator json::items_view::begin() {
    if (j->m_type == value_t::object) return {j->m_value.object.entries, true};
    return {j->m_value.array.ptr, false};
}
inline json::iterator json::items_view::end() {
    if (j->m_type == value_t::object) {
        return {(char*)j->m_value.object.entries + j->m_value.object.len * sizeof(TachyonObjectEntry), true};
    }
    return {(char*)j->m_value.array.ptr + j->m_value.array.len * sizeof(json), false};
}

// from_json forward
void from_json(const json& j, int& v);

// Getter definition
template<typename T> struct Getter {
    static T get(const json* j) {
        T t;
        from_json(*j, t);
        return t;
    }
};

// Specializations
template<> struct Getter<int> { static int get(const json* j) { return (int)j->m_value.number_integer; } };
template<> struct Getter<double> { static double get(const json* j) { return j->m_value.number_float; } };
template<> struct Getter<std::string> { static std::string get(const json* j) { return j->m_value.string.to_std(); } };

template<typename T> T json::get() const { return Getter<T>::get(this); }

inline std::string json::dump() const {
    if (m_type == value_t::string) return "\"" + m_value.string.to_std() + "\"";
    if (m_type == value_t::number_integer) return std::to_string(m_value.number_integer);
    if (m_type == value_t::number_float) return std::to_string(m_value.number_float);
    if (m_type == value_t::boolean) return m_value.boolean ? "true" : "false";
    if (m_type == value_t::null) return "null";
    if (m_type == value_t::array) {
        std::string s = "[";
        json* arr = (json*)m_value.array.ptr;
        for(size_t i=0; i<m_value.array.len; ++i) {
            if(i>0) s+=",";
            s += arr[i].dump();
        }
        s += "]";
        return s;
    }
    if (m_type == value_t::object) {
        std::string s = "{";
        TachyonObjectEntry* entries = (TachyonObjectEntry*)m_value.object.entries;
        for(size_t i=0; i<m_value.object.len; ++i) {
            if(i>0) s+=",";
            s += "\"" + entries[i].key.to_std() + "\":";
            s += entries[i].val_.dump();
        }
        s += "}";
        return s;
    }
    return "";
}

inline void from_json(const json& j, int& v) {
    if (j.m_type == value_t::number_integer) v = (int)j.m_value.number_integer;
    else if (j.m_type == value_t::number_float) v = (int)j.m_value.number_float;
    else v = 0;
}

// -----------------------------------------------------------------------------
// PARSER
// -----------------------------------------------------------------------------
namespace parser {

static thread_local std::vector<json> array_stack;
static thread_local std::vector<TachyonObjectEntry> object_stack;

struct StackItem {
    uint8_t type;
    size_t start_idx;
};

static thread_local std::vector<StackItem> call_stack;

TACHYON_FORCE_INLINE void init_stacks() {
    array_stack.reserve(65536);
    object_stack.reserve(65536);
    call_stack.reserve(1024);
    array_stack.clear();
    object_stack.clear();
    call_stack.clear();
}

TACHYON_FORCE_INLINE const char* scan_string(const char* curr, const char* end) {
    while (curr + 8 <= end) {
        uint64_t chunk;
        std::memcpy(&chunk, curr, 8);
        uint64_t v1 = chunk ^ 0x2222222222222222ULL;
        uint64_t v2 = chunk ^ 0x5C5C5C5C5C5C5C5CULL;
        uint64_t has_zero1 = (v1 - 0x0101010101010101ULL) & ~v1 & 0x8080808080808080ULL;
        uint64_t has_zero2 = (v2 - 0x0101010101010101ULL) & ~v2 & 0x8080808080808080ULL;
        if (has_zero1 | has_zero2) break;
        curr += 8;
    }
    while (curr < end) {
        if (*curr == '"' || *curr == '\\') return curr;
        curr++;
    }
    return curr;
}

TACHYON_FORCE_INLINE json parse_number(const char*& curr) {
    const char* start = curr;
    bool neg = false;
    if (*curr == '-') { neg=true; curr++; }

    bool floating = false;
    while(true) {
        char c = *curr;
        if (c >= '0' && c <= '9') { curr++; continue; }
        if (c == '.' || c == 'e' || c == 'E') { floating=true; curr++; continue; }
        if (c == '+' || c == '-') { curr++; continue; }
        break;
    }

    if (floating) {
#if __cplusplus >= 201703L
        double d;
        std::from_chars(start, curr, d);
        return json(d);
#else
        return json(std::strtod(start, nullptr));
#endif
    } else {
        uint64_t v = 0;
#if __cplusplus >= 201703L
        std::from_chars(start, curr, v);
#else
        const char* p = start;
        if (neg) p++;
        while(p < curr) { v = v*10 + (*p - '0'); p++; }
#endif
        if (neg) return json(-(int64_t)v);
        return json(v);
    }
}

inline json parse(const char* ptr, size_t len) {
    init_stacks();
    const char* end = ptr + len;
    const char* curr = ptr;

    while (curr < end && (unsigned char)*curr <= 32) curr++;
    if (curr == end) return json();

    json root;
    char c = *curr;
    if (c == '{') {
        call_stack.push_back({2, object_stack.size()});
        curr++;
    } else if (c == '[') {
        call_stack.push_back({1, array_stack.size()});
        curr++;
    } else {
        if (c == '"') {
            curr++; const char* s=curr; curr=scan_string(curr, end);
            TachyonString ts={s, (uint32_t)(curr-s)}; if (*curr=='\\') { while(*curr!='"' || *(curr-1)=='\\') curr++; } curr++;
            return json(std::string(ts.ptr, ts.len));
        }
        return parse_number(curr);
    }

    while (!call_stack.empty()) {
        StackItem& state = call_stack.back();
        while (curr < end && (unsigned char)*curr <= 32) curr++;

        if (state.type == 1) { // Array
            if (*curr == ']') {
                curr++;
                size_t start = state.start_idx;
                size_t count = array_stack.size() - start;

                json* arr_ptr = nullptr;
                if (count > 0) {
                    arr_ptr = (json*)PagedArena::tls_instance().allocate(count * sizeof(json));
                    std::memcpy(arr_ptr, &array_stack[start], count * sizeof(json));
                    array_stack.resize(start);
                }

                json j;
                j.m_type = value_t::array;
                j.m_value.array = {arr_ptr, (uint32_t)count, (uint32_t)count};

                call_stack.pop_back();
                if (call_stack.empty()) return j;

                StackItem& parent = call_stack.back();
                if (parent.type == 1) array_stack.push_back(j);
                else {
                    object_stack.back().val_ = j;
                }

                while (curr < end && (unsigned char)*curr <= 32) curr++;
                if (*curr == ',') { curr++; continue; }
                continue;
            }

            char c = *curr;
            if (c == '{') {
                call_stack.push_back({2, object_stack.size()});
                curr++;
                continue;
            } else if (c == '[') {
                call_stack.push_back({1, array_stack.size()});
                curr++;
                continue;
            } else {
                json val;
                if (c == '"') {
                    curr++; const char* s=curr; curr=scan_string(curr, end);
                    if (*curr == '\\') { while(*curr!='"' || *(curr-1)=='\\') curr++; }
                    val.set_string(s, curr-s); curr++;
                } else if (c == 't') { curr+=4; val=json(true); }
                else if (c == 'f') { curr+=5; val=json(false); }
                else if (c == 'n') { curr+=4; val=json(nullptr); }
                else { val = parse_number(curr); }

                array_stack.push_back(val);

                while (curr < end && (unsigned char)*curr <= 32) curr++;
                if (*curr == ',') { curr++; continue; }
            }

        } else { // Object
            if (*curr == '}') {
                curr++;
                size_t start = state.start_idx;
                size_t count = object_stack.size() - start;

                TachyonObjectEntry* obj_ptr = nullptr;
                if (count > 0) {
                    obj_ptr = (TachyonObjectEntry*)PagedArena::tls_instance().allocate(count * sizeof(TachyonObjectEntry));
                    std::memcpy(obj_ptr, &object_stack[start], count * sizeof(TachyonObjectEntry));
                    object_stack.resize(start);
                }

                json j;
                j.m_type = value_t::object;
                j.m_value.object = {obj_ptr, (uint32_t)count, (uint32_t)count, false};

                call_stack.pop_back();
                if (call_stack.empty()) return j;

                StackItem& parent = call_stack.back();
                if (parent.type == 1) array_stack.push_back(j);
                else object_stack.back().val_ = j;

                while (curr < end && (unsigned char)*curr <= 32) curr++;
                if (*curr == ',') { curr++; continue; }
                continue;
            }

            if (*curr != '"') throw std::runtime_error("Key exp");
            curr++; const char* s=curr; curr=scan_string(curr, end);
            TachyonString key = {s, (uint32_t)(curr-s)};
            if (*curr == '\\') { while(*curr!='"' || *(curr-1)=='\\') curr++; key.len=curr-s; }
            curr++;

            while (curr < end && (unsigned char)*curr <= 32) curr++;
            if (*curr != ':') throw std::runtime_error("Col");
            curr++;
            while (curr < end && (unsigned char)*curr <= 32) curr++;

            TachyonObjectEntry entry;
            entry.key = key;
            object_stack.push_back(entry);

            char c = *curr;
            if (c == '{') {
                call_stack.push_back({2, object_stack.size()});
                curr++;
                continue;
            } else if (c == '[') {
                call_stack.push_back({1, array_stack.size()});
                curr++;
                continue;
            } else {
                json val;
                if (c == '"') {
                    curr++; const char* vs=curr; curr=scan_string(curr, end);
                    if (*curr == '\\') { while(*curr!='"' || *(curr-1)=='\\') curr++; }
                    val.set_string(vs, curr-vs); curr++;
                } else if (c == 't') { curr+=4; val=json(true); }
                else if (c == 'f') { curr+=5; val=json(false); }
                else if (c == 'n') { curr+=4; val=json(nullptr); }
                else { val = parse_number(curr); }

                object_stack.back().val_ = val;

                while (curr < end && (unsigned char)*curr <= 32) curr++;
                if (*curr == ',') { curr++; continue; }
            }
        }
    }
    return root;
}

} // parser

inline json json::parse(const std::string& s) {
    return parser::parse(s.data(), s.size());
}
inline json json::parse(const char* s, size_t len) {
    return parser::parse(s, len);
}

} // tachyon

#ifdef TACHYON_COMPATIBILITY_MODE
namespace nlohmann = tachyon;
#define NLOHMANN_JSON_TPL tachyon::json
#endif

#ifndef NLOHMANN_JSON_PASTE
#define NLOHMANN_JSON_PASTE(func, ...) func(__VA_ARGS__)
#endif

#ifndef NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE
#define NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Type, ...) \
    friend void to_json(nlohmann::json& nlohmann_json_j, const Type& nlohmann_json_t) { \
        nlohmann_json_j = nlohmann::json::object(); \
    } \
    friend void from_json(const nlohmann::json& nlohmann_json_j, Type& nlohmann_json_t) { /* Stub */ }
#endif

#endif // TACHYON_HPP
