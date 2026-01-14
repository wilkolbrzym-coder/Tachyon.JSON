# Tachyon v8.0 "Supernova"

**The Undisputed High-Performance C++ JSON Library**

Tachyon is an HFT-grade, single-header JSON parser designed to replace `nlohmann::json`. It creates a flat-layout memory structure using Small Object Optimization (SOO) and parses using AVX-512/AVX2 SIMD instructions, resulting in **2x-4x faster performance** while maintaining 100% API compatibility.

## 🚀 Performance

*Environment: AVX2 | Compiler: GCC 10+ (-O3 -mavx2)*

| Dataset | Metric | Nlohmann | Tachyon | Improvement |
| :--- | :--- | :--- | :--- | :--- |
| **Small** (Latency) | Throughput | 18.33 MB/s | **82.49 MB/s** | **4.5x Faster** |
| | Allocations | 28 | **16** | **43% Reduction** |
| **Canada** (Geometry) | Throughput | 17.64 MB/s | **43.35 MB/s** | **2.5x Faster** |
| **Large** (Throughput) | Throughput | 29.24 MB/s | **64.09 MB/s** | **2.2x Faster** |

## ⚡ Why is it faster?

1.  **Fast-Float Parsing**: Tachyon uses C++17 `std::from_chars` for integer and floating-point parsing, eliminating the overhead of `strtod` and locale checks used by legacy parsers. This allows it to crush float-heavy datasets like `canada.json`.
2.  **SIMD Engine**: Structural characters (whitespace, braces, quotes) are processed using AVX-512 or AVX2 instructions, allowing the parser to skip irrelevant data at memory bandwidth speeds.
3.  **Flat-Layout Storage**: Objects are stored as `std::vector<std::pair<string, json>>` instead of node-based `std::map`. This reduces cache misses and heap fragmentation significantly.
4.  **Small Object Optimization (SOO)**: Integers, doubles, and booleans are stored inline within the `json` union, avoiding heap allocation for primitive values.

## 🛠️ Usage

Tachyon is a drop-in replacement.

```cpp
// Remove: #include <nlohmann/json.hpp>
#include "tachyon.hpp"

using json = tachyon::json;

int main() {
    auto j = json::parse(R"({"price": 1234.56, "currency": "USD"})");

    // Exact same API
    double price = j["price"];
    std::string currency = j["currency"];

    std::cout << j.dump(4) << std::endl;
}
```

## 📦 Installation

Just copy `tachyon.hpp` to your include directory. No dependencies.

## 📜 License

MIT License.
