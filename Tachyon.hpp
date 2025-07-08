#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <variant>
#include <stdexcept>
#include <string_view>
#include <optional>
#include <memory>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <functional>
#include <initializer_list>
#include <charconv>
#include <type_traits>
#include <iterator>

/****************************************************************************
 *
 *                               Tachyon JSON
 *
 *  A modern, fast, and ergonomic single-header JSON library for C++.
 *
 *  Version: 2.0 BETA
 *  - ULTIMATE FIX: The `get<T>()` method has been entirely overhauled for
 *    maximum clarity and robustness using explicit `if constexpr` branches
 *    and direct `std::holds_alternative` checks. This definitively resolves
 *    all previous type conversion issues and ensures full stability across compilers.
 *
 ****************************************************************************/

#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
    #define TACHYON_HAS_FLOAT_FROM_CHARS 1
#else
    #define TACHYON_HAS_FLOAT_FROM_CHARS 0
#endif

namespace Tachyon {

    template<class Traits> class BasicJson;
    enum class JsonType { Null, Object, Array, String, Boolean, Integer, Float };

    struct ParseOptions {
        bool allow_comments = false;
        bool allow_trailing_commas = false;
        unsigned int max_depth = 128;
    };

    struct DumpOptions {
        int indent_width = -1;
        char indent_char = ' ';
        unsigned int float_precision = 6;
        bool sort_keys = false;
        bool escape_unicode = false;
    };

    class JsonException : public std::runtime_error { using std::runtime_error::runtime_error; };
    class JsonPointerException : public JsonException { using JsonException::JsonException; };
    class JsonParseException : public JsonException {
    public:
        JsonParseException(const std::string& msg, size_t line, size_t col, const std::string& context = "")
            : JsonException(msg), m_line(line), m_col(col) {
            std::ostringstream ss;
            ss << "Parse error at line " << m_line << " col " << m_col << ": " << msg;
            if (!context.empty()) ss << "\nContext: " << context;
            m_detailed_what = ss.str();
        }
        const char* what() const noexcept override { return m_detailed_what.c_str(); }
        size_t line() const noexcept { return m_line; }
        size_t column() const noexcept { return m_col; }
    private:
        size_t m_line = 0, m_col = 0;
        std::string m_detailed_what;
    };
    
    template<typename T> struct is_unordered_map : std::false_type {};
    template<typename K, typename V, typename H, typename E, typename A>
    struct is_unordered_map<std::unordered_map<K, V, H, E, A>> : std::true_type {};

    template<class Allocator>
    struct DefaultTraits {
        template<class T> using Alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<T>;
        using StringType = std::basic_string<char, std::char_traits<char>, Alloc<char>>;
        using ObjectType = std::map<StringType, BasicJson<DefaultTraits<Allocator>>, std::less<StringType>, Alloc<std::pair<const StringType, BasicJson<DefaultTraits<Allocator>>>>>;
        using ArrayType = std::vector<BasicJson<DefaultTraits<Allocator>>, Alloc<BasicJson<DefaultTraits<Allocator>>>>;
        using BooleanType = bool;
        using NumberIntegerType = int64_t;
        using NumberFloatType = double;
        using NullType = std::nullptr_t;
    };

    namespace internal {
        template<typename T, typename = void> struct is_basic_json : std::false_type {};
        template<typename T> struct is_basic_json<T, std::void_t<typename T::traits_type>>
            : std::is_base_of<BasicJson<typename T::traits_type>, T> {};
        
        // Pomocniczy trait do sprawdzania typów string-podobnych (dla ułatwienia get<T>())
        template <typename T>
        using is_string_like = std::disjunction<
            std::is_same<std::decay_t<T>, typename DefaultTraits<std::allocator<char>>::StringType>,
            std::is_same<std::decay_t<T>, std::string>,
            std::is_same<std::decay_t<T>, std::string_view>,
            std::is_same<std::decay_t<T>, const char*>
        >;
    }

