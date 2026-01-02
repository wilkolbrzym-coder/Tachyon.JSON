#ifndef TACHYON_HPP
#define TACHYON_HPP

#include <algorithm>
#include <charconv>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <cstdio>
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

// -----------------------------------------------------------------------------
// Tachyon JSON 5v (Turbo) - C++23
// -----------------------------------------------------------------------------

namespace Tachyon {

// -----------------------------------------------------------------------------
// Concepts (C++20/23)
// -----------------------------------------------------------------------------

template <typename T>
concept Arithmetic = std::is_arithmetic_v<T> && !std::is_same_v<T, bool> &&
                     !std::is_same_v<T, char>;

template <typename T>
concept StringLike = std::convertible_to<T, std::string_view>;

template <typename T>
concept JsonValue = requires(T t) {
    typename T::Type;
    { t.dump() } -> std::convertible_to<std::string>;
};

// -----------------------------------------------------------------------------
// Forward Declarations
// -----------------------------------------------------------------------------

class Json;
struct ParseOptions;
struct DumpOptions;
class JsonParseException;

// -----------------------------------------------------------------------------
// Configuration & Constants
// -----------------------------------------------------------------------------

enum class Type : uint8_t {
    Null,
    Boolean,
    NumberInt,
    NumberFloat,
    String,
    Array,
    Object
};

struct ParseOptions {
    bool allow_comments = true;
    bool allow_trailing_commas = true;
    bool fast_float = true;
    size_t max_depth = 256;
};

struct DumpOptions {
    int indent = -1; // -1 for compact
    char indent_char = ' ';
    bool sort_keys = false; // Note: ObjectMap is usually sorted by default for speed
    bool ascii_only = false;
};

// -----------------------------------------------------------------------------
// Internal: Fast Flat Map for Object Storage
// -----------------------------------------------------------------------------

class ObjectMap {
public:
    using Member = std::pair<std::string, Json>;
    using Container = std::vector<Member>;
    using iterator = Container::iterator;
    using const_iterator = Container::const_iterator;

    ObjectMap() = default;
    ObjectMap(std::initializer_list<Member> init);

    // Access
    Json& operator[](std::string_view key);
    const Json& at(std::string_view key) const;
    bool contains(std::string_view key) const;

    // Modifiers
    void insert_or_assign(std::string key, Json value);
    void emplace(std::string key, Json value);
    void erase(std::string_view key);
    void clear() { m_data.clear(); }

    // Iterators
    iterator begin() { return m_data.begin(); }
    iterator end() { return m_data.end(); }
    const_iterator begin() const { return m_data.begin(); }
    const_iterator end() const { return m_data.end(); }
    size_t size() const { return m_data.size(); }
    bool empty() const { return m_data.empty(); }

    void sort();

    // Equality
    bool operator==(const ObjectMap& other) const;
    bool operator!=(const ObjectMap& other) const { return !(*this == other); }

private:
    Container m_data;
    bool m_sorted = false;

    iterator find_impl(std::string_view key);
    const_iterator find_impl(std::string_view key) const;
};

// -----------------------------------------------------------------------------
// Exceptions
// -----------------------------------------------------------------------------

class JsonException : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

class JsonParseException : public JsonException {
public:
    JsonParseException(std::string msg, size_t line, size_t col)
        : JsonException(std::format("{} at line {}, col {}", msg, line, col)),
          m_line(line), m_col(col) {}
    size_t line() const { return m_line; }
    size_t column() const { return m_col; }
private:
    size_t m_line;
    size_t m_col;
};

// -----------------------------------------------------------------------------
// Main Json Class
// -----------------------------------------------------------------------------

class Json {
public:
    // Types
    using array_t = std::vector<Json>;
    using object_t = ObjectMap;
    using string_t = std::string;
    using number_int_t = int64_t;
    using number_float_t = double;
    using boolean_t = bool;

private:
    using Variant = std::variant<std::monostate, boolean_t, number_int_t, number_float_t, string_t, array_t, object_t>;
    Variant m_data;

public:
    // Constructors
    Json() noexcept : m_data(std::monostate{}) {}
    Json(std::nullptr_t) noexcept : m_data(std::monostate{}) {}
    Json(bool val) noexcept : m_data(val) {}
    Json(Arithmetic auto val) noexcept {
        if constexpr (std::is_floating_point_v<decltype(val)>) {
            m_data = static_cast<double>(val);
        } else {
            m_data = static_cast<int64_t>(val);
        }
    }
    Json(StringLike auto val) : m_data(std::string(val)) {}
    Json(const array_t& val) : m_data(val) {}
    Json(array_t&& val) noexcept : m_data(std::move(val)) {}
    Json(const object_t& val) : m_data(val) {}
    Json(object_t&& val) noexcept : m_data(std::move(val)) {}

