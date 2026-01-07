# Tachyon v7.0 - The World's Fastest JSON Parser

**Tachyon** is a next-generation C++20 JSON library designed for maximum throughput. It leverages AVX2 SIMD instructions, a "Tape" architecture, and a dual-mode API to deliver unparalleled performance.

## 🚀 Performance Metrics

Benchmark results on Cloud Environment (AVX2):

| Dataset | Library | Mode | Speed (MB/s) |
|---|---|---|---|
| **Large Array (50MB)** | **Tachyon** | **God Mode (Raw)** | **1040 MB/s** |
| | Glaze | Typed | 360 MB/s |
| | Nlohmann | DOM | 27 MB/s |
| **Canada (Complex)** | **Tachyon** | **God Mode (Raw)** | **593 MB/s** |
| | **Tachyon** | **Comfort (DOM)** | **437 MB/s** |
| | Glaze | Generic | 241 MB/s |
| | Nlohmann | DOM | 28 MB/s |

**Key Takeaways:**
*   **God Mode** is **3x faster** than Glaze Typed.
*   **Comfort Mode** is **15x faster** than Nlohmann JSON on complex data.

## 🌟 Dual-Mode Architecture

Tachyon v7.0 introduces two distinct ways to interact with JSON data:

### 1. Comfort Mode (DOM)
Designed for ease of use and Nlohmann compatibility. Uses a lazy DOM that materializes objects on demand.

```cpp
#include "Tachyon.hpp"

// Implicit parsing & conversion
Tachyon::json j = Tachyon::json::parse(json_string);

// Easy access
std::string name = j["name"];
double score = j["score"];

// Iterators
for (auto& element : j["array"]) {
    std::cout << element << "\n";
}
```

### 2. God Mode (Raw Engine)
Designed for **Zero-Copy, Zero-Allocation** parsing. You interact directly with the structural indices (Tape) and use SWAR (SIMD Within A Register) parsing primitives.

```cpp
#include "Tachyon.hpp"

Tachyon::Document doc;
doc.parse_view(data.data(), data.size());
Tachyon::Cursor c(&doc, 0, data.data());

// Manual traversal using structural indices
c.next(); // [
uint32_t obj_start = c.next(); // {
// ... Skip to value ...
const char* val_ptr = data.data() + offset;
int64_t id = Tachyon::json::parse_int_swar(val_ptr); // Fast SWAR parse
```

## 🔧 Installation

Single header file: `Tachyon.hpp`.

**Requirements:**
*   C++20 compliant compiler (GCC 10+, Clang 11+, MSVC 19.29+)
*   AVX2 support (`-mavx2` / `/arch:AVX2`)

```bash
g++ -O3 -mavx2 -std=c++20 main.cpp -o app
```

## ⚖️ License

Proprietary Technology. All Rights Reserved.