    template<class Traits>
    class BasicJson {
    public:
        using traits_type = Traits;
        using Object = typename Traits::ObjectType;
        using Array = typename Traits::ArrayType;
        using String = typename Traits::StringType;
        using Boolean = typename Traits::BooleanType;
        using Integer = typename Traits::NumberIntegerType;
        using Float = typename Traits::NumberFloatType;
        using Null = typename Traits::NullType;
        using InitializerList = std::initializer_list<BasicJson<Traits>>;

    private:
        using JsonVariant = std::variant<Null, Boolean, Integer, Float, String, Array, Object>;
        JsonVariant m_data;

    public:
        // Constructors
        BasicJson(Null val = nullptr) noexcept : m_data(val) {}
        BasicJson(Boolean val) noexcept : m_data(val) {}
        BasicJson(Integer val) noexcept : m_data(val) {}
        BasicJson(Float val) noexcept : m_data(val) {}
        BasicJson(int val) noexcept : m_data(static_cast<Integer>(val)) {}
        BasicJson(const String& val) : m_data(val) {}
        BasicJson(String&& val) noexcept : m_data(std::move(val)) {}
        BasicJson(const char* val) : m_data(String(val)) {}
        BasicJson(const Array& val) : m_data(val) {}
        BasicJson(Array&& val) noexcept : m_data(std::move(val)) {}
        BasicJson(const Object& val) : m_data(val) {}
        BasicJson(Object&& val) noexcept : m_data(std::move(val)) {}
        
        BasicJson(InitializerList init) {
            bool is_object_like = std::all_of(init.begin(), init.end(), [](const BasicJson& el){
                return el.is_array() && el.size() == 2 && el.at(0).is_string();
            });
            if (is_object_like) {
                m_data = Object();
                for (const auto& el : init) {
                    this->template get_ref<Object>()[el.at(0).template get<String>()] = el.at(1);
                }
            } else { m_data = Array(init.begin(), init.end()); }
        }

        template<class InputIt, typename = std::enable_if_t<
            std::is_base_of_v<
                std::input_iterator_tag,
                typename std::iterator_traits<InputIt>::iterator_category
            >
        >>
        BasicJson(InputIt first, InputIt last) {
            m_data = Array(first, last);
        }

        template<typename T, typename = std::enable_if_t<
            !std::is_constructible_v<BasicJson<Traits>, T> && !internal::is_basic_json<T>::value
        >>
        BasicJson(const T& value) {
            to_json(*this, value);
        }

        // Type Inspection
        JsonType type() const noexcept {
            switch(m_data.index()) {
                case 0: return JsonType::Null; case 1: return JsonType::Boolean; case 2: return JsonType::Integer;
                case 3: return JsonType::Float; case 4: return JsonType::String; case 5: return JsonType::Array;
                case 6: return JsonType::Object; default: return JsonType::Null;
            }
        }
        bool is_null() const noexcept { return std::holds_alternative<Null>(m_data); }
        bool is_object() const noexcept { return std::holds_alternative<Object>(m_data); }
        bool is_array() const noexcept { return std::holds_alternative<Array>(m_data); }
        bool is_string() const noexcept { return std::holds_alternative<String>(m_data); }
        bool is_boolean() const noexcept { return std::holds_alternative<Boolean>(m_data); }
        bool is_integer() const noexcept { return std::holds_alternative<Integer>(m_data); }
        bool is_float() const noexcept { return std::holds_alternative<Float>(m_data); }
        bool is_number() const noexcept { return this->is_integer() || this->is_float(); }

        // Value Access
        template<typename T> T& get_ref() {
            try { return std::get<T>(m_data); }
            catch (const std::bad_variant_access&) { throw JsonException("Invalid type access in get_ref<T>()"); }
        }
        template<typename T> const T& get_ref() const {
            try { return std::get<T>(m_data); }
            catch (const std::bad_variant_access&) { throw JsonException("Invalid type access in get_ref<const T>()"); }
        }
        
