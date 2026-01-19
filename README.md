# Tachyon v8.0 "Supernova"

**The Ultimate Hybrid JSON Library (C++11 / C++17)**

Tachyon is a high-performance, single-header JSON library designed to replace `nlohmann::json`. It features a unique **Hybrid Engine** that ensures strict C++11 compliance on legacy systems while automatically activating C++17 fast-paths and AVX-512 optimizations on modern compilers.

## ⚡ Hybrid Architecture

Tachyon adapts to your build environment:

| Feature | Legacy Mode (C++11) | Modern Mode (C++17/20) |
| :--- | :--- | :--- |
| **Number Parsing** | `strtod` / `strtoll` | `std::from_chars` (2-3x Faster) |
| **SIMD** | Scalar / AVX2 (if enabled) | AVX-512 (if enabled) |
| **Storage** | `std::vector` (Flat Layout) | `std::vector` (Flat Layout) |
| **Safety** | Stack Guard | Stack Guard |

## 🚀 Performance

*Comparison vs Nlohmann JSON (v3.11.3)*

| Dataset | Metric | Nlohmann | Tachyon (Modern) | Improvement |
| :--- | :--- | :--- | :--- | :--- |
| **Small** (Latency) | Throughput | ~18 MB/s | **~80 MB/s** | **4.5x** |
| **Canada** (Floats) | Throughput | ~17 MB/s | **~43 MB/s** | **2.5x** |
| **Large** (Throughput) | Throughput | ~29 MB/s | **~64 MB/s** | **2.2x** |

## 🛠️ Usage

**Drop-in Replacement**:
```cpp
// #include <nlohmann/json.hpp>
#include "tachyon.hpp"

using json = nlohmann::json; // Alias provided automatically

int main() {
    json j = json::parse(R"({"fast": true})");
    for (auto& [key, val] : j.items()) {
        std::cout << key << ": " << val << "\n";
    }
}
```

## 📜 License

MIT License.
