# Tachyon.JSON

![Language](https://img.shields.io/badge/language-C%2B%2B20-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)
[![Version](https://img.shields.io/badge/version-2.0.0--BETA-orange.svg)](https://github.com/YOUR_USERNAME/Tachyon.JSON)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)](https://github.com/YOUR_USERNAME/Tachyon.JSON/actions)

**Tachyon.JSON** is a modern, fast, and feature-rich single-header C++20 library for working with JSON. It is designed for maximum flexibility, performance, and API elegance, making it an ideal choice for high-performance applications, game development, and embedded systems.

## Table of Contents

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
  - [Customization with Traits](#customization-with-traits)
  - [User-Defined Literals](#user-defined-literals)
- [License](#license)
- [Contributing](#contributing)

## Key Features

*   **Header-Only:** Just include `Tachyon.hpp` in your project. No building, no linking, minimal dependencies.
*   **High-Performance Parser:**
    *   **Non-recursive, State-Machine-Based:** Efficiently handles deeply nested JSON structures, preventing stack overflows.
    *   **`std::from_chars` Powered:** Leverages `std::from_chars` for lightning-fast, locale-independent numeric parsing (where available).
    *   **Flexible Parsing Options:** Configurable `ParseOptions` to allow comments (single-line `//` and multi-line `/* */`) and trailing commas.
*   **Robust Type System:**
    *   **Distinct Number Types:** Separates `Integer` (`int64_t`) and `Float` (`double`) for precise representation.
    *   **"ULTIMATE FIX" `get<T>()`:** Provides highly robust, type-safe, and compile-time checked value retrieval with intelligent conversions.
*   **Flexible Serialization:** Customizable `DumpOptions` for pretty-printing (indentation, char), floating-point precision, optional key sorting for `unordered_map` based objects, and full Unicode escaping.
*   **Detailed Error Reporting:** `JsonParseException` provides precise error messages including line number, column, and the problematic context, greatly aiding debugging of malformed input.
*   **Full JSON Standard Compliance:** Supports UTF-8 strings (including surrogate pairs), numbers, arrays, objects, `true`, `false`, and `null`.
*   **Powerful Customization with Traits:** Define custom underlying types for strings, objects (e.g., `std::map` for ordered keys or `std::unordered_map` for faster access), arrays, and even allocators.
*   **JSON Pointer Support:** Navigate complex JSON structures with `at_pointer()` (RFC 6901 compliant).
*   **Extensible Serialization:** Easily serialize and deserialize custom C++ types to/from `Tachyon::Json` using familiar `to_json` and `from_json` ADL functions.
*   **User-Defined Literals:** Convenient `_tjson` literal for creating JSON objects directly from string literals.

## Why Tachyon.JSON?

Tachyon.JSON offers a unique blend of ease-of-use (intuitive API, header-only) and powerful, low-level control. Its `Traits` system provides unparalleled flexibility to tailor the internal data structures and allocators to your project's specific performance, memory, and ordering requirements, a feature often missing in other JSON libraries. This makes it particularly well-suited for high-performance applications, game engines, or scenarios where every byte and cycle counts.

## Comparison with `nlohmann/json`

`nlohmann/json` is a widely adopted and excellent JSON library. Tachyon.JSON does not aim to replace it, but rather to offer a compelling alternative for specific use cases.

### Tachyon.JSON's Strengths:

*   **True Single-Header, Zero External Dependencies:** Truly self-contained, ideal for projects with strict dependency constraints.
*   **Performance-Oriented Core:** Explicitly non-recursive parser and `std::from_chars` integration out-of-the-box offer potential performance benefits, especially for deeply nested or heavily numeric JSON.
*   **Unparalleled Customization:** The `Traits` system provides the deepest level of control over underlying container types and allocators, allowing for highly optimized configurations.
*   **Detailed Error Reporting:** Superior parse error messages with exact line/column/context for quick debugging.
*   **Flexible Parsing (Non-Standard):** Built-in support for comments and trailing commas is highly convenient for configuration files.

### `nlohmann/json`'s Strengths:

*   **Maturity & Community:** As an industry standard, it's extensively battle-tested with a vast community, comprehensive documentation, and a long track record.
*   **Rich Ecosystem:** Supports JSON Patch, JSON Merge Patch, and binary formats like CBOR, MessagePack, UBJSON, BSON.
*   **Full JSON RFC Adherence (Default):** Strictly adheres to the JSON standard by default, ensuring maximum compatibility.
*   **More Relaxed Implicit Conversions:** Offers more automatic type conversions, which can be convenient (though potentially less strict).

**Choose Tachyon.JSON if:** you prioritize minimal dependencies, maximum performance control, detailed error messages, or need to parse JSON with comments/trailing commas.
**Choose `nlohmann/json` if:** you need the most mature, widely-adopted solution with a broad feature set and extensive community support.

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
  GIT_REPOSITORY https://github.com/YOUR_USERNAME/Tachyon.JSON.git
  GIT_TAG        main # Or a specific release tag like v2.0.0-BETA
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
            {"major", 2},
            {"minor", 0},
            {"status", "BETA"}
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

    // Access using get() for custom types or fundamental types with conversions
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

### User-Defined Types (UDTs)

You can easily serialize and deserialize your custom C++ structs/classes by providing `to_json` and `from_json` free functions in the same namespace as your type, or in the `Tachyon` namespace.

```cpp
#include "Tachyon.hpp"
#include <iostream>
#include <vector>

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
    std::optional<std::string> email; // std::optional is supported!
};

// --- Custom serialization for Address ---
void to_json(Tachyon::Json& j, const Address& a) {
    j = {
        {"street", a.street},
        {"city", a.city},
        {"zip_code", a.zip_code}
    };
}

// --- Custom deserialization for Address ---
void from_json(const Tachyon::Json& j, Address& a) {
    a.street = j.at("street").get<std::string>();
    a.city = j.at("city").get<std::string>();
    a.zip_code = j.at("zip_code").get<int>();
}

// --- Custom serialization for User ---
void to_json(Tachyon::Json& j, const User& u) {
    j = {
        {"name", u.name},
        {"age", u.age},
        {"roles", u.roles}, // std::vector<T> is automatically handled if T is convertible
        {"home_address", u.home_address} // Address is handled due to its to_json
    };
    if (u.email) {
        j["email"] = *u.email;
    }
}

// --- Custom deserialization for User ---
void from_json(const Tachyon::Json& j, User& u) {
    u.name = j.at("name").get<std::string>();
    u.age = j.at("age").get<int>();
    j.at("roles").get(u.roles); // Using get(T&) for deserializing vector
    j.at("home_address").get(u.home_address); // Using get(T&) for deserializing nested UDT
    if (j.is_object() && j.get_ref<Tachyon::Json::Object>().count("email")) {
        u.email = j.at("email").get<std::string>();
    } else {
        u.email = std::nullopt;
    }
}

int main() {
    User alice = {
        "Alice", 30, {"Admin", "Developer"},
        {"123 Main St", "Anytown", 90210},
        "alice@example.com"
    };

    // Serialize User to Json
    Tachyon::Json j_alice = alice; // Implicit conversion using to_json
    std::cout << "Serialized User:\n" << j_alice.dump(4) << std::endl;

    // Deserialize Json to User
    std::string json_str = R"({"name":"Bob", "age":25, "roles":["Guest"], "home_address":{"street":"456 Side Ave","city":"Otherville","zip_code":10001}})";
    Tachyon::Json j_bob = Tachyon::Json::parse(json_str);
    
    User bob;
    from_json(j_bob, bob); // Explicit deserialization using from_json
    std::cout << "\nDeserialized User:\nName: " << bob.name << ", Age: " << bob.age << ", Roles[0]: " << bob.roles[0] << ", City: " << bob.home_address.city << std::endl;
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
        std::cout << j.at_pointer("/i~1j").get<int>() << std::endl;                      // 5 (Note: "~1" for "/")
        std::cout << j.at_pointer("/k\"l").get<int>() << std::endl;                      // 6 (Note: escaped quote)

    } catch (const Tachyon::JsonPointerException& e) {
        std::cerr << "JSON Pointer error: " << e.what() << std::endl;
    } catch (const Tachyon::JsonException& e) {
        std::cerr << "General JSON error: " << e.what() << std::endl;
    }
}
```

### Customization with Traits

Tachyon.JSON is built with a `Traits` system allowing you to swap out internal types like `StringType`, `ObjectType`, and `ArrayType`.

By default, `Tachyon::Json` uses `DefaultTraits`, which employs `std::map` for objects (guaranteeing key order). If performance for object lookups is more critical than key order, you can use `Tachyon::UnorderedJson`, which uses `std::unordered_map`.

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

You can even define your own custom `Traits` to integrate with custom allocators or specific container types for highly specialized needs.

### User-Defined Literals

Tachyon.JSON provides a convenient user-defined literal `_tjson` to parse JSON strings directly at compile time (or runtime for non-constexpr strings).

```cpp
#include "Tachyon.hpp"
#include <iostream>

// Bring the literal operator into scope
using namespace Tachyon::literals::json_literals;

int main() {
    // Create a JSON object directly from a string literal
    auto my_data = R"({"name":"Literal", "value":123})"_tjson;

    std::cout << "Parsed via literal: " << my_data.dump() << std::endl;

    // This can be used for compile-time constants (if content is valid JSON constant)
    // constexpr auto compile_time_json = R"({"version": 2.0})"_tjson;
    // static_assert(compile_time_json.at("version").get<double>() == 2.0);
}
```

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please feel free to open an issue to report a bug or suggest a feature, or a pull request to submit Change.
