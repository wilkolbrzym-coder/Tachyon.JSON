# Tachyon v8.0 "Supernova"

**The Ultimate Hybrid JSON Library (C++11 / C++17)**

Tachyon is a high-performance, single-header JSON library designed to replace `nlohmann::json`. It features a unique **Hybrid Engine** that ensures strict C++11 compliance on legacy systems while automatically activating C++17 fast-paths and AVX-512 optimizations on modern compilers.

**Current Version:** v8.0.0 "Supernova"
*Note: v8.x updates are free. v9.0 will be a new paid version.*

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

| Dataset | Metric | Nlohmann | Tachyon (C++11) | Tachyon (C++17) |
| :--- | :--- | :--- | :--- | :--- |
| **Canada** (Floats) | Throughput | ~20 MB/s | **~25 MB/s** | **~36 MB/s** |
| **Unicode** (Strings) | Throughput | ~59 MB/s | **~100 MB/s** | **~94 MB/s** |

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

## 💰 Licensing & Support

This library is dual-licensed.

**Open Source License:**
[GNU GPL v3](LICENSE). Free for open-source projects.

**Commercial License ($100):**
For proprietary use, please purchase a commercial license:
[**Buy Commercial License**](https://ko-fi.com/c/4d333e7c52)

**Donations & Support:**
Support the development: [**Donate on Ko-fi**](https://ko-fi.com/wilkolbrzym)

### 🛡️ How to Verify
Keep your Ko-fi payment confirmation/email as your **Proof of Commercial License**.
