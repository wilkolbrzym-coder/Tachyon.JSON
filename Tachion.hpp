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
#include <cstdlib>
#include <iomanip>
#include <functional>
#include <initializer_list>

namespace Tachyon {

    // --- Forward Declarations ---
    template<class Traits>
    class BasicJson;

    // --- Configuration Structs ---
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

    // --- Exception Classes ---
    class JsonException : public std::runtime_error { using std::runtime_error::runtime_error; };
    class JsonParseException : public JsonException {
    public:
        JsonParseException(const std::string& msg, size_t line, size_t col)
            : JsonException(msg), m_line(line), m_col(col) {
            m_detailed_what = "Parse error at line " + std::to_string(m_line) + " col " + std::to_string(m_col) + ": " + msg;
        }
        const char* what() const noexcept override { return m_detailed_what.c_str(); }
        size_t line() const noexcept { return m_line; }
        size_t column() const noexcept { return m_col; }
    private:
        size_t m_line = 0, m_col = 0;
        std::string m_detailed_what;
    };
    class JsonPointerException : public JsonException { using JsonException::JsonException; };
    
    // --- Type Traits ---
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

    // --- Main JSON Class Template ---
    enum class JsonType { Null, Object, Array, String, Boolean, Integer, Float };

    template<class Traits = DefaultTraits<std::allocator<char>>>
    class BasicJson {
    public:
        using Object = typename Traits::ObjectType;
        using Array = typename Traits::ArrayType;
        using String = typename Traits::StringType;
        using Boolean = typename Traits::BooleanType;
        using Integer = typename Traits::NumberIntegerType;
        using Float = typename Traits::NumberFloatType;
        using Null = typename Traits::NullType;
        using InitializerList = std::initializer_list<BasicJson>;

    private:
        using JsonVariant = std::variant<Null, Boolean, Integer, Float, String, Array, Object>;
        JsonVariant m_data;

    public:
        // --- Deklaracje wczesne dla konstruktora initializer_list ---
        bool is_null() const noexcept { return std::holds_alternative<Null>(m_data); }
        bool is_array() const noexcept { return std::holds_alternative<Array>(m_data); }
        bool is_string() const noexcept { return std::holds_alternative<String>(m_data); }
        bool is_object() const noexcept { return std::holds_alternative<Object>(m_data); }
        
        template<typename T> const T& get() const {
            try { 
                // KLUCZOWA POPRAWKA #1: Usunięto 'const' z typu T wewnątrz std::get.
                // std::get<T> na stałym wariancie automatycznie zwróci const T&.
                // Poprzednia wersja, std::get<const T>, była nieprawidłowa.
                return std::get<T>(m_data); 
            }
            catch (const std::bad_variant_access&) { throw JsonException("Invalid type access in Json::get<const T>()"); }
        }
        template<typename T> T& get() {
            try { return std::get<T>(m_data); }
            catch (const std::bad_variant_access&) { throw JsonException("Invalid type access in Json::get<T>()"); }
        }
        
        size_t size() const {
            if (is_object()) return get<Object>().size();
            if (is_array()) return get<Array>().size();
            if (is_string()) return get<String>().size();
            return is_null() ? 0 : 1;
        }
        const BasicJson& at(size_t index) const {
            if (!is_array()) throw JsonException("at(index) not applicable to non-array type");
            return get<Array>().at(index);
        }

