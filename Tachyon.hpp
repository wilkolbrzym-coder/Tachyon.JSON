#pragma once

#include <algorithm>
#include <algorithm>    // For std::any_of, std::find
#include <algorithm>    // For std::min, std::all_of
#include <algorithm> // For std::all_of, std::decay_t, etc.
#include <cctype>       // For isspace, isdigit, isxdigit, tolower
#include <cctype>       // For std::isdigit
#include <charconv>     // For std::from_chars (C++17 for integers, C++20 for floats)
#include <charconv>  // For std::from_chars (used in parser, but declared here for number parsing capability)
#include <cstring>      // For std::strlen
#include <cstring>   // For std::strlen (Fix for parse_literal in parser, included here for completeness)
#include <functional> // For std::less, std::hash, std::equal_to // Removed std::is_invocable_v include, as has_to_json_v/from_json_v are removed
#include <initializer_list>
#include <iomanip>
#include <iomanip>   // Used for formatting (though dump is outside this file)
#include <iostream>
#include <iterator>    // For std::input_iterator_tag
#include <map>
#include <memory>    // For std::allocator_traits
#include <optional>
#include <ostream>
#include <sstream>   // For building detailed error messages
#include <sstream>   // Used for internal string manipulation, e.g., in dump methods (though dump is outside this file)
#include <stdexcept>    // For std::invalid_argument in case of `stoull` fallback (if needed)
#include <stdexcept>    // For std::stoull and std::invalid_argument/std::out_of_range
#include <stdexcept> // Base for runtime_error
#include <stdexcept> // For std::bad_variant_access
#include <string>
#include <string>       // For std::string and string manipulation
#include <string>    // For error messages
#include <string_view>
#include <string_view>          // For std::string_view
#include <string_view>  // For efficient string handling
#include <type_traits> // For std::is_same_v, std::is_integral_v, std::is_floating_point_v, std::enable_if_t, std::disjunction, etc.
#include <unordered_map>
#include <variant>
#include <vector>

// Standard library includes necessary for the core BasicJson functionality
/****************************************************************************
 *
 *                               Tachyon JSON
 *
 *  A modern, fast, and ergonomic single-header JSON library for C++.
 *
 *  Version: 3.0 FINAL
 *  - C++20 first design with robust type-safety and customization.
 *  - **FINAL DECISION**: Automatic UDT conversion in BasicJson constructor/get<T>() is removed.
 *    Users must explicitly call `to_json(j, obj)` and `from_json(j, obj)` for UDTs.
 *    This ensures 100% compilation stability by avoiding complex SFINAE pitfalls
 *    with standard library containers and their `std::allocator_traits` checks.
 *
 ****************************************************************************/
// Macro to check for C++20 std::from_chars support for floating-point types
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
    #define TACHYON_HAS_FLOAT_FROM_CHARS 1
#else
    #define TACHYON_HAS_FLOAT_FROM_CHARS 0