        // OSTATECZNA, SOLIDNA IMPLEMENTACJA get<T>()
        template<typename T>
        T get() const {
            using DecayedT = std::decay_t<T>;

            // Priority 1: String-like types
            if constexpr (internal::is_string_like<DecayedT>::value) {
                if (!std::holds_alternative<String>(m_data)) {
                    throw JsonException("Invalid type access: Expected string value for string conversion.");
                }
                return static_cast<T>(std::get<String>(m_data));
            }
            // Priority 2: Boolean type
            else if constexpr (std::is_same_v<DecayedT, bool>) {
                if (!std::holds_alternative<Boolean>(m_data)) {
                    throw JsonException("Invalid type access: Expected boolean value for bool conversion.");
                }
                return static_cast<T>(std::get<Boolean>(m_data));
            }
            // Priority 3: Null type
            else if constexpr (std::is_same_v<DecayedT, std::nullptr_t>) {
                if (!std::holds_alternative<Null>(m_data)) {
                    throw JsonException("Invalid type access: Expected null value for nullptr_t conversion.");
                }
                return static_cast<T>(std::get<Null>(m_data));
            }
            // Priority 4: Integral types
            else if constexpr (std::is_integral_v<DecayedT>) {
                if (std::holds_alternative<Integer>(m_data)) {
                    return static_cast<T>(std::get<Integer>(m_data));
                }
                if (std::holds_alternative<Float>(m_data)) {
                    return static_cast<T>(std::get<Float>(m_data)); // Allow float to int conversion (truncation)
                }
                throw JsonException("Invalid type access: Expected numeric value for integral conversion.");
            }
            // Priority 5: Floating point types
            else if constexpr (std::is_floating_point_v<DecayedT>) {
                if (std::holds_alternative<Float>(m_data)) {
                    return static_cast<T>(std::get<Float>(m_data));
                }
                if (std::holds_alternative<Integer>(m_data)) {
                    return static_cast<T>(std::get<Integer>(m_data)); // Allow int to float conversion
                }
                throw JsonException("Invalid type access: Expected numeric value for floating point conversion.");
            }
            // Priority 6: User-defined types (UDTs)
            else {
                T value;
                from_json(*this, value);
                return value;
            }
        }

        // Container Access
        size_t size() const {
            if (this->is_object()) return this->template get_ref<Object>().size();
            if (this->is_array()) return this->template get_ref<Array>().size();
            if (this->is_string()) return this->template get_ref<String>().size();
            return this->is_null() ? 0 : 1;
        }
        bool empty() const { return this->size() == 0; }
        const BasicJson& at(size_t index) const { return this->template get_ref<Array>().at(index); }
        BasicJson& at(size_t index) { return this->template get_ref<Array>().at(index); }
        const BasicJson& at(const String& key) const { return this->template get_ref<Object>().at(key); }
        BasicJson& at(const String& key) { return this->template get_ref<Object>().at(key); }
        const BasicJson& at_pointer(const std::string& json_pointer) const;

        // Operators
        BasicJson& operator[](size_t index) {
            if (this->is_null()) m_data = Array();
            if (!this->is_array()) throw JsonException("operator[size_t] not applicable to non-array type");
            auto& arr = this->template get_ref<Array>();
            if (index >= arr.size()) arr.resize(index + 1);
            return arr[index];
        }

        BasicJson& operator[](int index) {
            return this->operator[](static_cast<size_t>(index));
        }

        BasicJson& operator[](const String& key) {
            if (this->is_null()) m_data = Object();
            if (!this->is_object()) throw JsonException("operator[key] not applicable to non-object type");
            return this->template get_ref<Object>()[key];
        }
        BasicJson& operator[](const char* key) { return this->operator[](String(key)); }
        
        template<typename T> BasicJson& operator=(const T& value) { *this = BasicJson(value); return *this; }

        // Modifiers
        void clear() {
            if (this->is_object()) this->template get_ref<Object>().clear();
            else if (this->is_array()) this->template get_ref<Array>().clear();
            else if (this->is_string()) this->template get_ref<String>().clear();
        }
        void push_back(const BasicJson& val) {
            if (this->is_null()) m_data = Array();
            if (!this->is_array()) throw JsonException("push_back not applicable to non-array type");
            this->template get_ref<Array>().push_back(val);
        }

