# Tachyon.JSON <sub>v5.0 Turbo</sub>

![Language](https://img.shields.io/badge/language-C++23-blue.svg)
![Standard](https://img.shields.io/badge/standard-C++23-blue)
![License](https://img.shields.io/badge/license-MIT-green.svg)
[![Version](https://img.shields.io/badge/version-5.0.0--Turbo-brightgreen.svg)](https://github.com/YOUR_USERNAME/Tachyon.JSON)

**Tachyon.JSON V5 "Turbo"** is the next evolution of high-performance JSON processing for C++. Rebuilt from the ground up using **C++23**, it abandons legacy C++ patterns in favor of modern Concepts, `std::variant`, and contiguous memory layouts to deliver the fastest, most cache-efficient, and developer-friendly JSON library available.

Tachyon V5 isn't just an update; it's a revolution. It strips away the bloat of template metaprogramming (SFINAE) and replaces it with clean, readable, and lightning-fast code. It introduces a new hybrid object storage system that rivals `std::unordered_map` in speed while maintaining deterministic memory usage and superior cache locality.

## 🚀 What's New in V5 Turbo?

Version 5.0 is the biggest overhaul in the library's history. Here is an extensive breakdown of the improvements:

### 1. Pure C++23 Architecture
*   **Concepts-Driven API:** We removed verbose `std::enable_if` SFINAE hacks. V5 uses C++20/23 **Concepts** (`requires`, `std::convertible_to`, etc.) to enforce type safety at compile time. This results in readable compiler error messages and faster build times.
*   **`std::variant` Core:** The underlying node storage now uses `std::variant`, eliminating unsafe unions and custom type discriminators. This standardizes the memory layout and improves safety.
*   **`std::from_chars` & `std::to_chars`:** Parsing and serialization of numbers now exclusively use the C++17/20 high-performance conversion primitives, bypassing standard IOStreams and locale dependencies completely for maximum throughput.

### 2. High-Performance `ObjectMap`
*   **Vector-Based Storage:** In V4, objects were `std::map` (RB-Tree) or `std::unordered_map` (Hash Table). V5 introduces `ObjectMap`, which stores members in a **sorted `std::vector`**.
    *   **Cache Locality:** Data is stored contiguously in memory. Iterating over an object is now a linear memory walk, drastically reducing CPU cache misses compared to node-based containers.
    *   **O(log N) Lookups:** The parser automatically sorts object keys. Subsequent lookups use binary search (`std::lower_bound`), offering O(log N) performance that often beats hash maps for typical JSON object sizes (small to medium) due to lack of hashing overhead.
    *   **Zero Allocation Overhead:** Unlike maps that allocate a node for every entry, `ObjectMap` performs a single buffer allocation (plus resizes), reducing malloc pressure.

### 3. Zero-Copy Access (`get_ref`)
*   **The Problem:** Traditional `get<T>()` methods often return by value. For large strings, arrays, or objects, this forces a deep copy of the data, killing performance.
*   **The V5 Solution:** V5 introduces `get_ref<T>()`.
    *   **Direct Access:** Returns a `const T&` or `T&` directly to the internal storage.
    *   **No Copies:** Read and modify nested structures without a single byte being copied.
    *   **Safety:** Checked at runtime to ensure you are accessing the correct type.

### 4. Enhanced Parsing Engine
*   **Non-Recursive Logic:** The parser has been hardened to handle deeply nested structures without blowing up the stack.
*   **UTF-16 Surrogate Pair Support:** Full support for parsing escaped UTF-16 surrogate pairs (e.g., `\uD83D\uDE00` -> 😃) into valid UTF-8 strings.
*   **Trailing Commas & Comments:** Native support for "JSON5-like" features: standard C++ style comments (`//`, `/* */`) and trailing commas in arrays/objects are supported by default.

## Table of Contents

- [Philosophy](#philosophy)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [Performance Deep Dive](#performance-deep-dive)
- [API Reference](#api-reference)
    - [Parsing](#parsing)
    - [Serialization](#serialization)
    - [Accessing Data](#accessing-data)
    - [Modifying Data](#modifying-data)
- [Advanced Usage](#advanced-usage)
- [Error Handling](#error-handling)

## Philosophy

Tachyon V5 follows the **"Pay for what you use, but use the fastest by default"** principle.
*   **Simplicity:** One header. One class `Json`. No complex template typedefs needed for basic usage.
*   **Speed:** Default behaviors (like `ObjectMap`) are chosen for the 99% use case of high-performance games and realtime apps.
*   **Modernity:** We don't support old compilers. C++23 is the baseline, allowing us to use the best tools the language offers.

## Installation

Tachyon is a single-header library.

1.  **Download** `Tachyon.hpp` from the repository.
2.  **Include** it in your project.
3.  **Compile** with C++23 enabled (`-std=c++23` for GCC/Clang, `/std:c++latest` for MSVC).

```cpp
#include "Tachyon.hpp"
using namespace Tachyon;
```

## Quick Start

```cpp
#include "Tachyon.hpp"
#include <iostream>

using namespace Tachyon;

int main() {
    // 1. Creation using Initializer Lists
    Json player = {
        {"id", 12345},
        {"username", "SpeedRunner"},
        {"stats", {
            {"wins", 10},
            {"losses", 2}
        }},
        {"inventory", {"sword", "shield", "potion"}}
    };

    // 2. Modification
    player["stats"]["wins"] = 11; // Update value
    player["inventory"].push_back("map"); // Add to array
    player["active"] = true; // Add new field

    // 3. Serialization (Dump)
    std::cout << player.dump({.indent=4}) << std::endl;

    // 4. Zero-Copy Access
    // Get reference to the inventory array without copying
    // Note: Use Json::array_t and Json::object_t for underlying types
    const auto& inv = player["inventory"].get_ref<Json::array_t>();
    for(const auto& item : inv) {
        std::cout << "- " << item.get_ref<std::string>() << "\n";
    }
}
```

## Performance Deep Dive

### The `ObjectMap` Advantage
Most JSON libraries use `std::map` or `std::unordered_map`.
*   **std::map**: Allocates a node for every element. Pointers are scattered in heap memory. Iteration involves pointer chasing (cache misses).
*   **std::unordered_map**: Faster lookups, but high memory overhead for hash buckets and nodes. Non-deterministic order.

**Tachyon::ObjectMap** uses `std::vector<std::pair<std::string, Json>>`.
1.  **Parsing**: We parse all keys into the vector.
2.  **Sorting**: At the end of the object parse, we run `std::sort`.
3.  **Lookup**: We use `std::lower_bound` (Binary Search).
    *   For N=10 to N=100 (typical JSON object size), binary search on contiguous memory is often faster than computing a hash and chasing a pointer.
    *   Iterating `for (auto& [k,v] : obj)` reads memory linearly. This is the **fastest possible iteration**.

### Zero-Copy Strings
When you write `json["key"].get<std::string>()`, a copy is made.
In V5, you can write `json["key"].get_ref<std::string>()`. This returns a `const std::string&` pointing directly to the data inside the JSON node. This is crucial for high-performance applications handling large text blobs.

## API Reference

### Parsing

```cpp
// Simple Parse
Json j = Json::parse(R"({"x": 1})");

// Parse with Options
ParseOptions opts;
opts.allow_comments = true;       // Allow // and /* */
opts.allow_trailing_commas = true; // Allow [1, 2,]
j = Json::parse(json_string, opts);
```

### Serialization

```cpp
Json j = {{"x", 1}};

// Compact (no spaces)
std::string s = j.dump();

// Pretty Print
DumpOptions opts;
opts.indent = 4;        // Indent with 4 spaces
opts.indent_char = ' '; // Use space (or '\t')
std::string pretty = j.dump(opts);
```

### Accessing Data

**Safe Access (Copies)**
```cpp
int x = j["x"].get<int>();          // Automatic conversion from int64_t
double d = j["y"].get<double>();
float f = j["z"].get<float>();      // Automatic conversion
std::string s = j["s"].get<std::string>();
```

**Fast Access (References)**
```cpp
// Returns const std::string&
const std::string& ref = j["s"].get_ref<std::string>();

// Returns Json::array_t& (std::vector<Json>&)
auto& arr = j["arr"].get_ref<Json::array_t>();
```

**Safe Access with Defaults**
```cpp
// If "missing" doesn't exist or is wrong type, return 42
int val = j["missing"].get_or(42);
```

### Modifying Data

```cpp
Json j;

// Array
j = Json::array_t{};
j.push_back(10);
j.push_back("hello");

// Object
j = Json::object_t{};
j["key"] = "value";      // Insert or Assign
j["key"] = 123;          // Change type/value

// Type Check
if (j.is_object()) { ... }
if (j.is_array()) { ... }
```

## Advanced Usage

### Working with Unicode
Tachyon V5 fully supports UTF-8. It correctly parses unicode escape sequences, including surrogate pairs used for Emojis.

```cpp
Json j = Json::parse(R"({"emoji": "\uD83D\uDE00"})"); // Parses to 😃 (UTF-8 bytes)
std::cout << j["emoji"].get<std::string>(); // Prints 😃
```

### Structured Binding (Manual)
Since `Json` is a dynamic type, standard structured binding `auto [a, b] = json` is not supported directly. However, you can easily unpack arrays:

```cpp
Json arr = {1, 2};
int a = arr[0].get<int>();
int b = arr[1].get<int>();
```

## Error Handling

Tachyon throws `Tachyon::JsonParseException` for parsing errors and `Tachyon::JsonException` for usage errors (like type mismatches).

```cpp
try {
    Json j = Json::parse("{ bad json ");
} catch (const Tachyon::JsonParseException& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    std::cerr << "Line: " << e.line() << ", Col: " << e.column() << std::endl;
}
```

---

*Tachyon.JSON V5 is designed for those who need speed, simplicity, and modern C++. Enjoy the Turbo.*