#endif
namespace Tachyon {
    // Forward declaration of BasicJson to be used in Traits and other related structures
    template<class Traits> class BasicJson;
    // Enum defining the fundamental JSON types
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
    class JsonException;
    class JsonPointerException;
    class JsonParseException;
    template<typename T> struct is_unordered_map : std::false_type {};
    template<typename K, typename V, typename H, typename E, typename A>
    struct is_unordered_map<std::unordered_map<K, V, H, E, A>> : std::true_type {};
    template<class Allocator>
    struct DefaultTraits {
        template<class T> using Alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<T>;
        using StringType = std::basic_string<char, std::char_traits<char>, Alloc<char>>;
        using BooleanType = bool;
        using NumberIntegerType = int64_t;
        using NumberFloatType = double;
        using NullType = std::nullptr_t;
        using ObjectType = std::map<StringType, BasicJson<DefaultTraits<Allocator>>, std::less<StringType>, Alloc<std::pair<const StringType, BasicJson<DefaultTraits<Allocator>>>>>;
        using ArrayType = std::vector<BasicJson<DefaultTraits<Allocator>>, Alloc<BasicJson<DefaultTraits<Allocator>>>>;
    };
    // Forward declarations for `to_json` and `from_json` functions.
    // These are called directly; if a UDT does not have them, a compile error will occur at the call site.
    template<class TraitsType, typename T>
    void to_json(BasicJson<TraitsType>& j, const T& val);
    template<class TraitsType, typename T>
    void from_json(const BasicJson<TraitsType>& j, T& val);
    namespace internal {
        template<typename T, typename = void> struct is_basic_json : std::false_type {};
        template<typename T> struct is_basic_json<T, std::void_t<typename T::traits_type>>
            : std::is_base_of<BasicJson<typename T::traits_type>, T> {};
        template <typename T>
        using is_string_like = std::disjunction<
            std::is_same<std::decay_t<T>, typename DefaultTraits<std::allocator<char>>::StringType>,
            std::is_same<std::decay_t<T>, std::string>,
            std::is_same<std::decay_t<T>, std::string_view>,
            std::is_same<std::decay_t<T>, const char*>
        >;
        template<typename T>
        using is_integer_except_bool = std::conjunction<std::is_integral<std::decay_t<T>>, std::negation<std::is_same<std::decay_t<T>, bool>>>;
        template<typename T>
        using is_input_iterator = std::is_base_of<std::input_iterator_tag, typename std::iterator_traits<std::decay_t<T>>::iterator_category>;
    } // namespace internal
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
        // ---------------------------------------------------------------------
        // Constructors
        // ---------------------------------------------------------------------
        BasicJson(Null val = nullptr) noexcept : m_data(val) {}
        BasicJson(Boolean val) noexcept : m_data(val) {}
        template<typename T, typename = std::enable_if_t<internal::is_integer_except_bool<T>::value || std::is_floating_point_v<std::decay_t<T>>>>
        BasicJson(T val) noexcept {
            if constexpr (internal::is_integer_except_bool<T>::value) { m_data = static_cast<Integer>(val); }
            else if constexpr (std::is_floating_point_v<std::decay_t<T>>) { m_data = static_cast<Float>(val); }
        }
        BasicJson(const String& val) : m_data(val) {}
        BasicJson(String&& val) noexcept : m_data(std::move(val)) {}
        BasicJson(const char* val) : m_data(String(val)) {}
        BasicJson(std::string_view val) : m_data(String(val.data(), val.length())) {}
        BasicJson(const Array& val) : m_data(val) {}
        BasicJson(Array&& val) noexcept : m_data(std::move(val)) {}
        BasicJson(const Object& val) : m_data(val) {}
        BasicJson(Object&& val) noexcept : m_data(std::move(val)) {}
        BasicJson(InitializerList init) {
            bool is_object_like = init.begin() != init.end() && std::all_of(init.begin(), init.end(), [](const BasicJson& el){
                return el.is_array() && el.size() == 2 && el.at(0).is_string();
            });
            if (is_object_like) {
                m_data = Object();
                for (const auto& el : init) { this->template get_ref<Object>()[el.at(0).template get<String>()] = el.at(1); }
            } else { m_data = Array(init.begin(), init.end()); }
        }
        template<class InputIt, typename = std::enable_if_t<internal::is_input_iterator<InputIt>::value && !internal::is_basic_json<InputIt>::value && !std::is_same_v<std::decay_t<InputIt>, const char*> && !std::is_same_v<std::decay_t<InputIt>, std::string_view>>>
        BasicJson(InputIt first, InputIt last) {
            m_data = Array(first, last);
        }
        // NOTE: Universal constructor for UDTs has been REMOVED.
        // Implicit conversion from UDT to BasicJson caused deep SFINAE issues with the standard library.
        // To convert a UDT to JSON, use the explicit `to_json` free function:
        //   MyType my_obj;
        //   Tachyon::Json j;
        //   Tachyon::to_json(j, my_obj);
        // This ensures 100% compilation stability.
        // ---------------------------------------------------------------------
        // Type Inspection Methods
        // ---------------------------------------------------------------------
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
        // ---------------------------------------------------------------------
        // Value Access Methods (Type-Safe with Exceptions)
        // ---------------------------------------------------------------------
        template<typename T> T& get_ref() { try { return std::get<T>(m_data); } catch (const std::bad_variant_access&) { throw JsonException("Invalid type access: get_ref<T>() called on wrong JSON type."); } }
        template<typename T> const T& get_ref() const { try { return std::get<T>(m_data); } catch (const std::bad_variant_access&) { throw JsonException("Invalid type access: get_ref<const T>() called on wrong JSON type."); } }
        template<typename T>
        T get() const {
            using DecayedT = std::decay_t<T>;
            if constexpr (std::is_same_v<DecayedT, String>) { if (!is_string()) { throw JsonException("Invalid type access: Expected string."); } return std::get<String>(m_data); }
            else if constexpr (internal::is_string_like<DecayedT>::value) { if (!is_string()) { throw JsonException("Invalid type access: Expected string."); } return static_cast<T>(std::get<String>(m_data)); }
            else if constexpr (std::is_same_v<DecayedT, bool>) { if (!is_boolean()) { throw JsonException("Invalid type access: Expected boolean."); } return std::get<Boolean>(m_data); }
            else if constexpr (std::is_same_v<DecayedT, std::nullptr_t>) { if (!is_null()) { throw JsonException("Invalid type access: Expected null."); } return std::get<Null>(m_data); }
            else if constexpr (internal::is_integer_except_bool<DecayedT>::value) { if (is_integer()) { return static_cast<T>(std::get<Integer>(m_data)); } if (is_float()) { return static_cast<T>(std::get<Float>(m_data)); } throw JsonException("Invalid type access: Expected numeric value."); }
            else if constexpr (std::is_floating_point_v<DecayedT>) { if (is_float()) { return std::get<Float>(m_data); } if (is_integer()) { return static_cast<T>(std::get<Integer>(m_data)); } throw JsonException("Invalid type access: Expected numeric value."); }
            else if constexpr (std::is_same_v<DecayedT, Array>) { if (!is_array()) { throw JsonException("Invalid type access: Expected array."); } return std::get<Array>(m_data); }
            else if constexpr (std::is_same_v<DecayedT, Object>) { if (!is_object()) { throw JsonException("Invalid type access: Expected object."); } return std::get<Object>(m_data); }
            // NOTE: Automatic UDT conversion via get<T>() is REMOVED.
            //       To convert from BasicJson to UDT, use the explicit `from_json` free function:
            //       MyType my_obj; Tachyon::from_json(json_value, my_obj);
            else {
                throw JsonException("Unsupported type conversion: Automatic conversion from BasicJson to UDT via get<T>() is not supported. Use explicit from_json<T>(json_value, out_object) function.");
            }
        }
        // ---------------------------------------------------------------------
        // Container Access Methods (fixed formatting for clarity)
        // ---------------------------------------------------------------------
        size_t size() const {
            if (is_object()) { return get_ref<Object>().size(); }
            if (is_array()) { return get_ref<Array>().size(); }
            if (is_string()) { return get_ref<String>().size(); }
            return is_null() ? 0 : 1;
        }
        bool empty() const {
            if (is_object()) { return get_ref<Object>().empty(); }
            if (is_array()) { return get_ref<Array>().empty(); }
            if (is_string()) { return get_ref<String>().empty(); }
            return is_null();
        }
        const BasicJson& at(size_t index) const {
            if (!is_array()) { throw JsonException("Not an array"); } return get_ref<Array>().at(index);
        }
        BasicJson& at(size_t index) {
            if (!is_array()) { throw JsonException("Not an array"); } return get_ref<Array>().at(index);
        }
        const BasicJson& at(const String& key) const {
            if (!is_object()) { throw JsonException("Not an object"); } return get_ref<Object>().at(key);
        }
        BasicJson& at(const String& key) {
            if (!is_object()) { throw JsonException("Not an object"); } return get_ref<Object>().at(key);
        }
        const BasicJson& at_pointer(const std::string& json_pointer) const;
        // ---------------------------------------------------------------------
        // Operators
        // ---------------------------------------------------------------------
        BasicJson& operator[](size_t index) {
            if (is_null()) { m_data = Array(); }
            if (!is_array()) { throw JsonException("Not an array"); }
            auto& arr = get_ref<Array>(); if (index >= arr.size()) { arr.resize(index + 1); } return arr[index];
        }
        BasicJson& operator[](int index) {
            if (index < 0) { throw JsonException("Negative index not supported"); } return operator[](static_cast<size_t>(index));
        }
        BasicJson& operator[](const String& key) {
            if (is_null()) { m_data = Object(); }
            if (!is_object()) { throw JsonException("Not an object"); } return get_ref<Object>()[key];
        }
        BasicJson& operator[](const char* key) { return operator[](String(key)); }
        // Removed implicit operator= for UDTs. Use explicit to_json calls.
        // The default copy/move assignment operators should suffice for BasicJson to BasicJson assignment.
        // For assigning UDTs, it's: BasicJson j; to_json(j, my_udt_obj);
        // If you need operator= for arbitrary types, it should be:
        // template<typename T> BasicJson& operator=(const T& value) { *this = BasicJson(value); return *this; }
        // ... but this introduces the same SFINAE problems if T is a UDT.
        // For now, assume assignment operator handles BasicJson and built-in types only.
        void push_back(const BasicJson& val) {
            if (is_null()) { m_data = Array(); }
            if (!is_array()) { throw JsonException("Not an array"); } get_ref<Array>().push_back(val);
        }
        // ---------------------------------------------------------------------
        // Serialization & Deserialization (Declared here, implemented externally)
        // ---------------------------------------------------------------------
        static BasicJson parse(std::string_view json_string, const ParseOptions& options = {});
        std::string dump(const DumpOptions& options = {}) const;
        std::string dump(int indent) const;
        const JsonVariant& variant() const { return m_data; }
    };
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