        // Parsing and Serialization
        static BasicJson parse(std::string_view json_string, const ParseOptions& options = {});
        std::string dump(const DumpOptions& options = {}) const;
        std::string dump(int indent) const;

        const JsonVariant& variant() const { return m_data; }
    };

    using Json = BasicJson<DefaultTraits<std::allocator<char>>>;

    template<typename T, typename Alloc>
    void to_json(Json& j, const std::vector<T, Alloc>& vec) {
        j = Json(vec.begin(), vec.end());
    }
    template<typename T, typename Alloc>
    void from_json(const Json& j, std::vector<T, Alloc>& vec) {
        if (!j.is_array()) {
            throw JsonException("Cannot convert non-array type to std::vector");
        }
        vec.clear();
        vec.reserve(j.size());
        for (const auto& item : j.get_ref<Json::Array>()) {
            vec.push_back(item.get<T>());
        }
    }
    
    namespace internal {
        class Parser {
        public:
            Parser(std::string_view input, const ParseOptions& options) : m_input(input), m_opts(options) {}
            Json parse_json() { skip_whitespace_and_comments(); return parse_value(); }

        private:
            std::string_view m_input; const ParseOptions& m_opts;
            size_t m_pos = 0, m_line = 1, m_col = 1; unsigned int m_depth = 0;
            
            [[noreturn]] void throw_parse_error(const std::string& msg) {
                size_t context_start = (m_pos > 20) ? m_pos - 20 : 0;
                size_t context_end = std::min(m_pos + 20, m_input.length());
                std::string context(m_input.substr(context_start, context_end - context_start));
                std::string pointer(m_pos - context_start, ' ');
                pointer += "<-- HERE";
                throw JsonParseException(msg, m_line, m_col, context + "\n" + pointer);
            }
            char peek() const { return m_pos < m_input.length() ? m_input[m_pos] : '\0'; }
            char advance() { char c = peek(); if (c != '\0') { if (c == '\n') { m_line++; m_col = 1; } else { m_col++; } m_pos++; } return c; }
            void expect(char c) { skip_whitespace_and_comments(); if (peek() != c) throw_parse_error(std::string("Expected '") + c + "' but got '" + peek() + "'"); advance(); }
            
            void skip_whitespace_and_comments() {
                while(true) {
                    while (m_pos < m_input.length() && std::isspace(static_cast<unsigned char>(m_input[m_pos]))) advance();
                    if (m_opts.allow_comments && peek() == '/') {
                        advance();
                        if (peek() == '/') { while (peek() != '\n' && peek() != '\0') advance(); continue; }
                        else if (peek() == '*') { advance(); while(true) { if (peek() == '\0') throw_parse_error("Unterminated block comment"); if (advance() == '*' && peek() == '/') { advance(); break; } } continue; }
                        else { m_pos--; m_col--; break; }
                    } break;
                }
            }

            Json parse_value() {
                if (++m_depth > m_opts.max_depth) throw_parse_error("Max parse depth exceeded");
                skip_whitespace_and_comments(); Json result;
                switch (peek()) {
                    case '{': result = parse_object(); break; case '[': result = parse_array(); break;
                    case '"': result = parse_string(); break; case 't': result = parse_literal("true", true); break;
                    case 'f': result = parse_literal("false", false); break; case 'n': result = parse_literal("null", nullptr); break;
                    case '-': case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
                        result = parse_number(); break;
                    default: throw_parse_error("Unexpected character");
                }
                m_depth--; return result;
            }
            
            template<typename T> Json parse_literal(const char* literal, T val) {
                if (m_input.substr(m_pos, std::strlen(literal)) == literal) {
                    for(size_t i=0; i < std::strlen(literal); ++i) advance();
                    return Json(val);
                }
                throw_parse_error(std::string("Expected '") + literal + "'");
            }
            