    // Initializer List
    Json(std::initializer_list<Json> init);

    // Type Check
    Type type() const noexcept;
    bool is_null() const noexcept { return type() == Type::Null; }
    bool is_boolean() const noexcept { return type() == Type::Boolean; }
    bool is_number() const noexcept { return type() == Type::NumberInt || type() == Type::NumberFloat; }
    bool is_number_int() const noexcept { return type() == Type::NumberInt; }
    bool is_number_float() const noexcept { return type() == Type::NumberFloat; }
    bool is_string() const noexcept { return type() == Type::String; }
    bool is_array() const noexcept { return type() == Type::Array; }
    bool is_object() const noexcept { return type() == Type::Object; }

    // Conversions
    template <typename T>
    T get() const;

    // Zero-Copy Reference Access
    template <typename T>
    const T& get_ref() const;

    template <typename T>
    T& get_ref();

    template <typename T>
    T get_or(T default_value) const {
        try { return get<T>(); } catch(...) { return default_value; }
    }

    // Accessors
    Json& operator[](size_t index);
    const Json& operator[](size_t index) const;
    Json& operator[](int index);
    const Json& operator[](int index) const;
    Json& operator[](std::string_view key);
    const Json& at(size_t index) const;
    const Json& at(std::string_view key) const;

    size_t size() const;
    bool empty() const;
    void clear();

    // Modifiers
    void push_back(Json val);

    // Serialization
    static Json parse(std::string_view json, const ParseOptions& opts = {});
    std::string dump(const DumpOptions& opts = {}) const;