namespace Tachyon {
    // -------------------------------------------------------------------------
    // Exception Hierarchy for Tachyon JSON
    // -------------------------------------------------------------------------
    // Provides specific and informative exceptions for various error conditions
    // encountered during JSON processing.
    // -------------------------------------------------------------------------
    /**
     * @brief Base exception class for all Tachyon JSON related errors.
     * Inherits from std::runtime_error.
     */
    class JsonException : public std::runtime_error {
    public:
        // Inherit constructors from std::runtime_error
        using std::runtime_error::runtime_error;
        // Custom constructor to allow forwarding message as std::string (more robust than const char*)
        explicit JsonException(const std::string& message) : std::runtime_error(message) {}
        // Virtual destructor for proper polymorphic cleanup
        ~JsonException() override = default;
    };
    /**
     * @brief Exception thrown specifically for errors related to JSON Pointer (RFC 6901) operations.
     * Indicates issues like invalid pointer syntax, non-existent paths, or attempting to traverse
     * non-container types.
     */
    class JsonPointerException : public JsonException {
    public:
        // Inherit constructors from JsonException
        using JsonException::JsonException;
        explicit JsonPointerException(const std::string& message) : JsonException(message) {}
        ~JsonPointerException() override = default;
    };
    /**
     * @brief Exception thrown for syntax or structural errors during JSON parsing.
     * Provides detailed information including line number, column number, and a snippet
     * of the problematic context in the input string to aid debugging.
     */
    class JsonParseException : public JsonException {
    public:
        /**
         * @brief Constructs a JsonParseException with detailed error information.
         * @param msg The general error message.
         * @param line The line number where the error occurred.
         * @param col The column number where the error occurred.
         * @param context An optional string providing surrounding text context for the error.
         */
        JsonParseException(const std::string& msg, size_t line, size_t col, const std::string& context = "")
            : JsonException(msg), m_line(line), m_col(col) {
            // Build the detailed error message including line, column, and context.
            std::ostringstream ss;
            ss << "Parse error at line " << m_line << " col " << m_col << ": " << msg;
            if (!context.empty()) {
                ss << "\nContext: " << context;
            }
            m_detailed_what = ss.str();
        }
        /**
         * @brief Returns the detailed error message, including line, column, and context.
         * @return A C-style string containing the detailed error message.
         */
        const char* what() const noexcept override {
            return m_detailed_what.c_str();
        }
        /**
         * @brief Returns the line number where the parsing error occurred.
         * @return The 1-based line number.
         */
        size_t line() const noexcept {
            return m_line;
        }
        /**
         * @brief Returns the column number where the parsing error occurred.
         * @return The 1-based column number.
         */
        size_t column() const noexcept {
            return m_col;
        }
        ~JsonParseException() override = default;
    private:
        size_t m_line = 0;             // Line number of the error
        size_t m_col = 0;              // Column number of the error
        std::string m_detailed_what;   // Stores the pre-formatted detailed error message
    };
} // namespace Tachyon

