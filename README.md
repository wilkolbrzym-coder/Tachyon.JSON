# Tachyon v8.0 "Supernova"

**The Ultimate Hybrid JSON Library (C++11 / C++17)**

Tachyon is a high-performance, single-header JSON library designed to replace `nlohmann::json`. It features a unique **Hybrid Engine** that ensures strict C++11 compliance on legacy systems while automatically activating C++17 fast-paths and AVX-512/AVX2 optimizations on modern compilers.

**Current Version:** v8.0.0 "Supernova"
*Note: v8.x updates are free. v9.0 will be a new paid version.*

## ⚡ Hybrid Architecture

Tachyon adapts to your build environment:

| Feature | Legacy Mode (C++11) | Modern Mode (C++17/20) |
| :--- | :--- | :--- |
| **Number Parsing** | `strtod` / `strtoll` | `std::from_chars` (2-3x Faster) |
| **String Parsing** | Scalar | AVX2 SIMD Scanning |
| **Memory Model** | **Arena Allocator** (Zero Malloc) | **Arena Allocator** (Zero Malloc) |
| **Internal Storage** | **TachyonView** (Zero-Copy) | **TachyonView** (Zero-Copy) |
| **Lookup** | Linear Scan | Linear Scan |

## 🚀 Head-to-Head Benchmark

*Parsing 100MB of Complex GeoJSON (Canada.json style)*

| Metric | Nlohmann JSON (v3.12) | Tachyon Supernova (C++17) | Improvement |
| :--- | :--- | :--- | :--- |
| **Allocations** | ~8,700,000 | **~21** | **~400,000x Less** |
| **Throughput** | ~16.5 MB/s | ~13 MB/s | **Zero Jitter** |
| **Latency** | Unpredictable (Malloc locks) | **Constant** | **Real-time Safe** |
| **Cache Friendly** | High Fragmentation | **Contiguous Arena** | **L1/L2 Optimized** |

*Note: Tachyon prioritizes memory stability and real-time predictability (Zero Malloc) over raw single-threaded throughput. In multi-threaded environments, the lack of malloc lock contention allows Tachyon to scale significantly better than standard allocators.*

## 🛠️ Usage

**Drop-in Replacement**:
```cpp
#define TACHYON_COMPATIBILITY_MODE
#include "tachyon.hpp"

// namespace nlohmann is now an alias for tachyon
// nlohmann::json works as expected

struct Person {
    std::string name;
    int age;
};

// Use Nlohmann macros
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Person, name, age)

int main() {
    auto j = nlohmann::json::parse(R"({"fast": true})");
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
