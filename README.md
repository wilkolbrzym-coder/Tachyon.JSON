# Tachyon.JSON <sub>v3.0</sub>

![Language](https://img.shields.io/badge/language-C++20-blue.svg)
![Standard](https://img.shields.io/badge/standard-C++20-blue)
![License](https://img.shields.io/badge/license-MIT-green.svg)
[![Version](https://img.shields.io/badge/version-3.0.0--FINAL-brightgreen.svg)](https://github.com/YOUR_USERNAME/Tachyon.JSON)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)](https://github.com/YOUR_USERNAME/Tachyon.JSON/actions)

**Tachyon.JSON** is a modern, fast, and hyper-customizable single-header C++20 library for working with JSON. It is designed from the ground up for maximum flexibility, performance, and API elegance, making it an ideal choice for high-performance applications, game development, and embedded systems where fine-grained control is paramount.

Tachyon.JSON is not just another JSON library; it's a testament to modern C++ design principles, aiming to provide a highly performant, type-safe, and incredibly customizable framework for JSON manipulation that adapts to the developer's specific needs rather than dictating them.

## Table of Contents

- [Philosophy](#philosophy)
- [Key Features](#key-features)
- [Why Tachyon.JSON?](#why-tachyonjson)
- [Comparison with `nlohmann/json`](#comparison-with-nlohmannjson)
- [Quick Start](#quick-start)
  - [Installation](#installation)
  - [CMake Integration](#cmake-integration)
- [Usage Examples](#usage-examples)
  - [Creating JSON](#creating-json)
  - [Parsing JSON](#parsing-json)
  - [Serialization (Dumping)](#serialization-dumping)
  - [Accessing Data](#accessing-data)
  - [Iteration](#iteration)
  - [Error Handling](#error-handling)
- [User-Defined Types (UDTs)](#user-defined-types-udts)
- [Advanced Features](#advanced-features)
  - [JSON Pointer (RFC 6901)](#json-pointer-rfc-6901)
  - [User-Defined Literals](#user-defined-literals)
- [The `Traits` System: Ultimate Customization](#the-traits-system-ultimate-customization)
  - [Using `UnorderedJson` for Performance](#using-unorderedjson-for-performance)
  - [Defining Your Own Custom `Traits`](#defining-your-own-custom-traits)
- [API Reference (Overview)](#api-reference-overview)
- [License](#license)
- [Contributing](#contributing)

## Philosophy

The core philosophy of Tachyon.JSON is to provide developers with **uncompromising control** without sacrificing ease of use. While other libraries offer a one-size-fits-all solution, Tachyon.JSON acknowledges that different projects have different needs:
-   **Performance is not universal:** Some applications need fast object access (`std::unordered_map`), while others require deterministic output (`std::map`).
-   **Memory is critical:** The ability to specify custom allocators is essential for memory-constrained environments.
-   **Type safety is paramount:** Modern C++ allows for robust compile-time checks that prevent common errors.
-   **Clarity over "magic":** The library prefers explicit function calls (like `to_json`/`from_json`) for User-Defined Types to avoid complex and error-prone implicit SFINAE behavior, leading to more stable and predictable compilation.

## Key Features

*   **Header-Only:** Just include `Tachyon.hpp` in your project. No building, no linking, minimal dependencies.
*   **Modern C++20 Design:** Fully leverages C++20 features for compile-time safety, efficiency, and an ergonomic API.
*   **High-Performance Parser:**
    *   **Non-recursive, State-Machine-Based:** Efficiently handles deeply nested JSON structures, preventing stack overflows.
    *   **`std::from_chars` Powered:** Leverages `std::from_chars` for lightning-fast, locale-independent numeric parsing (where available).
    *   **Flexible Parsing Options:** Configurable `ParseOptions` to allow comments (single-line `//` and multi-line `/* */`) and trailing commas.
*   **Robust Type System:**
    *   **Distinct Number Types:** Separates `Integer` (`int64_t`) and `Float` (`double`) for precise representation.
    *   **Type-Safe `get<T>()`:** Provides highly robust, type-safe, and compile-time checked value retrieval with intelligent numeric conversions between `Integer` and `Float`.
*   **Flexible Serialization:** Customizable `DumpOptions` for pretty-printing (indentation, char), floating-point precision, optional key sorting for `unordered_map` based objects, and byte-level Unicode escaping (for non-ASCII characters as `\u00XX`).
*   **Detailed Error Reporting:** `JsonParseException` provides precise error messages including line number, column, and the problematic context, greatly aiding debugging of malformed input.
*   **Full JSON Standard Compliance:** Supports UTF-8 strings (including surrogate pairs), numbers, arrays, objects, `true`, `false`, and `null`.
*   **Powerful Customization with Traits:** Define custom underlying types for strings, objects (e.g., `std::map` for ordered keys or `std::unordered_map` for faster access), arrays, and even allocators.
*   **JSON Pointer Support:** Navigate complex JSON structures with `at_pointer()` (RFC 6901 compliant), including handling empty string keys (`"/"`).
*   **Extensible Serialization:** Easily serialize and deserialize custom C++ types to/from `Tachyon::Json` using explicit `to_json` and `from_json` ADL functions.
*   **User-Defined Literals:** Convenient `_tjson` literal for creating JSON objects directly from string literals.

## Why Tachyon.JSON?

Tachyon.JSON offers a unique blend of ease-of-use (intuitive API, header-only) and powerful, low-level control, all built upon modern C++20 idioms. Its `Traits` system provides unparalleled flexibility to tailor the internal data structures and allocators to your project's specific performance, memory, and ordering requirements. This makes it particularly well-suited for high-performance applications, game engines, or scenarios where every byte and cycle counts, providing a level of customization often missing in other JSON libraries.

## Comparison with `nlohmann/json`

`nlohmann/json` is a widely adopted and excellent JSON library. Tachyon.JSON does not aim to replace it, but rather to offer a compelling alternative for specific use cases.

### Tachyon.JSON's Strengths:

*   **True Single-Header, Zero External Dependencies:** Truly self-contained, ideal for projects with strict dependency constraints.
*   **Performance-Oriented Core:** Explicitly non-recursive parser and `std::from_chars` integration out-of-the-box offer potential performance benefits, especially for deeply nested or heavily numeric JSON.
*   **Unparalleled Customization:** The `Traits` system provides the deepest level of control over underlying container types and allocators, allowing for highly optimized configurations.
*   **Detailed Error Reporting:** Superior parse error messages with exact line/column/context for quick debugging.
*   **Flexible Parsing (Non-Standard):** Built-in support for comments and trailing commas is highly convenient for configuration files.
*   **Strict UDT Conversion:** By requiring explicit `to_json`/`from_json` calls for User-Defined Types (UDTs), Tachyon.JSON avoids complex SFINAE (Substitution Failure Is Not An Error) pitfalls that can occur with implicit conversions involving standard library containers, leading to greater compilation stability and clarity.

### `nlohmann/json`'s Strengths:

*   **Maturity & Community:** As an industry standard, it's extensively battle-tested with a vast community, comprehensive documentation, and a long track record.
*   **Rich Ecosystem:** Supports JSON Patch, JSON Merge Patch, and binary formats like CBOR, MessagePack, UBJSON, BSON.
*   **Full JSON RFC Adherence (Default):** Strictly adheres to the JSON standard by default, ensuring maximum compatibility.
*   **More Relaxed Implicit Conversions:** Offers more automatic type conversions for UDTs via constructor/`get<T>()`, which can be convenient (though potentially less strict in compilation environments).

**Choose Tachyon.JSON if:** you prioritize minimal dependencies, maximum performance control, detailed error messages, compilation stability (especially with complex UDTs), or need to parse JSON with comments/trailing commas.
**Choose `nlohmann/json` if:** you need the most mature, widely-adopted solution with a broad feature set and extensive community support, and are comfortable with its implicit conversion behaviors.

## Quick Start

### Installation

As a header-only library, simply copy `Tachyon.hpp` into your project and include it:
```cpp
#include "Tachyon.hpp"

// ...and you're ready to go!
using Json = Tachyon::Json; // Use the default Json (std::map based object)
// For unordered maps, use:
// using UnorderedJson = Tachyon::UnorderedJson;
```

### CMake Integration

You can easily integrate the library into your CMake project using `FetchContent` (requires CMake 3.11+):
```cmake
# In your CMakeLists.txt
include(FetchContent)
FetchContent_Declare(
  tachyon_json
  GIT_REPOSITORY https://github.com/Maciej0programista/Tachion.JSON/
  GIT_TAG        main # Or a specific release tag like v3.0.0-FINAL
)
FetchContent_MakeAvailable(tachyon_json)

# Link the library to your executable or library target
target_link_libraries(YourTarget PRIVATE tachyon_json)
```

## Usage Examples

### Creating JSON

```cpp
#include "Tachyon.hpp"
#include <iostream>

using Json = Tachyon::Json;

int main() {
    // Create JSON using an initializer list (object or array)
    Json j_object = {
        {"name", "Tachyon"},
        {"type", "library"},
        {"awesome", true},
        {"version", {
            {"major", 3},
            {"minor", 0},
            {"status", "FINAL"}
        }},
        {"features", Json::Array{"traits", "json_pointer", "exceptions", "performance"}}
    };

    Json j_array = Json::Array{ "value1", 123, true, Json::Null() };

    // Modify using the [] operator
    j_object["features"].push_back("header_only");
    j_object["features"][0] = "customizable_traits"; // Modify an existing element
    j_object["new_property"] = 42; // Add a new property

    std::cout << "Object JSON:\n" << j_object.dump(4) << std::endl;
    std::cout << "\nArray JSON:\n" << j_array.dump(4) << std::endl;
}
```

### Parsing JSON

```cpp
#include "Tachyon.hpp"
#include <iostream>

using Json = Tachyon::Json;

int main() {
    std::string json_string_with_features = R"(
        {
            "product": "Tachyon Library", // Single-line comment
            "price": 99.99,
            "is_beta": true, /* Multi-line comment */
            "tags": ["cpp", "json", ], // Trailing comma
            "release_date": null
        }
    )";

    // Parse with custom options
    Tachyon::ParseOptions options;
    options.allow_comments = true;
    options.allow_trailing_commas = true;
    options.max_depth = 256; // Increase max depth if needed for deep structures

    try {
        Json parsed_json = Json::parse(json_string_with_features, options);
        std::cout << "Parsed JSON:\n" << parsed_json.dump(2) << std::endl;
    } catch (const Tachyon::JsonParseException& e) {
        std::cerr << "Parsing error: " << e.what() << std::endl;
    }
}
```

### Serialization (Dumping)

```cpp
#include "Tachyon.hpp"
#include <iostream>

using Json = Tachyon::Json;

int main() {
    Json data = {
        {"id", 123},
        {"name", "Example Item"},
        {"price", 12.3456789},
        {"details", {
            {"material", "wood"},
            {"weight", 0.5}
        }}
    };

    // Serialize to a compact string (default)
    std::string compact_string = data.dump();
    std::cout << "Compact:\n" << compact_string << std::endl;

    // Serialize to a pretty-printed string with 4 spaces indent
    std::string pretty_string = data.dump(4);
    std::cout << "\nPretty (4 spaces):\n" << pretty_string << std::endl;

    // Custom dump options
    Tachyon::DumpOptions custom_opts;
    custom_opts.indent_width = 2; // 2 spaces indent
    custom_opts.indent_char = '-'; // Use '-' for indent
    custom_opts.float_precision = 2; // Only 2 decimal places for floats
    // For UnorderedJson, you could set custom_opts.sort_keys = true;
    // custom_opts.escape_unicode = true; // Use this to escape non-ASCII bytes as \u00XX
    
    std::string custom_dump_string = data.dump(custom_opts);
    std::cout << "\nCustom dump:\n" << custom_dump_string << std::endl;
}
```

### Accessing Data

```cpp
#include "Tachyon.hpp"
#include <iostream>

using Json = Tachyon::Json;

int main() {
    Json j = {
        {"product", "Smartphone"},
        {"price", 799.99},
        {"in_stock", true},
        {"quantity", 150},
        {"features", {"camera", "touchscreen", "5G"}},
        {"manufacturer", Json::Null()}
    };

    // Safe access with .at() (throws JsonException if key/index not found or wrong type)
    std::string product_name = j.at("product").get<Json::String>();
    double product_price = j.at("price").get<double>(); // get<double>() from Json::Float
    bool in_stock = j.at("in_stock").get<bool>();
    long long quantity = j.at("quantity").get<long long>(); // get<long long>() from Json::Integer

    std::cout << "Product: " << product_name << ", Price: " << product_price << std::endl;

    // Convenient access with operator[] (creates null if key/index doesn't exist)
    Json& features_array = j["features"];
    std::string first_feature = features_array.at(0).get<std::string>();
    std::cout << "First Feature: " << first_feature << std::endl;

    // Access using get() for fundamental types with conversions
    if (j.at("price").is_number()) {
        float price_float = j.at("price").get<float>(); // float from double
        int price_int = j.at("price").get<int>(); // int from double (truncation)
        std::cout << "Price as float: " << price_float << ", Price as int: " << price_int << std::endl;
    }
}
```

### Iteration

```cpp
#include "Tachyon.hpp"
#include <iostream>

using Json = Tachyon::Json;

int main() {
    // Iterate over an array
    Json arr = {"alpha", 1, true, 3.14};
    std::cout << "Array elements:" << std::endl;
    for (const auto& element : arr.get_ref<Json::Array>()) { // Use get_ref for direct container access
        std::cout << "- " << element.dump() << std::endl;
    }

    // Iterate over an object
    Json obj = {{"name", "Widget"}, {"id", 1001}, {"active", false}};
    std::cout << "\nObject properties:" << std::endl;
    for (const auto& pair : obj.get_ref<Json::Object>()) { // Use get_ref for direct container access
        std::cout << "- Key: \"" << pair.first << "\", Value: " << pair.second.dump() << std::endl;
    }
}
```

### Error Handling

Tachyon.JSON throws `Tachyon::JsonParseException` for parsing errors, providing detailed information about the location of the error.

```cpp
#include "Tachyon.hpp"
#include <iostream>

using Json = Tachyon::Json;

int main() {
    std::string malformed_json = R"(
        {
            "item1": 123,
            "item2": "hello"  // Missing comma here
            "item3": true
        }
    )";

    try {
        Json::parse(malformed_json);
    } catch (const Tachyon::JsonParseException& e) {
        std::cerr << "Caught a parsing error:\n" << e.what() << std::endl;
        // Example Output:
        // Caught a parsing error:
        // Parse error at line 4 col 24: Expected ',' but got '"'
        // Context:             "item2": "hello"
        //                             ^-- HERE
    } catch (const Tachyon::JsonException& e) {
        std::cerr << "Caught a generic JSON error: " << e.what() << std::endl;
    }
}
```

## User-Defined Types (UDTs)

You can easily serialize and deserialize your custom C++ structs/classes by providing `to_json` and `from_json` free functions in the same namespace as your type, or in the `Tachyon` namespace.

**Note:** Unlike some other libraries, Tachyon.JSON requires **explicit calls** to `to_json` and `from_json` for User-Defined Types. This design choice prevents complex SFINAE pitfalls and ensures higher compilation stability, especially when dealing with advanced type systems or custom allocators.

```cpp
#include "Tachyon.hpp"
#include <iostream>
#include <vector>
#include <optional> // For std::optional support

struct Address {
    std::string street;
    std::string city;
    int zip_code;
};

struct User {
    std::string name;
    int age;
    std::vector<std::string> roles;
    Address home_address;
    std::optional<std::string> email; // std::optional can be supported with custom to_json/from_json
};

// --- Custom serialization for Address (in Tachyon namespace for ADL) ---
namespace Tachyon {
    template<class TraitsType>
    void to_json(BasicJson<TraitsType>& j, const Address& a) {
        j = BasicJson<TraitsType>::Object{ // Explicitly construct an object
            {"street", a.street},
            {"city", a.city},
            {"zip_code", a.zip_code}
        };
    }

    // --- Custom deserialization for Address ---
    template<class TraitsType>
    void from_json(const BasicJson<TraitsType>& j, Address& a) {
        a.street = j.at("street").template get<std::string>();
        a.city = j.at("city").template get<std::string>();
        a.zip_code = j.at("zip_code").template get<int>();
    }
} // namespace Tachyon

// --- Custom serialization for User (in Tachyon namespace for ADL) ---
namespace Tachyon {
    template<class TraitsType>
    void to_json(BasicJson<TraitsType>& j, const User& u) {
        j = BasicJson<TraitsType>::Object{
            {"name", u.name},
            {"age", u.age},
        };
        
        // Handle vector<string> (standard conversion is provided in TachyonJson_Conversions.hpp)
        BasicJson<TraitsType> roles_json;
        Tachyon::to_json(roles_json, u.roles); // Explicitly call to_json for std::vector
        j["roles"] = roles_json;

        // Handle nested UDT (Address)
        BasicJson<TraitsType> address_json;
        Tachyon::to_json(address_json, u.home_address); // Explicitly call to_json for Address
        j["home_address"] = address_json;

        // Handle std::optional (requires custom handling if not directly supported by the library)
        if (u.email) {
            j["email"] = *u.email;
        } else {
            j["email"] = nullptr; // Represent std::nullopt as JSON null
        }
    }

    // --- Custom deserialization for User ---
    template<class TraitsType>
    void from_json(const BasicJson<TraitsType>& j, User& u) {
        u.name = j.at("name").template get<std::string>();
        u.age = j.at("age").template get<int>();
        
        // Handle vector<string>
        if (j.at("roles").is_array()) {
            Tachyon::from_json(j.at("roles"), u.roles); // Explicitly call from_json for std::vector
        }

        // Handle nested UDT (Address)
        if (j.at("home_address").is_object()) {
            Tachyon::from_json(j.at("home_address"), u.home_address); // Explicitly call from_json for Address
        }

        // Handle std::optional
        if (j.is_object() && j.get_ref<BasicJson<TraitsType>::Object>().count("email")) {
            const BasicJson<TraitsType>& email_json = j.at("email");
            if (email_json.is_null()) {
                u.email = std::nullopt;
            } else {
                u.email = email_json.template get<std::string>();
            }
        } else {
            u.email = std::nullopt;
        }
    }
} // namespace Tachyon

int main() {
    User alice = {
        "Alice", 30, {"Admin", "Developer"},
        {"123 Main St", "Anytown", 90210},
        std::make_optional<std::string>("alice@example.com")
    };

    // Serialize User to Json
    Tachyon::Json j_alice;
    Tachyon::to_json(j_alice, alice); // Explicit conversion using to_json
    std::cout << "Serialized User:\n" << j_alice.dump(4) << std::endl;

    // Deserialize Json to User
    std::string json_str = R"({"name":"Bob", "age":25, "roles":["Guest"], "home_address":{"street":"456 Side Ave","city":"Otherville","zip_code":10001}, "email":null})";
    Tachyon::Json j_bob = Tachyon::Json::parse(json_str);
    
    User bob;
    Tachyon::from_json(j_bob, bob); // Explicit deserialization using from_json
    std::cout << "\nDeserialized User:\nName: " << bob.name << ", Age: " << bob.age << ", Roles[0]: " << bob.roles[0] << ", City: " << bob.home_address.city;
    if (bob.email) {
        std::cout << ", Email: " << *bob.email << std::endl;
    } else {
        std::cout << ", Email: (null)" << std::endl;
    }
}
```

## Advanced Features

### JSON Pointer (RFC 6901)

Access nested elements using a simple path syntax, compliant with [RFC 6901](https://tools.ietf.org/html/rfc6901).

```cpp
#include "Tachyon.hpp"
#include <iostream>

using Json = Tachyon::Json;

int main() {
    Json j = Json::parse(R"(
        {
            "foo": ["bar", "baz"],
            "": 0,
            "a/b": 1,
            "c%d": 2,
            "e^f": 3,
            "g|h": 4,
            "i\\j": 5,
            "k\"l": 6,
            " ": 7,
            "m~n": 8,
            "test": {
                "nested_array": [
                    {"id": 1, "name": "Item A"},
                    {"id": 2, "name": "Item B"}
                ]
            }
        }
    )");

    try {
        std::cout << j.at_pointer("/foo/0").get<std::string>() << std::endl;             // "bar"
        std::cout << j.at_pointer("/test/nested_array/1/name").get<std::string>() << std::endl; // "Item B"
        std::cout << j.at_pointer("/m~0n").get<int>() << std::endl;                      // 8 (Note: "~0" for "~")
        std::cout << j.at_pointer("/a~1b").get<int>() << std::endl;                      // 1 (Note: "~1" for "/")
        std::cout << j.at_pointer("/").get<int>() << std::endl;                          // 0 (Accesses the empty key "")
        std::cout << j.at_pointer("/ ").get<int>() << std::endl;                         // 7 (Accesses the key with a space)

    } catch (const Tachyon::JsonPointerException& e) {
        std::cerr << "JSON Pointer error: " << e.what() << std::endl;
    } catch (const Tachyon::JsonException& e) {
        std::cerr << "General JSON error: " << e.what() << std::endl;
    }
}
```

### User-Defined Literals

Tachyon.JSON provides a convenient user-defined literal `_tjson` to parse JSON strings directly.

```cpp
#include "Tachyon.hpp"
#include <iostream>

// Bring the literal operator into scope
using namespace Tachyon::literals::json_literals;

int main() {
    // Create a JSON object directly from a string literal
    auto my_data = R"({"name":"Literal", "value":123})"_tjson;

    std::cout << "Parsed via literal: " << my_data.dump() << std::endl;
}
```

## The `Traits` System: Ultimate Customization

The `BasicJson<Traits>` class template is the heart of Tachyon.JSON's flexibility. By defining a custom `Traits` struct, you can change the fundamental building blocks of the JSON object.

### Using `UnorderedJson` for Performance

Tachyon.JSON provides a pre-configured alias, `Tachyon::UnorderedJson`, which uses `std::unordered_map` for JSON objects. This can provide a significant performance boost for applications that perform frequent key lookups and do not rely on key order.

```cpp
#include "Tachyon.hpp"
#include <iostream>

// Use the alias for the std::unordered_map version
using UnorderedJson = Tachyon::UnorderedJson;

int main() {
    UnorderedJson j_fast = {
        {"unordered", true},
        {"fast", true},
        {"data", {{"a", 1}, {"b", 2}}}
    };

    // The API is identical, but keys in dump() might not be sorted by default
    // (unless DumpOptions::sort_keys is true for UnorderedJson)
    std::cout << j_fast.dump(2) << std::endl;
    
    Tachyon::DumpOptions opts;
    opts.indent_width = 2;
    opts.sort_keys = true; // Force sorting for unordered map output
    std::cout << "\nSorted UnorderedJson:\n" << j_fast.dump(opts) << std::endl;
}
```

### Defining Your Own Custom `Traits`

For the ultimate control, you can define your own `Traits` struct. This is useful for:
-   Using custom, high-performance allocators (e.g., from Boost, or your own pool allocator).
-   Integrating with custom string or container types from other libraries.
-   Changing the numeric precision by using `float` or `long double` instead of `double`.

```cpp
#include "Tachyon.hpp"
#include <iostream>
#include <boost/container/flat_map.hpp> // Example: Using a different map type
#include <boost/container/vector.hpp>   // Example: Using a different vector type

// 1. Define your custom traits struct
template<class Allocator>
struct MyCustomTraits {
    template<class T> using Alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<T>;

    using StringType = std::basic_string<char, std::char_traits<char>, Alloc<char>>;
    using BooleanType = bool;
    using NumberIntegerType = long; // Use 'long' instead of 'int64_t'
    using NumberFloatType = float;  // Use 'float' instead of 'double' for less precision
    using NullType = std::nullptr_t;

    // Use Boost's flat_map for potentially faster lookups and better cache performance
    using ObjectType = boost::container::flat_map<
        StringType,
        Tachyon::BasicJson<MyCustomTraits<Allocator>>,
        std::less<StringType>,
        Alloc<std::pair<StringType, Tachyon::BasicJson<MyCustomTraits<Allocator>>>>
    >;

    // Use Boost's vector
    using ArrayType = boost::container::vector<
        Tachyon::BasicJson<MyCustomTraits<Allocator>>,
        Alloc<Tachyon::BasicJson<MyCustomTraits<Allocator>>>
    >;
};

// 2. Create a type alias for your custom JSON type
using MyCustomJson = Tachyon::BasicJson<MyCustomTraits<std::allocator<char>>>;

int main() {
    MyCustomJson j;
    j["message"] = "Using custom traits!";
    j["data"] = MyCustomJson::Array{1.0f, 2.5f, 3.0f}; // Values will be stored as floats

    std::cout << j.dump(2) << std::endl;
}
```

## API Reference (Overview)

A brief overview of the `Tachyon::BasicJson` API.

| Method                                      | Description                                                                 |
| ------------------------------------------- | --------------------------------------------------------------------------- |
| `Json::parse(str, [opts])`                  | Parses a JSON string into a `Json` object.                                    |
| `j.dump([opts])`                            | Serializes the `Json` object to a string.                                   |
| `j.type()`                                  | Returns the `JsonType` of the object.                                       |
| `j.is_null()`, `is_object()`, etc.          | Checks the type of the `Json` object.                                       |
| `j.at(key/index)`                           | Accesses an element with bounds/key checking. Throws on failure.            |
| `j[key/index]`                              | Accesses an element, creating it if it doesn't exist.                       |
| `j.get<T>()`                                | Retrieves the value as type `T` (for fundamental types).                     |
| `j.get_ref<T>()`                            | Returns a direct reference to the underlying container/value.                 |
| `j.at_pointer(ptr)`                         | Accesses a nested element using a JSON Pointer string.                       |
| `j.size()`                                  | Returns the size of an object, array, or string.                            |
| `j.empty()`                                 | Checks if an object, array, or string is empty, or if the value is null.    |
| `j.push_back(val)`                          | Appends a value to a JSON array.                                            |

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please feel free to open an issue to report a bug or suggest a feature, or a pull request to submit changes.