    // Comparison
    bool operator==(const Json& other) const;
    bool operator!=(const Json& other) const { return !(*this == other); }
};

// -----------------------------------------------------------------------------
// Implementation: ObjectMap
// -----------------------------------------------------------------------------

inline ObjectMap::ObjectMap(std::initializer_list<Member> init) : m_data(init) {
    m_sorted = false;
}

inline void ObjectMap::sort() {
    if (m_sorted) return;
    std::ranges::sort(m_data, [](const Member& a, const Member& b) {
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
    return std::find_if(m_data.begin(), m_data.end(),
        [&](const Member& m) { return m.first == key; });
}

inline ObjectMap::const_iterator ObjectMap::find_impl(std::string_view key) const {
    if (m_sorted) {
        auto it = std::lower_bound(m_data.begin(), m_data.end(), key,
            [](const Member& m, std::string_view k) { return m.first < k; });
        if (it != m_data.end() && it->first == key) return it;
        return m_data.end();
    }
    return std::find_if(m_data.begin(), m_data.end(),
        [&](const Member& m) { return m.first == key; });
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
    if (it == m_data.end()) throw JsonException(std::format("Key '{}' not found", key));
    return it->second;
}

inline bool ObjectMap::contains(std::string_view key) const {
    return find_impl(key) != m_data.end();
}

inline void ObjectMap::insert_or_assign(std::string key, Json value) {
    auto it = find_impl(key);
    if (it != m_data.end()) {
        it->second = std::move(value);
    } else {
        m_data.emplace_back(std::move(key), std::move(value));
        m_sorted = false;
    }
}

inline void ObjectMap::emplace(std::string key, Json value) {
    m_data.emplace_back(std::move(key), std::move(value));
    m_sorted = false;
}

inline void ObjectMap::erase(std::string_view key) {
     auto it = find_impl(key);
     if (it != m_data.end()) {
         m_data.erase(it);
         // Erasing from sorted vector keeps it sorted?
         // Yes, if we use erase(iterator) on vector, relative order is preserved.
     }
}

inline bool ObjectMap::operator==(const ObjectMap& other) const {
    if (size() != other.size()) return false;
    for (const auto& [key, val] : m_data) {
        if (!other.contains(key)) return false;
        if (other.at(key) != val) return false;
    }
    return true;
}

// -----------------------------------------------------------------------------
// Implementation: Json
// -----------------------------------------------------------------------------

inline Json::Json(std::initializer_list<Json> init) {
    bool is_obj = std::all_of(init.begin(), init.end(), [](const Json& j) {
        return j.is_array() && j.size() == 2 && j.at(0).is_string();
    });

    if (is_obj && init.size() > 0) {
        object_t obj;
        for (const auto& el : init) {
            obj.emplace(el.at(0).get<std::string>(), el.at(1));
        }
        obj.sort();
        m_data = std::move(obj);
    } else {
        m_data = array_t(init);
    }
}

inline Type Json::type() const noexcept {
    if (std::holds_alternative<std::monostate>(m_data)) return Type::Null;
    if (std::holds_alternative<boolean_t>(m_data)) return Type::Boolean;
    if (std::holds_alternative<number_int_t>(m_data)) return Type::NumberInt;
    if (std::holds_alternative<number_float_t>(m_data)) return Type::NumberFloat;
    if (std::holds_alternative<string_t>(m_data)) return Type::String;
    if (std::holds_alternative<array_t>(m_data)) return Type::Array;
    if (std::holds_alternative<object_t>(m_data)) return Type::Object;
    return Type::Null;
}

template <typename T>
T Json::get() const {
    return get_ref<T>(); // Return copy of the reference
}

template <typename T>
const T& Json::get_ref() const {
    if constexpr (std::is_same_v<T, bool>) {
        if (is_boolean()) return std::get<boolean_t>(m_data);
        throw JsonException("Type mismatch: expected boolean");
    } else if constexpr (std::is_same_v<T, int64_t>) {
        if (is_number_int()) return std::get<number_int_t>(m_data);
        throw JsonException("Type mismatch: expected int64_t");
    } else if constexpr (std::is_same_v<T, double>) {
        if (is_number_float()) return std::get<number_float_t>(m_data);
        throw JsonException("Type mismatch: expected double");
    } else if constexpr (std::integral<T>) {
         // Cannot return reference to temporary cast, so get_ref<int> on int64_t is tricky.
         // We must only support exact types for get_ref, or throw error.
         // Or we allow get_ref to fail if type is not exact.
         // For convenience, we assume get_ref is for container access mainly.
         // For primitives, by-value get<T> is better.
         throw JsonException("get_ref<T> requires exact type match. Use get<T> for conversions.");
    } else if constexpr (std::is_same_v<T, std::string>) {
        if (is_string()) return std::get<string_t>(m_data);
        throw JsonException("Type mismatch: expected string");
    } else if constexpr (std::is_same_v<T, array_t>) {
        if (is_array()) return std::get<array_t>(m_data);
        throw JsonException("Type mismatch: expected array");
    } else if constexpr (std::is_same_v<T, object_t>) {
        if (is_object()) return std::get<object_t>(m_data);
        throw JsonException("Type mismatch: expected object");
    } else {
        throw JsonException("Unsupported type get_ref");
    }
}

template <typename T>
T& Json::get_ref() {
    // Non-const version
     if constexpr (std::is_same_v<T, bool>) {
        if (is_boolean()) return std::get<boolean_t>(m_data);
        throw JsonException("Type mismatch: expected boolean");
    } else if constexpr (std::is_same_v<T, int64_t>) {
        if (is_number_int()) return std::get<number_int_t>(m_data);
        throw JsonException("Type mismatch: expected int64_t");
    } else if constexpr (std::is_same_v<T, double>) {
        if (is_number_float()) return std::get<number_float_t>(m_data);
        throw JsonException("Type mismatch: expected double");
    } else if constexpr (std::is_same_v<T, std::string>) {
        if (is_string()) return std::get<string_t>(m_data);
        throw JsonException("Type mismatch: expected string");
    } else if constexpr (std::is_same_v<T, array_t>) {
        if (is_array()) return std::get<array_t>(m_data);
        throw JsonException("Type mismatch: expected array");
    } else if constexpr (std::is_same_v<T, object_t>) {
        if (is_object()) return std::get<object_t>(m_data);
        throw JsonException("Type mismatch: expected object");
    } else {
        throw JsonException("Unsupported type get_ref");
    }
}

// Specialization for get<T> where T is a primitive but storage is compatible
// We need to overload get<T> properly because generic get calls get_ref, which fails for int vs int64_t
template <>
inline int Json::get<int>() const {
    if (is_number_int()) return static_cast<int>(std::get<number_int_t>(m_data));
    if (is_number_float()) return static_cast<int>(std::get<number_float_t>(m_data));
    throw JsonException("Type mismatch: expected integer");
}

template <>
inline double Json::get<double>() const {
    if (is_number_float()) return std::get<number_float_t>(m_data);
    if (is_number_int()) return static_cast<double>(std::get<number_int_t>(m_data));
    throw JsonException("Type mismatch: expected number");
}

inline Json& Json::operator[](size_t index) {
    if (is_null()) m_data = array_t{};
    if (!is_array()) throw JsonException("Not an array");
    auto& arr = std::get<array_t>(m_data);
    if (index >= arr.size()) arr.resize(index + 1);
    return arr[index];
}

inline const Json& Json::operator[](size_t index) const {
    return at(index);
}

inline Json& Json::operator[](int index) {
    if (index < 0) throw JsonException("Index cannot be negative");
    return operator[](static_cast<size_t>(index));
}

inline const Json& Json::operator[](int index) const {
    if (index < 0) throw JsonException("Index cannot be negative");
    return at(static_cast<size_t>(index));
}

inline Json& Json::operator[](std::string_view key) {
    if (is_null()) m_data = object_t{};
    if (!is_object()) throw JsonException("Not an object");
    return std::get<object_t>(m_data)[key];
}

inline const Json& Json::at(size_t index) const {
    if (!is_array()) throw JsonException("Not an array");
    const auto& arr = std::get<array_t>(m_data);
    if (index >= arr.size()) throw JsonException("Index out of bounds");
    return arr[index];
}

inline const Json& Json::at(std::string_view key) const {
    if (!is_object()) throw JsonException("Not an object");
    return std::get<object_t>(m_data).at(key);
}

inline size_t Json::size() const {
    if (is_array()) return std::get<array_t>(m_data).size();
    if (is_object()) return std::get<object_t>(m_data).size();
    if (is_string()) return std::get<string_t>(m_data).size();
    if (is_null()) return 0;
    return 1;
}

inline bool Json::empty() const {
    return size() == 0;
}

inline void Json::clear() {
    m_data = std::monostate{};
}

inline void Json::push_back(Json val) {
    if (is_null()) m_data = array_t{};
    if (!is_array()) throw JsonException("Not an array");
    std::get<array_t>(m_data).push_back(std::move(val));
}

inline bool Json::operator==(const Json& other) const {
    return m_data == other.m_data;
}

// -----------------------------------------------------------------------------
// Parser
// -----------------------------------------------------------------------------

namespace Internal {
    class Parser {
        std::string_view m_json;
        size_t m_pos = 0;
        const ParseOptions& m_opts;

    public:
        Parser(std::string_view json, const ParseOptions& opts)
            : m_json(json), m_opts(opts) {}

        Json parse() {
            skip_whitespace();
            Json res = parse_value();
            skip_whitespace();
            if (m_pos < m_json.size()) throw error("Unexpected characters after end of JSON");
            return res;
        }

    private:
        char peek() const { return m_pos < m_json.size() ? m_json[m_pos] : 0; }
        char advance() { return m_pos < m_json.size() ? m_json[m_pos++] : 0; }
        bool match(char c) { if (peek() == c) { advance(); return true; } return false; }

        JsonParseException error(std::string msg) {
            size_t line = 1, col = 1;
            for (size_t i = 0; i < m_pos && i < m_json.size(); ++i) {
                if (m_json[i] == '\n') { line++; col = 1; }
                else col++;
            }
            return JsonParseException(msg, line, col);
        }

        void skip_whitespace() {
            while (m_pos < m_json.size()) {
                char c = m_json[m_pos];
                if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                    m_pos++;
                } else if (c == '/' && m_opts.allow_comments) {
                    if (m_pos + 1 < m_json.size()) {
                        if (m_json[m_pos+1] == '/') {
                            m_pos += 2;
                            while (m_pos < m_json.size() && m_json[m_pos] != '\n') m_pos++;
                        } else if (m_json[m_pos+1] == '*') {
                            m_pos += 2;
                            while (m_pos + 1 < m_json.size() && !(m_json[m_pos] == '*' && m_json[m_pos+1] == '/')) m_pos++;
                            m_pos += 2;
                        } else {
                            break;
                        }
                    } else break;
                } else {
                    break;
                }
            }
        }

        Json parse_value() {
            skip_whitespace();
            char c = peek();
            if (c == '{') return parse_object();
            if (c == '[') return parse_array();
            if (c == '"') return parse_string();
            if (c == 't') return parse_true();
            if (c == 'f') return parse_false();
            if (c == 'n') return parse_null();
            if (c == '-' || (c >= '0' && c <= '9')) return parse_number();
            throw error(std::format("Unexpected character '{}'", c));
        }

        Json parse_object() {
            advance(); // {
            ObjectMap obj;
            skip_whitespace();
            if (match('}')) return obj;

            while (true) {
                skip_whitespace();
                std::string key = parse_string_raw();
                skip_whitespace();
                if (!match(':')) throw error("Expected ':'");

                obj.emplace(std::move(key), parse_value());

                skip_whitespace();
                if (match('}')) break;
                if (!match(',')) throw error("Expected ',' or '}'");

                if (m_opts.allow_trailing_commas) {
                    skip_whitespace();
                    if (match('}')) break;
                }
            }
            // IMPORTANT: Sort to ensure O(log N) lookups
            obj.sort();
            return Json(std::move(obj));
        }

        Json parse_array() {
            advance(); // [
            std::vector<Json> arr;
            skip_whitespace();
            if (match(']')) return arr;

            while (true) {
                arr.push_back(parse_value());
                skip_whitespace();
                if (match(']')) break;
                if (!match(',')) throw error("Expected ',' or ']'");

                if (m_opts.allow_trailing_commas) {
                    skip_whitespace();
                    if (match(']')) break;
                }
            }
            return Json(std::move(arr));
        }

        std::string parse_string_raw() {
            if (!match('"')) throw error("Expected '\"'");
            std::string res;
            res.reserve(16);
            while (m_pos < m_json.size()) {
                char c = m_json[m_pos++];
                if (c == '"') return res;
                if (c == '\\') {
                    if (m_pos >= m_json.size()) throw error("Unexpected end of string");
                    char esc = m_json[m_pos++];
                    switch (esc) {
                        case '"': res += '"'; break;
                        case '\\': res += '\\'; break;
                        case '/': res += '/'; break;
                        case 'b': res += '\b'; break;
                        case 'f': res += '\f'; break;
                        case 'n': res += '\n'; break;
                        case 'r': res += '\r'; break;
                        case 't': res += '\t'; break;
                        case 'u': {
                            // Unicode Escape Logic with Surrogate Pairs
                            if (m_pos + 4 > m_json.size()) throw error("Invalid unicode escape");
                            std::string_view hex = m_json.substr(m_pos, 4);
                            m_pos += 4;
                            int code = 0;
                            std::from_chars(hex.data(), hex.data()+4, code, 16);

                            // Check for High Surrogate
                            if (code >= 0xD800 && code <= 0xDBFF) {
                                if (m_pos + 6 > m_json.size() || m_json[m_pos] != '\\' || m_json[m_pos+1] != 'u') {
                                    throw error("Expected low surrogate");
                                }
                                m_pos += 2;
                                std::string_view hex2 = m_json.substr(m_pos, 4);
                                m_pos += 4;
                                int low = 0;
                                std::from_chars(hex2.data(), hex2.data()+4, low, 16);
                                if (low < 0xDC00 || low > 0xDFFF) throw error("Invalid low surrogate");

                                code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                            }

                            if (code < 0x80) res += (char)code;
                            else if (code < 0x800) {
                                res += (char)(0xC0 | (code >> 6));
                                res += (char)(0x80 | (code & 0x3F));
                            } else if (code < 0x10000) {
                                res += (char)(0xE0 | (code >> 12));
                                res += (char)(0x80 | ((code >> 6) & 0x3F));
                                res += (char)(0x80 | (code & 0x3F));
                            } else {
                                res += (char)(0xF0 | (code >> 18));
                                res += (char)(0x80 | ((code >> 12) & 0x3F));
                                res += (char)(0x80 | ((code >> 6) & 0x3F));
                                res += (char)(0x80 | (code & 0x3F));
                            }
                            break;
                        }
                        default: throw error("Invalid escape sequence");
                    }
                } else {
                    res += c;
                }
            }
            throw error("Unterminated string");
        }

        Json parse_string() {
            return Json(parse_string_raw());
        }

        Json parse_number() {
            size_t start = m_pos;
            if (peek() == '-') advance();
            while (isdigit(peek())) advance();
            bool is_float = false;
            if (peek() == '.') {
                is_float = true;
                advance();
                while (isdigit(peek())) advance();
            }
            if (peek() == 'e' || peek() == 'E') {
                is_float = true;
                advance();
                if (peek() == '+' || peek() == '-') advance();
                while (isdigit(peek())) advance();
            }

            std::string_view num_str = m_json.substr(start, m_pos - start);
            if (is_float) {
                double val;
                std::from_chars(num_str.data(), num_str.data() + num_str.size(), val);
                return Json(val);
            } else {
                int64_t val;
                std::from_chars(num_str.data(), num_str.data() + num_str.size(), val);
                return Json(val);
            }
        }

        Json parse_true() {
            if (m_json.substr(m_pos, 4) == "true") { m_pos += 4; return Json(true); }
            throw error("Expected true");
        }
        Json parse_false() {
            if (m_json.substr(m_pos, 5) == "false") { m_pos += 5; return Json(false); }
            throw error("Expected false");
        }
        Json parse_null() {
            if (m_json.substr(m_pos, 4) == "null") { m_pos += 4; return Json(nullptr); }
            throw error("Expected null");
        }
    };

    class Serializer {
        std::string m_out;
        const DumpOptions& m_opts;
        int m_depth = 0;

        void indent() {
            if (m_opts.indent >= 0) {
                m_out += '\n';
                m_out.append(m_depth * m_opts.indent, m_opts.indent_char);
            }
        }

    public:
        Serializer(const DumpOptions& opts) : m_opts(opts) {}

        std::string run(const Json& j) {
            visit(j);
            return m_out;
        }

        void visit(const Json& j) {
            switch(j.type()) {
                case Type::Null: m_out += "null"; break;
                case Type::Boolean: m_out += (j.get<bool>() ? "true" : "false"); break;
                case Type::NumberInt: m_out += std::to_string(j.get<int64_t>()); break;
                case Type::NumberFloat: {
                    char buf[64];
                    auto res = std::to_chars(buf, buf+64, j.get<double>());
                    m_out.append(buf, res.ptr);
                    break;
                }
                case Type::String: dump_string(j.get_ref<std::string>()); break; // Zero copy
                case Type::Array: {
                    m_out += '[';
                    const auto& arr = j.get_ref<Json::array_t>(); // Zero copy
                    if (!arr.empty()) {
                        m_depth++;
                        for (size_t i = 0; i < arr.size(); ++i) {
                            indent();
                            visit(arr[i]);
                            if (i < arr.size() - 1) m_out += ',';
                        }
                        m_depth--;
                        indent();
                    }
                    m_out += ']';
                    break;
                }
                case Type::Object: {
                    m_out += '{';
                    const auto& obj = j.get_ref<Json::object_t>(); // Zero copy
                    if (!obj.empty()) {
                        m_depth++;
                        size_t i = 0;
                        for (const auto& [k, v] : obj) {
                            indent();
                            dump_string(k);
                            m_out += ": ";
                            visit(v);
                            if (i < obj.size() - 1) m_out += ',';
                            i++;
                        }
                        m_depth--;
                        indent();
                    }
                    m_out += '}';
                    break;
                }
            }
        }

        void dump_string(const std::string& s) {
            m_out += '"';
            for (char c : s) {
                switch (c) {
                    case '"': m_out += "\\\""; break;
                    case '\\': m_out += "\\\\"; break;
                    case '\b': m_out += "\\b"; break;
                    case '\f': m_out += "\\f"; break;
                    case '\n': m_out += "\\n"; break;
                    case '\r': m_out += "\\r"; break;
                    case '\t': m_out += "\\t"; break;
                    default:
                        if (static_cast<unsigned char>(c) < 0x20) {
                            char buf[7];
                            snprintf(buf, 7, "\\u%04x", c);
                            m_out += buf;
                        } else {
                            m_out += c;
                        }
                }
            }
            m_out += '"';
        }
    };
} // namespace Internal

inline Json Json::parse(std::string_view json, const ParseOptions& opts) {
    Internal::Parser p(json, opts);
    return p.parse();
}

inline std::string Json::dump(const DumpOptions& opts) const {
    Internal::Serializer s(opts);
    return s.run(*this);
}

} // namespace Tachyon

#endif // TACHYON_HPP