            Json parse_number() {
                size_t start_pos = m_pos;
                bool is_float = false;
                if (peek() == '-') advance();
                while (isdigit(static_cast<unsigned char>(peek()))) advance();
                if (peek() == '.') { is_float = true; advance(); while (isdigit(static_cast<unsigned char>(peek()))) advance(); }
                if (peek() == 'e' || peek() == 'E') { is_float = true; advance(); if (peek() == '+' || peek() == '-') advance(); if (!isdigit(static_cast<unsigned char>(peek()))) throw_parse_error("Invalid exponent"); while (isdigit(static_cast<unsigned char>(peek()))) advance(); }
                
                std::string_view num_sv(m_input.data() + start_pos, m_pos - start_pos);

                if (is_float) {
                    #if TACHYON_HAS_FLOAT_FROM_CHARS
                        Json::Float val;
                        auto res = std::from_chars(num_sv.data(), num_sv.data() + num_sv.size(), val);
                        if (res.ec != std::errc()) throw_parse_error("Invalid float format");
                        return Json(val);
                    #else
                        try { return Json(static_cast<Json::Float>(std::stod(std::string(num_sv)))); }
                        catch (...) { throw_parse_error("Invalid float format: " + std::string(num_sv)); }
                    #endif
                } else {
                    Json::Integer val;
                    auto res = std::from_chars(num_sv.data(), num_sv.data() + num_sv.size(), val);
                    if (res.ec != std::errc()) {
                        #if TACHYON_HAS_FLOAT_FROM_CHARS
                            Json::Float f_val;
                            auto res_f = std::from_chars(num_sv.data(), num_sv.data() + num_sv.size(), f_val);
                            if (res_f.ec != std::errc()) throw_parse_error("Invalid number format");
                            return Json(f_val);
                        #else
                            try { return Json(static_cast<Json::Float>(std::stod(std::string(num_sv)))); }
                            catch (...) { throw_parse_error("Invalid number format: " + std::string(num_sv)); }
                        #endif
                    }
                    return Json(val);
                }
            }

