# Tachyon.JSON

![Language](https://img.shields.io/badge/language-C%2B%2B20-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)

**Tachyon.JSON** is a modern, feature-rich, header-only C++20 library for working with JSON. It is designed for maximum flexibility, performance, and API elegance.

## Table of Contents

- [Key Features](#key-features)
- [Why Tachyon.JSON?](#why-tachyonjson)
- [Quick Start](#quick-start)
  - [Installation](#installation)
  - [CMake Integration](#cmake-integration)
- [Usage Examples](#usage-examples)
  - [Creating JSON](#creating-json)
  - [Parsing and Serialization](#parsing-and-serialization)
  - [Accessing Data](#accessing-data)
  - [Iteration](#iteration)
  - [Error Handling](#error-handling)
- [Advanced Features](#advanced-features)
  - [JSON Pointer](#json-pointer-rfc-6901)
  - [Customization with Traits](#customization-with-traits)
- [License](#license)
- [Contributing](#contributing)

## Key Features

*   **Header-Only:** Just include `Tachyon.hpp` in your project. No building, no linking.
*   **Intuitive API:** A clean, modern syntax inspired by best practices, making it easy to learn and use.
*   **Highly Customizable:** Use `std::map` or `std::unordered_map` for objects, or even define your own custom types via a powerful `Traits` system.
*   **Safe & Robust Error Handling:** Detailed `JsonParseException` exceptions provide the exact line and column of any parsing error.
*   **Full Standard Compliance:** Supports UTF-8 strings (including surrogate pairs), numbers, arrays, objects, `true`, `false`, and `null`.
*   **Advanced Functionality:** Built-in support for [JSON Pointer (RFC 6901)](https://tools.ietf.org/html/rfc6901).
*   **Flexible Parsing:** Optional support for comments and trailing commas.

## Why Tachyon.JSON?

Tachyon.JSON combines the ease of use found in the most popular JSON libraries with unique architectural flexibility. The `Traits` system allows you to precisely tailor the internal data structures to your project's specific performance or memory requirements, a feature rarely found in other libraries.

## Quick Start

### Installation

As a header-only library, simply copy `Tachyon.hpp` into your project and include it:
```cpp
#include "Tachyon.hpp"

// ...and you're ready to go!
using Json = Tachyon::Json;
```

### CMake Integration

You can easily integrate the library into your CMake project:
```cmake
# Add the library via FetchContent or as a submodule
include(FetchContent)
FetchContent_Declare(
  tachyon_json
  GIT_REPOSITORY https://github.com/YOUR_USERNAME/Tachyon.JSON.git
  GIT_TAG        main # or a specific release tag
)
FetchContent_MakeAvailable(tachyon_json)

# ...then link it to your target
target_link_libraries(YourTarget PRIVATE tachyon_json)
```

## Usage Examples

### Creating JSON

```cpp
#include "Tachyon.hpp"
#include <iostream>

using Json = Tachyon::Json;

int main() {
    // Create JSON using an initializer list
    Json j = {
        {"name", "Tachyon"},
        {"type", "library"},
        {"awesome", true},
        {"version", {
            {"major", 1},
            {"minor", 0}
        }},
        {"features", {"traits", "json_pointer", "exceptions"}}
    };

    // Modify using the [] operator
    j["features"].push_back("header_only");

    std::cout << j.dump(4) << std::endl;
}
```

### Parsing and Serialization

```cpp
// Parse a string
auto parsed_json = Json::parse(R"({ "hello": "world" })");

// Serialize to a pretty-printed string
std::string pretty_string = parsed_json.dump(4);
std::cout << pretty_string << std::endl;

// Serialize to a compact string
std::string compact_string = parsed_json.dump();
std::cout << compact_string << std::endl;
```

### Accessing Data

```cpp
Json j = { {"pi", 3.141}, {"happy", true} };

// Safe access with .at() (throws an exception on error)
double pi = j.at("pi").get<Json::Float>();

// Convenient access with operator[]
bool happy = j["happy"].get<Json::Boolean>();

// Convert to fundamental types
if (j.at("pi").is_number()) {
    double value = j.at("pi").get<Json::Float>();
}
```

### Iteration

```cpp
// Iterate over an array
Json arr = {"one", 2, false};
for (const auto& element : arr.get<Json::Array>()) {
    // ...
}

// Iterate over an object
Json obj = {{"a", 1}, {"b", 2}};
for (const auto& [key, value] : obj.get<Json::Object>()) {
    std::cout << key << ": " << value.dump() << std::endl;
}
```

### Error Handling

```cpp
try {
    // Malformed JSON (missing comma)
    auto j = Json::parse(R"({ "key1": 1 "key2": 2 })");
} catch (const Tachyon::JsonParseException& e) {
    std::cerr << e.what() << std::endl;
    // Output: Parse error at line 1 col 12: Expected ','
}
```

## Advanced Features

### JSON Pointer (RFC 6901)

Access nested elements using a simple path syntax.
```cpp
Json j = {
    {"users", {
        {{"name", "Alice"}, {"id", 101}},
        {{"name", "Bob"}, {"id", 102}}
    }}
};

// Access the name of the second user
const auto& bobs_name = j.at_pointer("/users/1/name");
std::cout << bobs_name.get<Json::String>() << std::endl; // Prints "Bob"
```

### Customization with Traits

By default, Tachyon.JSON uses `std::map` for objects. If hash-based performance is more important than key order, you can use `UnorderedJson`:
```cpp
#include "Tachyon.hpp"

// Use the alias for the std::unordered_map version
using UnorderedJson = Tachyon::UnorderedJson;

UnorderedJson j_fast = {
    {"unordered", true},
    {"fast", true}
};

// ...the API is identical!
```
You can define your own `Traits` to use custom allocators, strings, or containers, giving you unparalleled control over performance and memory usage.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please feel free to open an issue to report a bug or suggest a feature, or a pull request to submit changes.