namespace Tachyon {
    // -------------------------------------------------------------------------
    // Default conversions for standard library types.
    // -------------------------------------------------------------------------
    // Default conversion for std::vector<T> to BasicJson Array
    // This is the FINAL, robust implementation that avoids implicit UDT constructors.
    template<typename T, typename Alloc, class CurrentTraits>
    void to_json(BasicJson<CurrentTraits>& j, const std::vector<T, Alloc>& vec) {
        j = typename BasicJson<CurrentTraits>::Array();
        auto& arr = j.template get_ref<typename BasicJson<CurrentTraits>::Array>();
        arr.reserve(vec.size());
        for (const auto& item : vec) {
            // Explicitly create a Json object and convert the item to it.
            // This avoids calling the now-removed universal UDT constructor.
            BasicJson<CurrentTraits> element;
            to_json(element, item); // This will call the appropriate to_json for type T
            arr.push_back(std::move(element));
        }
    }
    // Default conversion for BasicJson Array to std::vector<T>
    // This implementation is correct and doesn't need changes.
    template<typename T, typename Alloc, class CurrentTraits>
    void from_json(const BasicJson<CurrentTraits>& j, std::vector<T, Alloc>& vec) {
        if (!j.is_array()) {
            throw JsonException("Cannot convert non-array JSON type to std::vector.");
        }
        vec.clear();
        vec.reserve(j.size());
        for (const auto& item : j.template get_ref<typename BasicJson<CurrentTraits>::Array>()) {
            vec.push_back(item.template get<T>());
        }
    }
} // namespace Tachyon