        // --- Constructors ---
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
                for (const auto& el : init) { get<Object>()[el.at(0).template get<String>()] = el.at(1); }
            } else { m_data = Array(init.begin(), init.end()); }
        }

        // --- Pozostałe metody inspekcji i dostępu ---
        JsonType type() const noexcept {
            switch(m_data.index()) {
                case 0: return JsonType::Null; case 1: return JsonType::Boolean; case 2: return JsonType::Integer;
                case 3: return JsonType::Float; case 4: return JsonType::String; case 5: return JsonType::Array;
                case 6: return JsonType::Object; default: return JsonType::Null;
            }
        }
        bool is_boolean() const noexcept { return std::holds_alternative<Boolean>(m_data); }
        bool is_integer() const noexcept { return std::holds_alternative<Integer>(m_data); }
        bool is_float() const noexcept { return std::holds_alternative<Float>(m_data); }
        bool is_number() const noexcept { return is_integer() || is_float(); }

        // --- Operatory ---
        BasicJson& operator[](size_t index) {
            if (is_null()) m_data = Array();
            if (!is_array()) throw JsonException("operator[size_t] not applicable to non-array type");
            auto& arr = get<Array>();
            if (index >= arr.size()) arr.resize(index + 1);
            return arr[index];
        }
        BasicJson& operator[](const String& key) {
            if (is_null()) m_data = Object();
            if (!is_object()) throw JsonException("operator[key] not applicable to non-object type");
            return get<Object>()[key];
        }
        BasicJson& operator[](const char* key) { return this->operator[](String(key)); }
        
        // --- Zaawansowany dostęp ---
        const BasicJson& at(const String& key) const {
            if (!is_object()) throw JsonException("at(key) not applicable to non-object type");
            return get<Object>().at(key);
        }
        const BasicJson& at_pointer(const std::string& json_pointer) const;

        // --- Metody pomocnicze ---
        bool empty() const { return size() == 0; }
        void clear() { if (is_object()) get<Object>().clear(); else if (is_array()) get<Array>().clear(); else if (is_string()) get<String>().clear(); }
        void push_back(const BasicJson& val) { if (is_null()) m_data = Array(); if (!is_array()) throw JsonException("push_back not applicable to non-array type"); get<Array>().push_back(val); }

        // --- Parsowanie i serializacja ---
        static BasicJson parse(std::string_view json_string, const ParseOptions& options = {});
        std::string dump(const DumpOptions& options) const;
        std::string dump(int indent = -1) const;

    private:
        // Prywatne klasy Parser i Serializer
        class Parser {
        public:
            Parser(std::string_view input, const ParseOptions& options) : m_input(input), m_opts(options) {}
            BasicJson parse_json() { skip_whitespace_and_comments(); return parse_value(); }
        private:
            std::string_view m_input; const ParseOptions& m_opts;
            size_t m_pos = 0, m_line = 1, m_col = 1; unsigned int m_depth = 0;
            [[noreturn]] void throw_parse_error(const std::string& msg) { throw JsonParseException(msg, m_line, m_col); }
            char peek() const { return m_pos < m_input.length() ? m_input[m_pos] : '\0'; }
            char advance() { char c = peek(); if (c != '\0') { if (c == '\n') { m_line++; m_col = 1; } else { m_col++; } m_pos++; } return c; }
            void expect(char c) { skip_whitespace_and_comments(); if (peek() != c) throw_parse_error(std::string("Expected '") + c + "'"); advance(); }
            void skip_whitespace_and_comments() {
                while(true) {
                    while (m_pos < m_input.length() && std::isspace(m_input[m_pos])) advance();
                    if (m_opts.allow_comments && peek() == '/') {
                        advance();
                        if (peek() == '/') { while (peek() != '\n' && peek() != '\0') advance(); continue; }
                        else if (peek() == '*') { advance(); while(true) { if (peek() == '\0') throw_parse_error("Unterminated comment"); if (advance() == '*' && peek() == '/') { advance(); break; } } continue; }
                        else { m_pos--; m_col--; break; }
                    } break;
                }
            }
            BasicJson parse_value() {
                if (++m_depth > m_opts.max_depth) throw_parse_error("Max parse depth exceeded");
                skip_whitespace_and_comments(); BasicJson result;
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
            template<typename T> BasicJson parse_literal(const char* literal, T val) {
                size_t len = std::strlen(literal); if (m_input.substr(m_pos, len) == literal) { for(size_t i=0; i<len; ++i) advance(); return BasicJson(val); }
                throw_parse_error(std::string("Expected '") + literal + "'");
            }
            BasicJson parse_number() {
                size_t start_pos = m_pos; bool is_float = false; if (peek() == '-') advance();
                while (isdigit(peek())) advance();
                if (peek() == '.') { is_float = true; advance(); while (isdigit(peek())) advance(); }
                if (peek() == 'e' || peek() == 'E') { is_float = true; advance(); if (peek() == '+' || peek() == '-') advance(); if (!isdigit(peek())) throw_parse_error("Invalid exponent"); while (isdigit(peek())) advance(); }
                std::string num_str(m_input.substr(start_pos, m_pos - start_pos));
                try {
                    if (is_float) return BasicJson(static_cast<Float>(std::stod(num_str)));
                    else { size_t end; Integer val = std::stoll(num_str, &end); if (end != num_str.length()) return BasicJson(static_cast<Float>(std::stod(num_str))); return BasicJson(val); }
                } catch(...) { throw_parse_error("Invalid number format: " + num_str); }
            }
            void append_utf8(String& s, uint32_t cp) {
                if (cp <= 0x7F) { s += static_cast<char>(cp); }
                else if (cp <= 0x7FF) { s += static_cast<char>(0xC0 | (cp >> 6)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
                else if (cp <= 0xFFFF) { s += static_cast<char>(0xE0 | (cp >> 12)); s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
                else { s += static_cast<char>(0xF0 | (cp >> 18)); s += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)); s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
            }
            BasicJson parse_string() {
                expect('"'); String str; str.reserve(32);
                while (peek() != '"') {
                    char c = advance(); if (c == '\0') throw_parse_error("Unterminated string");
                    if (c == '\\') {
                        switch (advance()) {
                            case '"': str += '"'; break; case '\\': str += '\\'; break; case '/': str += '/'; break;
                            case 'b': str += '\b'; break; case 'f': str += '\f'; break; case 'n': str += '\n'; break;
                            case 'r': str += '\r'; break; case 't': str += '\t'; break;
                            case 'u': {
                                uint32_t cp = 0;
                                for (int i=0; i<4; ++i) { char h = advance(); if (!isxdigit(h)) throw_parse_error("Invalid hex in unicode"); cp = (cp << 4) + (isdigit(h) ? tolower(h)-'0' : tolower(h)-'a'+10); }
                                if (cp >= 0xD800 && cp <= 0xDBFF) {
                                    if (m_input.substr(m_pos, 2) != "\\u") throw_parse_error("Unpaired high surrogate");
                                    advance(); advance(); uint32_t low = 0;
                                    for (int i=0; i<4; ++i) { char h = advance(); if (!isxdigit(h)) throw_parse_error("Invalid hex for low surrogate"); low = (low << 4) + (isdigit(h) ? tolower(h)-'0' : tolower(h)-'a'+10); }
                                    if (low < 0xDC00 || low > 0xDFFF) throw_parse_error("Invalid low surrogate value");
                                    cp = 0x10000 + (((cp - 0xD800) << 10) | (low - 0xDC00));
                                } append_utf8(str, cp); break;
                            }
                            default: throw_parse_error("Invalid escape sequence");
                        }
                    } else { str += c; }
                } expect('"'); return BasicJson(str);
            }
            BasicJson parse_array() {
                expect('['); Array arr; skip_whitespace_and_comments(); if (peek() == ']') { advance(); return BasicJson(std::move(arr)); }
                while (true) { arr.push_back(parse_value()); skip_whitespace_and_comments(); if (peek() == ']') { advance(); break; } expect(','); skip_whitespace_and_comments(); if (m_opts.allow_trailing_commas && peek() == ']') { advance(); break; } }
                return BasicJson(std::move(arr));
            }
            BasicJson parse_object() {
                expect('{'); Object obj; skip_whitespace_and_comments(); if (peek() == '}') { advance(); return BasicJson(std::move(obj)); }
                while (true) {
                    if (peek() != '"') throw_parse_error("Expected string key"); String key = std::get<String>(parse_string().m_data);
                    expect(':'); obj.emplace(std::move(key), parse_value()); skip_whitespace_and_comments();
                    if (peek() == '}') { advance(); break; } expect(','); skip_whitespace_and_comments();
                    if (m_opts.allow_trailing_commas && peek() == '}') { advance(); break; }
                } return BasicJson(std::move(obj));
            }
        };

        class Serializer {
        public:
            Serializer(std::ostream& os, const DumpOptions& options) : m_os(os), m_opts(options) { m_os.precision(m_opts.float_precision); }
            void serialize(const BasicJson& json) { std::visit([this](auto&& arg) { this->visit(arg); }, json.m_data); }
        private:
            std::ostream& m_os; const DumpOptions& m_opts; int m_level = 0;
            void indent() { if (m_opts.indent_width < 0) return; m_os << '\n'; for (int i = 0; i < m_level * m_opts.indent_width; ++i) m_os << m_opts.indent_char; }
            void visit(const Null&) { m_os << "null"; }
            void visit(const Boolean& val) { m_os << (val ? "true" : "false"); }
            void visit(const Integer& val) { m_os << val; }
            void visit(const Float& val) { m_os << val; }
            void visit(const String& val) {
                m_os << '"';
                for (unsigned char c : val) {
                    switch (c) {
                        case '"': m_os << "\\\""; break; case '\\': m_os << "\\\\"; break; case '\b': m_os << "\\b"; break;
                        case '\f': m_os << "\\f"; break; case '\n': m_os << "\\n"; break; case '\r': m_os << "\\r"; break;
                        case '\t': m_os << "\\t"; break;
                        default:
                            if (c < 0x20 || (m_opts.escape_unicode && c > 0x7E) ) {
                                m_os << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                            } else { m_os << c; } break;
                    }
                } m_os << '"';
            }
            void visit(const Array& val) { m_os << '['; if (val.empty()) { m_os << ']'; return; } m_level++; for (size_t i = 0; i < val.size(); ++i) { indent(); serialize(val[i]); if (i < val.size() - 1) m_os << ','; } m_level--; indent(); m_os << ']'; }
            template<typename MapType> void serialize_map(const MapType& val) {
                m_os << '{'; if (val.empty()) { m_os << '}'; return; } m_level++;
                // KLUCZOWA POPRAWKA #2: Usunięto nieużywaną zmienną 'it', aby zlikwidować ostrzeżenie.
                std::vector<typename MapType::const_iterator> iterators;
                for(auto i = val.cbegin(); i != val.cend(); ++i) { iterators.push_back(i); }
                if (m_opts.sort_keys && is_unordered_map<MapType>::value) {
                    std::sort(iterators.begin(), iterators.end(), [](auto a, auto b){ return a->first < b->first; });
                }
                for(size_t i = 0; i < iterators.size(); ++i) {
                    indent();
                    visit(iterators[i]->first);
                    m_os << (m_opts.indent_width > 0 ? ": " : ":");
                    serialize(iterators[i]->second);
                    if (i < iterators.size() - 1) m_os << ',';
                }
                m_level--; indent(); m_os << '}';
            }
            void visit(const Object& val) { serialize_map(val); }
        };
    };

    // --- Implementacje metod statycznych i pomocniczych ---
    template<class T> inline BasicJson<T> BasicJson<T>::parse(std::string_view s, const ParseOptions& o) { return Parser(s, o).parse_json(); }
    template<class T> inline std::string BasicJson<T>::dump(const DumpOptions& o) const { std::ostringstream ss; Serializer(ss, o).serialize(*this); return ss.str(); }
    template<class T> inline std::string BasicJson<T>::dump(int indent) const { DumpOptions opts; opts.indent_width = indent; return dump(opts); }

    template<class Traits>
    const BasicJson<Traits>& BasicJson<Traits>::at_pointer(const std::string& json_pointer) const {
        if (json_pointer.empty()) return *this; if (json_pointer[0] != '/') throw JsonPointerException("Pointer must start with '/'");
        const BasicJson* current = this; size_t start = 1;
        while (start < json_pointer.length()) {
            size_t end = json_pointer.find('/', start); if (end == std::string::npos) end = json_pointer.length();
            std::string token = json_pointer.substr(start, end - start);
            size_t pos; while ((pos = token.find("~1")) != std::string::npos) token.replace(pos, 2, "/"); while ((pos = token.find("~0")) != std::string::npos) token.replace(pos, 2, "~");
            if (current->is_object()) {
                auto it = current->template get<Object>().find(token); if (it == current->template get<Object>().end()) throw JsonPointerException("Pointer key not found: " + token); current = &it->second;
            } else if (current->is_array()) {
                size_t index = 0; try { index = std::stoul(token); } catch(...) { throw JsonPointerException("Invalid array index in pointer: " + token); }
                if (index >= current->size()) throw JsonPointerException("Pointer index out of bounds: " + token);
                // KLUCZOWA POPRAWKA #3: Naprawiono literówkę "¤t" na "current" i poprawiono logikę.
                // Metoda at() zwraca referencję, więc pobieramy jej adres, aby zaktualizować wskaźnik.
                current = &current->at(index);
            } else throw JsonPointerException("Pointer cannot traverse non-container type");
            start = end + 1;
        } return *current;
    }

    // --- Aliasy ---
    using Json = BasicJson<DefaultTraits<std::allocator<char>>>;

    template<class Allocator>
    struct UnorderedTraits {
        template<class T> using Alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<T>;
        using StringType = std::basic_string<char, std::char_traits<char>, Alloc<char>>;
        using BooleanType = bool; using NumberIntegerType = int64_t; using NumberFloatType = double; using NullType = std::nullptr_t;
        using ArrayType = std::vector<BasicJson<UnorderedTraits<Allocator>>, Alloc<BasicJson<UnorderedTraits<Allocator>>>>;
        using ObjectType = std::unordered_map< StringType, BasicJson<UnorderedTraits<Allocator>>, std::hash<StringType>, std::equal_to<>, Alloc<std::pair<const StringType, BasicJson<UnorderedTraits<Allocator>>>>>;
    };
    using UnorderedJson = BasicJson<UnorderedTraits<std::allocator<char>>>;

} // namespace Tachyon