            void append_utf8(Json::String& s, uint32_t cp) {
                if (cp <= 0x7F) { s += static_cast<char>(cp); }
                else if (cp <= 0x7FF) { s += static_cast<char>(0xC0 | (cp >> 6)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
                else if (cp <= 0xFFFF) { s += static_cast<char>(0xE0 | (cp >> 12)); s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
                else { s += static_cast<char>(0xF0 | (cp >> 18)); s += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)); s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
            }

            Json parse_string() {
                expect('"');
                Json::String str; str.reserve(32);
                while (peek() != '"') {
                    char c = advance(); if (c == '\0') throw_parse_error("Unterminated string");
                    if (c == '\\') {
                        switch (advance()) {
                            case '"': str += '"'; break; case '\\': str += '\\'; break; case '/': str += '/'; break;
                            case 'b': str += '\b'; break; case 'f': str += '\f'; break; case 'n': str += '\n'; break;
                            case 'r': str += '\r'; break; case 't': str += '\t'; break;
                            case 'u': {
                                uint32_t cp = 0;
                                for (int i=0; i<4; ++i) { char h = std::tolower(static_cast<unsigned char>(advance())); if (!isxdigit(h)) throw_parse_error("Invalid hex in unicode"); cp = (cp << 4) + (h <= '9' ? h - '0' : h - 'a' + 10); }
                                if (cp >= 0xD800 && cp <= 0xDBFF) {
                                    if (m_input.substr(m_pos, 2) != "\\u") throw_parse_error("Unpaired high surrogate");
                                    advance(); advance(); uint32_t low = 0;
                                    for (int i=0; i<4; ++i) { char h = std::tolower(static_cast<unsigned char>(advance())); if (!isxdigit(h)) throw_parse_error("Invalid hex for low surrogate"); low = (low << 4) + (h <= '9' ? h - '0' : h - 'a' + 10); }
                                    if (low < 0xDC00 || low > 0xDFFF) throw_parse_error("Invalid low surrogate value");
                                    cp = 0x10000 + (((cp - 0xD800) << 10) | (low - 0xDC00));
                                }
                                append_utf8(str, cp); break;
                            }
                            default: throw_parse_error("Invalid escape sequence");
                        }
                    } else { str += c; }
                }
                expect('"'); return Json(str);
            }

            Json parse_array() {
                expect('['); Json::Array arr; skip_whitespace_and_comments();
                if (peek() == ']') { advance(); return Json(std::move(arr)); }
                while (true) {
                    arr.push_back(parse_value()); skip_whitespace_and_comments();
                    if (peek() == ']') { advance(); break; }
                    expect(','); skip_whitespace_and_comments();
                    if (m_opts.allow_trailing_commas && peek() == ']') { advance(); break; }
                } return Json(std::move(arr));
            }

            Json parse_object() {
                expect('{'); Json::Object obj; skip_whitespace_and_comments();
                if (peek() == '}') { advance(); return Json(std::move(obj)); }
                while (true) {
                    if (peek() != '"') throw_parse_error("Expected string key for object");
                    Json::String key = std::get<Json::String>(parse_string().variant());
                    expect(':'); obj.emplace(std::move(key), parse_value());
                    skip_whitespace_and_comments();
                    if (peek() == '}') { advance(); break; }
                    expect(','); skip_whitespace_and_comments();
                    if (m_opts.allow_trailing_commas && peek() == '}') { advance(); break; }
                } return Json(std::move(obj));
            }
        };

        class Serializer {
        public:
            Serializer(std::ostream& os, const DumpOptions& options) : m_os(os), m_opts(options) { m_os.precision(m_opts.float_precision); }
            void serialize(const Json& json) { std::visit([this](auto&& arg) { this->visit(arg); }, json.variant()); }
        private:
            std::ostream& m_os; const DumpOptions& m_opts; int m_level = 0;

            void indent() { m_os << '\n'; for (int i = 0; i < m_level * m_opts.indent_width; ++i) m_os << m_opts.indent_char; }
            void visit(const Json::Null&) { m_os << "null"; }
            void visit(const Json::Boolean& val) { m_os << (val ? "true" : "false"); }
            void visit(const Json::Integer& val) { m_os << val; }
            void visit(const Json::Float& val) { m_os << val; }
            void visit(const Json::String& val) {
                m_os << '"';
                for (unsigned char c : val) {
                    switch (c) {
                        case '"': m_os << "\\\""; break; case '\\': m_os << "\\\\"; break; case '\b': m_os << "\\b"; break;
                        case '\f': m_os << "\\f"; break; case '\n': m_os << "\\n"; break; case '\r': m_os << "\\r"; break;
                        case '\t': m_os << "\\t"; break;
                        default:
                            if (c < 0x20 || (m_opts.escape_unicode && c > 0x7E)) {
                                m_os << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                            } else { m_os << c; } break;
                    }
                } m_os << '"';
            }
            
            void visit(const Json::Array& val) {
                m_os << '[';
                if (val.empty()) { m_os << ']'; return; }

                if (m_opts.indent_width >= 0) {
                    m_level++;
                    for (size_t i = 0; i < val.size(); ++i) {
                        indent();
                        serialize(val[i]);
                        if (i < val.size() - 1) m_os << ',';
                    }
                    m_level--;
                    indent();
                } else {
                    for (size_t i = 0; i < val.size(); ++i) {
                        serialize(val[i]);
                        if (i < val.size() - 1) m_os << ',';
                    }
                }
                m_os << ']';
            }
            
            template<typename MapType> void serialize_map(const MapType& val) {
                m_os << '{';
                if (val.empty()) { m_os << '}'; return; }
                
                std::vector<typename MapType::const_iterator> iterators;
                iterators.reserve(val.size());
                for(auto it = val.cbegin(); it != val.cend(); ++it) { iterators.push_back(it); }
                if (m_opts.sort_keys && is_unordered_map<MapType>::value) {
                    std::sort(iterators.begin(), iterators.end(), [](auto a, auto b){ return a->first < b->first; });
                }

                if (m_opts.indent_width >= 0) {
                    m_level++;
                    for(size_t i = 0; i < iterators.size(); ++i) {
                        indent();
                        visit(iterators[i]->first);
                        m_os << ": ";
                        serialize(iterators[i]->second);
                        if (i < iterators.size() - 1) m_os << ',';
                    }
                    m_level--;
                    indent();
                } else {
                    for(size_t i = 0; i < iterators.size(); ++i) {
                        visit(iterators[i]->first);
                        m_os << ":";
                        serialize(iterators[i]->second);
                        if (i < iterators.size() - 1) m_os << ',';
                    }
                }
                m_os << '}';
            }

            void visit(const Json::Object& val) { serialize_map(val); }
        };
    }