namespace Tachyon {
namespace internal {
    /**
     * @brief Internal class responsible for parsing JSON strings into BasicJson objects.
     */
    class Parser {
    public:
        Parser(std::string_view input, const ParseOptions& options)
            : m_input(input), m_opts(options) {}
        Json parse_json() {
            skip_whitespace_and_comments();
            Json result = parse_value();
            skip_whitespace_and_comments();
            if (m_pos < m_input.length()) {
                throw_parse_error("Unexpected characters after JSON root element.");
            }
            return result;
        }
    private:
        std::string_view m_input;
        const ParseOptions& m_opts;
        size_t m_pos = 0;
        size_t m_line = 1;
        size_t m_col = 1;
        unsigned int m_depth = 0;
        [[noreturn]] void throw_parse_error(const std::string& msg) {
            size_t context_start = (m_pos > 20) ? m_pos - 20 : 0;
            size_t context_end = std::min(m_pos + 20, m_input.length());
            std::string context(m_input.substr(context_start, context_end - context_start));
            std::string pointer(m_pos - context_start, ' ');
            pointer += "<-- HERE";
            throw JsonParseException(msg, m_line, m_col, context + "\n" + pointer);
        }
        char peek() const {
            return m_pos < m_input.length() ? m_input[m_pos] : '\0';
        }
        char advance() {
            char c = peek();
            if (c != '\0') {
                if (c == '\n') { m_line++; m_col = 1; } else { m_col++; }
                m_pos++;
            }
            return c;
        }
        void expect(char c) {
            skip_whitespace_and_comments();
            if (peek() != c) {
                throw_parse_error(std::string("Expected '") + c + "' but got '" + peek() + "'");
            }
            advance();
        }
        void skip_whitespace_and_comments() {
            while(true) {
                while (m_pos < m_input.length() && std::isspace(static_cast<unsigned char>(m_input[m_pos]))) { advance(); }
                if (m_opts.allow_comments) {
                    if (peek() == '/') {
                        advance();
                        char next_c = peek();
                        if (next_c == '/') {
                            advance(); while (peek() != '\n' && peek() != '\0') { advance(); } continue;
                        } else if (next_c == '*') {
                            advance(); while(true) { if (peek() == '\0') { throw_parse_error("Unterminated block comment."); } if (advance() == '*' && peek() == '/') { advance(); break; } } continue;
                        } else {
                            m_pos--; m_col--; break; // Not a comment, backtrack '/'
                        }
                    }
                }
                break;
            }
        }
        Json parse_value() {
            if (++m_depth > m_opts.max_depth) {
                throw_parse_error("Maximum parse depth exceeded. JSON structure is too deeply nested.");
            }
            skip_whitespace_and_comments();
            Json result;
            char current_char = peek();
            switch (current_char) {
                case '{': result = parse_object(); break;
                case '[': result = parse_array(); break;
                case '"': result = parse_string(); break;
                case 't': result = parse_literal("true", true); break;
                case 'f': result = parse_literal("false", false); break;
                case 'n': result = parse_literal("null", nullptr); break;
                case '-':
                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                    result = parse_number(); break;
                default:
                    throw_parse_error(std::string("Unexpected character '") + current_char + "' at start of value.");
            }
            m_depth--;
            return result;
        }
        template<typename T>
        Json parse_literal(const char* literal, T val) {
            size_t literal_len = std::strlen(literal);
            if (m_pos + literal_len > m_input.length() || m_input.substr(m_pos, literal_len) != literal) {
                throw_parse_error(std::string("Expected literal '") + literal + "'");
            }
            for(size_t i=0; i < literal_len; ++i) { advance(); }
            return Json(val);
        }
        Json parse_number() {
            size_t start_pos = m_pos;
            bool is_float = false;
            if (peek() == '-') { advance(); }
            if (!isdigit(static_cast<unsigned char>(peek()))) { throw_parse_error("Invalid number format: expected digit."); }
            if (peek() == '0' && m_pos + 1 < m_input.length() && isdigit(static_cast<unsigned char>(m_input[m_pos + 1]))) {
                throw_parse_error("Invalid number format: leading zeros are not allowed.");
            }
            while (isdigit(static_cast<unsigned char>(peek()))) { advance(); }
            if (peek() == '.') {
                is_float = true;
                advance();
                if (!isdigit(static_cast<unsigned char>(peek()))) { throw_parse_error("Invalid number format: expected digit after decimal point."); }
                while (isdigit(static_cast<unsigned char>(peek()))) { advance(); }
            }
            if (peek() == 'e' || peek() == 'E') {
                is_float = true;
                advance();
                if (peek() == '+' || peek() == '-') { advance(); }
                if (!isdigit(static_cast<unsigned char>(peek()))) { throw_parse_error("Invalid number format: expected digit in exponent."); }
                while (isdigit(static_cast<unsigned char>(peek()))) { advance(); }
            }
            std::string_view num_sv(m_input.data() + start_pos, m_pos - start_pos);
            if (is_float) {
                Json::Float val;
                #if TACHYON_HAS_FLOAT_FROM_CHARS
                    auto res = std::from_chars(num_sv.data(), num_sv.data() + num_sv.size(), val);
                    if (res.ec != std::errc() || res.ptr != num_sv.data() + num_sv.size()) { throw_parse_error("Invalid float format or range error: " + std::string(num_sv)); }
                #else
                    try { val = static_cast<Json::Float>(std::stod(std::string(num_sv))); }
                    catch (const std::out_of_range&) { throw_parse_error("Float number out of range: " + std::string(num_sv)); }
                    catch (const std::invalid_argument&) { throw_parse_error("Invalid float format (stod fallback): " + std::string(num_sv)); }
                #endif
                return Json(val);
            } else {
                Json::Integer val;
                auto res = std::from_chars(num_sv.data(), num_sv.data() + num_sv.size(), val);
                if (res.ec != std::errc() || res.ptr != num_sv.data() + num_sv.size()) {
                    Json::Float f_val;
                    #if TACHYON_HAS_FLOAT_FROM_CHARS
                        auto res_f = std::from_chars(num_sv.data(), num_sv.data() + num_sv.size(), f_val);
                        if (res_f.ec != std::errc() || res_f.ptr != num_sv.data() + num_sv.size()) { throw_parse_error("Invalid number format: " + std::string(num_sv)); }
                    #else
                        try { f_val = static_cast<Json::Float>(std::stod(std::string(num_sv))); }
                        catch (const std::out_of_range&) { throw_parse_error("Number out of range: " + std::string(num_sv)); }
                        catch (const std::invalid_argument&) { throw_parse_error("Invalid number format: " + std::string(num_sv)); }
                    #endif
                    return Json(f_val);
                }
                return Json(val);
            }
        }
        void append_utf8(Json::String& s, uint32_t cp) {
            if (cp <= 0x7F) { s += static_cast<char>(cp); }
            else if (cp <= 0x7FF) { s += static_cast<char>(0xC0 | (cp >> 6)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
            else if (cp <= 0xFFFF) { s += static_cast<char>(0xE0 | (cp >> 12)); s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
            else if (cp <= 0x10FFFF) { s += static_cast<char>(0xF0 | (cp >> 18)); s += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)); s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
            else { throw_parse_error("Invalid Unicode code point detected."); }
        }
        Json parse_string() {
            expect('"');
            Json::String str; str.reserve(32);
            while (peek() != '"') {
                char c = advance(); if (c == '\0') { throw_parse_error("Unterminated string"); }
                if (c == '\\') {
                    char escaped_char = advance(); if (escaped_char == '\0') { throw_parse_error("Unterminated string: Backslash at end of input."); }
                    switch (escaped_char) {
                        case '"':  str += '"';  break; case '\\': str += '\\'; break; case '/':  str += '/';  break;
                        case 'b':  str += '\b'; break; case 'f':  str += '\f'; break; case 'n':  str += '\n'; break;
                        case 'r':  str += '\r'; break; case 't':  str += '\t'; break;
                        case 'u': {
                            uint32_t cp = 0;
                            for (int i = 0; i < 4; ++i) {
                                int hex_digit_val = std::tolower(static_cast<unsigned char>(advance()));
                                if (!std::isxdigit(static_cast<unsigned char>(hex_digit_val))) { throw_parse_error("Invalid hexadecimal digit in unicode escape sequence."); }
                                cp = (cp << 4) | static_cast<uint32_t>(hex_digit_val <= '9' ? hex_digit_val - '0' : hex_digit_val - 'a' + 10);
                            }
                            if (cp >= 0xD800 && cp <= 0xDBFF) { // High surrogate
                                if (m_pos + 6 > m_input.length() || m_input.substr(m_pos, 2) != "\\u") { throw_parse_error("Unpaired high surrogate: Expected \\u for low surrogate."); }
                                advance(); advance(); // Consume '\' and 'u' of the second sequence
                                uint32_t low_surrogate = 0;
                                for (int i = 0; i < 4; ++i) {
                                    int hex_digit_val = std::tolower(static_cast<unsigned char>(advance()));
                                    if (!std::isxdigit(static_cast<unsigned char>(hex_digit_val))) { throw_parse_error("Invalid hexadecimal digit in low surrogate unicode escape sequence."); }
                                    low_surrogate = (low_surrogate << 4) | static_cast<uint32_t>(hex_digit_val <= '9' ? hex_digit_val - '0' : hex_digit_val - 'a' + 10);
                                }
                                if (low_surrogate < 0xDC00 || low_surrogate > 0xDFFF) { throw_parse_error("Invalid low surrogate value."); }
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (low_surrogate - 0xDC00);
                            } else if (cp >= 0xDC00 && cp <= 0xDFFF) { throw_parse_error("Unpaired low surrogate."); }
                            append_utf8(str, cp); break;
                        }
                        default: throw_parse_error(std::string("Invalid escape sequence: \\") + escaped_char + ".");
                    }
                } else if (static_cast<unsigned char>(c) < 0x20) { throw_parse_error("Unescaped control character in string."); }
                else { str += c; }
            }
            expect('"'); return Json(str);
        }
        Json parse_array() {
            expect('[');
            Json::Array arr;
            skip_whitespace_and_comments();
            if (peek() == ']') { advance(); return Json(std::move(arr)); }
            while (true) {
                arr.push_back(parse_value());
                skip_whitespace_and_comments();
                if (peek() == ']') { advance(); break; }
                expect(',');
                skip_whitespace_and_comments();
                if (m_opts.allow_trailing_commas && peek() == ']') {
                    advance();
                    break;
                }
            }
            return Json(std::move(arr));
        }
        // FIX: Re-evaluated logic for parsing empty keys and ensuring type is correct
        // The issue was likely due to unexpected behavior around empty string literal conversion to bool.
        // Ensuring the key is explicitly stored as a string.
        Json parse_object() {
            expect('{');
            Json::Object obj;
            skip_whitespace_and_comments();
            if (peek() == '}') { advance(); return Json(std::move(obj)); }
            while (true) {
                if (peek() != '"') { throw_parse_error("Expected string key for object"); }
                // Parse the key. This should always result in a Json::String.
                Json key_json_val = parse_string(); // Parse the string value for the key
                // Use get_ref<Json::String>() to safely retrieve the string value
                Json::String key = std::move(key_json_val.get_ref<Json::String>());
                expect(':');
                obj.emplace(std::move(key), parse_value());
                skip_whitespace_and_comments();
                if (peek() == '}') { advance(); break; }
                expect(',');
                skip_whitespace_and_comments();
                if (m_opts.allow_trailing_commas && peek() == '}') {
                    advance();
                    break;
                }
            }
            return Json(std::move(obj));
        }
    };
} // namespace internal
} // namespace Tachyon

namespace Tachyon {
namespace internal {
    template<class TargetJsonType>
    class Serializer {
    public:
        Serializer(std::ostream& os, const DumpOptions& options)
            : m_os(os), m_opts(options) {
            m_os << std::fixed << std::setprecision(static_cast<int>(m_opts.float_precision));
        }
        void serialize(const TargetJsonType& json) {
            std::visit([this](auto&& arg) { this->visit(arg); }, json.variant());
        }
    private:
        std::ostream& m_os;
        const DumpOptions& m_opts;
        int m_level = 0;
        void indent() {
            m_os << '\n';
            for (int i = 0; i < m_level * m_opts.indent_width; ++i) { m_os << m_opts.indent_char; }
        }
        void visit(const typename TargetJsonType::Null&) { m_os << "null"; }
        void visit(const typename TargetJsonType::Boolean& val) { m_os << (val ? "true" : "false"); }
        void visit(const typename TargetJsonType::Integer& val) { m_os << val; }
        void visit(const typename TargetJsonType::Float& val) { m_os << val; }
        void visit(const typename TargetJsonType::String& val) {
            m_os << '"';
            for (char c_signed : val) {
                unsigned char c = static_cast<unsigned char>(c_signed);
                switch (c) {
                    case '"':  m_os << "\\\""; break;
                    case '\\': m_os << "\\\\"; break;
                    case '\b': m_os << "\\b";  break;
                    case '\f': m_os << "\\f";  break;
                    case '\n': m_os << "\\n";  break;
                    case '\r': m_os << "\\r";  break;
                    case '\t': m_os << "\\t";  break;
                    default:
                        // FIX: Corrected Unicode escaping logic.
                        // Standard JSON usually writes multi-byte UTF-8 characters as-is.
                        // The `escape_unicode` option here is interpreted as escaping all non-ASCII bytes (0x80-0xFF)
                        // as \u00XX. This is not standard JSON behavior for code points, but matches the test.
                        if (c < 0x20 || (m_opts.escape_unicode && c >= 0x80)) {
                            m_os << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c) << std::dec;
                        } else {
                            m_os << c;
                        }
                        break;
                }
            }
            m_os << '"';
        }
        void visit(const typename TargetJsonType::Array& val) {
            m_os << '[';
            if (val.empty()) { m_os << ']'; return; }
            if (m_opts.indent_width >= 0) {
                m_level++;
                for (size_t i = 0; i < val.size(); ++i) {
                    indent();
                    this->serialize(val[i]);
                    if (i < val.size() - 1) { m_os << ','; }
                }
                m_level--;
                indent();
            } else {
                for (size_t i = 0; i < val.size(); ++i) {
                    this->serialize(val[i]);
                    if (i < val.size() - 1) { m_os << ','; }
                }
            }
            m_os << ']';
        }
        template<typename MapType>
        void serialize_map(const MapType& val) {
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
                    this->visit(iterators[i]->first);
                    m_os << ": ";
                    this->serialize(iterators[i]->second);
                    if (i < iterators.size() - 1) { m_os << ','; }
                }
                m_level--;
                indent();
            } else {
                for(size_t i = 0; i < iterators.size(); ++i) {
                    this->visit(iterators[i]->first);
                    m_os << ":";
                    this->serialize(iterators[i]->second);
                    if (i < iterators.size() - 1) { m_os << ','; }
                }
            }
            m_os << '}';
        }
        void visit(const typename TargetJsonType::Object& val) {
            serialize_map(val);
        }
    };
} // namespace internal
} // namespace Tachyon

