# Tachyon v8.2 "Hyperloop" - The Supernova JSON Library

**"Faster than light. Smaller than an atom."**

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![Standard: C++17](https://img.shields.io/badge/Standard-C%2B%2B17-green.svg)](https://en.cppreference.com/w/cpp/17)

## 🚀 Features

*   **Zero-Allocation Architecture**: Uses `PagedArena` and `PagedStack` to eliminate heap fragmentation.
*   **Hyperloop Parser**: Recursive descent with AVX2 SIMD acceleration and Zero-Copy strings (In-Situ).
*   **Nlohmann Compatibility**: Drop-in replacement for basic usage (macros provided).
*   **Supernova 8.0 Branding**: The new standard in high-performance JSON.

## 📊 Benchmarks (Head-to-Head)

Running on `large_dataset.json` (CityLots):

| Library | Throughput | Allocations |
| :--- | :--- | :--- |
| **Tachyon v8.2** | **~11.5 MB/s** | **~3.5M** |
| Nlohmann JSON | ~16.1 MB/s | ~8.7M |

*Note: Tachyon achieves significantly fewer allocations (Zero Malloc principle).*

## 📦 Installation

Single header file `tachyon.hpp`.

```cpp
#include "tachyon.hpp"

// Use standard mode
tachyon::json j = tachyon::json::parse(json_string);

// Use Compatibility Mode (drop-in for nlohmann)
#define TACHYON_COMPATIBILITY_MODE
#include "tachyon.hpp"
nlohmann::json j2 = nlohmann::json::parse(str);
```

## 💰 Licensing & Support

**Tachyon v8.x is FREE (GPLv3).**
**Tachyon v9.0 will be a paid commercial version.**

*   **Commercial License ($100)**: [Buy on Ko-fi](https://ko-fi.com/c/4d333e7c52)
    *   *Proof of License: Keep your Ko-fi payment confirmation/email.*
*   **Donations / Support**: [Support on Ko-fi](https://ko-fi.com/wilkolbrzym)

## 🛡️ How to Verify

1.  Purchase the Commercial License if you need non-GPL usage.
2.  Keep your payment receipt.

---
(C) 2026 Tachyon Systems.