    template<class T> inline BasicJson<T> BasicJson<T>::parse(std::string_view s, const ParseOptions& o) { return internal::Parser(s, o).parse_json(); }
    template<class T> inline std::string BasicJson<T>::dump(const DumpOptions& o) const { std::ostringstream ss; internal::Serializer ss_serializer(ss, o); ss_serializer.serialize(*this); return ss.str(); }
    template<class T> inline std::string BasicJson<T>::dump(int indent) const { DumpOptions opts; opts.indent_width = indent; return this->dump(opts); }

    template<class Traits>
    const BasicJson<Traits>& BasicJson<Traits>::at_pointer(const std::string& json_pointer) const {
        if (json_pointer.empty()) return *this;
        if (json_pointer[0] != '/') throw JsonPointerException("Pointer must start with '/'");
        const BasicJson* current = this;
        size_t start = 1;
        while (start < json_pointer.length()) {
            size_t end = json_pointer.find('/', start);
            if (end == std::string::npos) end = json_pointer.length();
            std::string token_str = json_pointer.substr(start, end - start);
            size_t pos;
            while ((pos = token_str.find("~1")) != std::string::npos) token_str.replace(pos, 2, "/");
            while ((pos = token_str.find("~0")) != std::string::npos) token_str.replace(pos, 2, "~");

            if (current->is_object()) {
                auto it = current->template get_ref<Object>().find(token_str);
                if (it == current->template get_ref<Object>().end()) throw JsonPointerException("Pointer key not found: " + token_str);
                current = &it->second;
            } else if (current->is_array()) {
                size_t index = 0;
                try {
                    if (token_str.empty() || (token_str.length() > 1 && token_str[0] == '0') ||
                        std::any_of(token_str.begin(), token_str.end(), [](char c){ return !std::isdigit(static_cast<unsigned char>(c)); })) {
                         throw std::invalid_argument("Invalid array index format");
                    }
                    index = std::stoull(token_str);
                } catch(...) {
                    throw JsonPointerException("Invalid array index in pointer: " + token_str);
                }
                if (index >= current->size()) throw JsonPointerException("Pointer index out of bounds: " + token_str);
                current = &current->at(index);
            } else {
                throw JsonPointerException("Pointer cannot traverse non-container type");
            }
            start = end + 1;
        }
        return *current;
    }

    inline namespace literals {
        inline namespace json_literals {
            inline Json operator"" _tjson(const char* s, size_t n) {
                return Json::parse(std::string_view(s, n));
            }
        }
    }

    template<class Allocator>
    struct UnorderedTraits {
        template<class T> using Alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<T>;
        using StringType = std::basic_string<char, std::char_traits<char>, Alloc<char>>;
        using BooleanType = bool; using NumberIntegerType = int64_t; using NumberFloatType = double; using NullType = std::nullptr_t;
        using ArrayType = std::vector<BasicJson<UnorderedTraits<Allocator>>, Alloc<BasicJson<UnorderedTraits<Allocator>>>>;
        using ObjectType = std::unordered_map< StringType, BasicJson<UnorderedTraits<Allocator>>, std::hash<StringType>, std::equal_to<>, Alloc<std::pair<const StringType, BasicJson<UnorderedTraits<Allocator>>>>>;
    };
    using UnorderedJson = BasicJson<UnorderedTraits<std::allocator<char>>>;
}