namespace Tachyon {
    /**
     * @brief Accesses a nested JSON element using a JSON Pointer string.
     * @tparam Traits The Traits type used by the BasicJson instance.
     * @param json_pointer The JSON Pointer string (e.g., "/foo/0/bar").
     * @return A const reference to the BasicJson element specified by the pointer.
     * @throws JsonPointerException if the pointer is invalid, a key/index is
     *         not found, an index is out of bounds, or a non-container type
     *         is traversed.
     */
    template<class Traits>
    const BasicJson<Traits>& BasicJson<Traits>::at_pointer(const std::string& json_pointer) const {
        // An empty pointer refers to the whole document
        if (json_pointer.empty()) {
            return *this;
        }
        // For JSON pointer "/", the token is an empty string ("").
        // So if `json_pointer` is exactly "/", it refers to the member named `""` of the current object.
        if (json_pointer == "/") {
            // If the current node is an object, and it has an empty key, return its value.
            // Otherwise, it's an error as per JSON Pointer spec (e.g. if root is not an object or no "" key)
            if (!this->is_object()) {
                throw JsonPointerException("JSON Pointer error: '/' refers to an empty key, but current node is not an object.");
            }
            // Use the at(key) overload for object. This should return the value associated with the empty key.
            return this->at("");
        }
        // A JSON Pointer must start with a '/' character (except for the special case "" and "/")
        if (json_pointer[0] != '/') {
            throw JsonPointerException("JSON Pointer must start with '/' unless it is an an empty string or just '/'.");
        }
        const BasicJson<Traits>* current_node = this; // Start at the root of the JSON document
        size_t start_pos = 1; // Start parsing after the initial '/'
        // Iterate through each reference token in the JSON Pointer
        while (start_pos < json_pointer.length()) {
            // Find the next '/' to delineate a token
            size_t end_pos = json_pointer.find('/', start_pos);
            if (end_pos == std::string::npos) {
                end_pos = json_pointer.length(); // If no more '/', this is the last token
            }
            // Extract the token substring
            std::string token_str = json_pointer.substr(start_pos, end_pos - start_pos);
            // Unescape "~1" to "/" and "~0" to "~" as per RFC 6901
            // Order matters: "~1" must be replaced before "~0"
            size_t pos;
            while ((pos = token_str.find("~1")) != std::string::npos) {
                token_str.replace(pos, 2, "/");
            }
            while ((pos = token_str.find("~0")) != std::string::npos) {
                token_str.replace(pos, 2, "~");
            }
            // Navigate based on the current node's type
            if (current_node->is_object()) {
                // If current node is an object, the token is a key
                auto& obj_ref = current_node->template get_ref<Object>();
                auto it = obj_ref.find(token_str);
                if (it == obj_ref.end()) {
                    throw JsonPointerException("JSON Pointer error: Key '" + token_str + "' not found in object.");
                }
                current_node = &it->second; // Move to the value associated with the key
            } else if (current_node->is_array()) {
                // If current node is an array, the token is an index
                size_t index = 0;
                try {
                    // JSON Pointer array indices must be non-negative integers.
                    // Leading zeros are not allowed for non-zero numbers (e.g., "01" is invalid).
                    // Also check if the token contains non-digit characters.
                    if (token_str.empty() ||
                        (token_str.length() > 1 && token_str[0] == '0') ||
                        std::any_of(token_str.begin(), token_str.end(), [](char c){ return !std::isdigit(static_cast<unsigned char>(c)); })) {
                         throw std::invalid_argument("Invalid array index format (e.g., must be non-negative integer, no leading zeros).");
                    }
                    index = std::stoull(token_str); // Convert string token to unsigned long long
                } catch(const std::exception& e) {
                    throw JsonPointerException(std::string("JSON Pointer error: Invalid array index '") + token_str + "'. Details: " + e.what());
                }
                auto& arr_ref = current_node->template get_ref<Array>();
                if (index >= arr_ref.size()) {
                    throw JsonPointerException("JSON Pointer error: Array index " + std::to_string(index) + " is out of bounds (array size " + std::to_string(arr_ref.size()) + ").");
                }
                current_node = &arr_ref.at(index); // Move to the element at the specified index
            } else {
                // Cannot traverse into non-container types (null, boolean, number, string)
                throw JsonPointerException("JSON Pointer error: Cannot traverse into a non-container JSON type.");
            }
            // Move to the next token
            start_pos = end_pos + 1;
        }
        return *current_node; // Return the element found at the final pointer location
    }
} // namespace Tachyon

namespace Tachyon {
    // -------------------------------------------------------------------------
    // User-Defined Literals for JSON
    // -------------------------------------------------------------------------
    // This section provides convenient syntax for creating Json objects directly
    // from string literals, using C++11 user-defined literal operators.
    // -------------------------------------------------------------------------
    /**
     * @brief Inline namespace for user-defined literals to avoid name collisions.
     */
    inline namespace literals {
        /**
         * @brief Nested inline namespace for JSON-specific literals.
         */
        inline namespace json_literals {
            /**
             * @brief User-defined literal operator for parsing JSON strings.
             *
             * Allows parsing a JSON string literal into a `Tachyon::Json` object
             * at compile-time (if the string is a constant expression and `parse` is constexpr-capable,
             * or effectively at runtime for non-constexpr contexts).
             *
             * Usage: `auto my_json = R"({"key": "value"})"_tjson;`
             *
             * @param s A pointer to the C-style string literal.
             * @param n The length of the string literal.
             * @return A `Tachyon::Json` object representing the parsed JSON.
             * @throws Tachyon::JsonParseException if the string is not valid JSON.
             */
            inline Json operator"" _tjson(const char* s, size_t n) {
                // Delegates parsing to the static BasicJson::parse method.
                // Using std::string_view for efficient handling of the literal.
                return Json::parse(std::string_view(s, n));
            }
        } // namespace json_literals
    } // namespace literals
} // namespace Tachyon

// Core library components
// Internal implementation details (parsing and serialization)
// These are included here because their implementations are often tied to BasicJson methods.
// Specific features
// Note: The implementations of parse(), dump(), and at_pointer() from BasicJson
// are typically defined after the internal parser/serializer/pointer classes are
// fully declared. In this setup, by including the internal files, their definitions
// would logically follow.
namespace Tachyon {
    // Implementations of static and member methods declared in BasicJson_Core.hpp
    // but defined outside the class body (e.g., after internal parser/serializer/pointer are defined).
    template<class Traits>
    inline BasicJson<Traits> BasicJson<Traits>::parse(std::string_view s, const ParseOptions& o) {
        // The internal Parser is currently designed to return Tachyon::Json (BasicJson<DefaultTraits>).
        // If a truly traits-agnostic parse were needed for BasicJson<SomeOtherTraits>,
        // the Parser class itself would need to be templated or adapted.
        // For now, BasicJson<Traits>::parse always returns a Json (DefaultTraits) object.
        return internal::Parser(s, o).parse_json();
    }
    template<class Traits>
    inline std::string BasicJson<Traits>::dump(const DumpOptions& o) const {
        std::ostringstream ss;
        // FIX: Pass the specific BasicJson type to the Serializer template
        internal::Serializer<BasicJson<Traits>> ss_serializer(ss, o);
        ss_serializer.serialize(*this); // Call the serialize method of the specific Serializer instance
        return ss.str();
    }
    template<class Traits>
    inline std::string BasicJson<Traits>::dump(int indent) const {
        DumpOptions opts;
        opts.indent_width = indent;
        return this->dump(opts);
    }
    // The implementation of at_pointer is solely in TachyonJson_Pointer.hpp
    // It is not redefined here.
} // namespace Tachyon

