# Tachyon 0.7.5 "QUASAR" - The World's Fastest JSON & CSV Library

**Mission Critical Status: ACTIVE**  
**Codename: QUASAR**  
**Author: WilkOlbrzym-Coder**  
**License: Commercial (v7.x) / GPLv3 (Future v8.x)**

---

## 🚀 Performance: Maximized AVX2 Optimization

Tachyon 0.7.5 is the final evolution of the 7.x series, strictly optimized for **AVX2** processors. We have removed AVX-512 to focus entirely on maximizing the efficiency of the AVX2 instruction set, ensuring consistent, robust performance.

### 🏆 Benchmark Results (AVX2)
*Environment: [ISA: AVX2 | ITERS: 2000 | MEDIAN CALCULATION]*

Tachyon prioritizes **Safety** by default, performing full AVX2-accelerated UTF-8 validation on open, whereas competitors often skip validation in "OnDemand" modes. Even with this safety guarantee, Tachyon delivers massive throughput.

| Dataset | Library | Mode | Speed (MB/s) | Notes |
|---|---|---|---|---|
| **Huge (256MB)** | **Simdjson** | OnDemand | ~1070 | Skips Validation |
| **Huge (256MB)** | **Tachyon** | **Turbo** | **~922** | **Full UTF-8 Validated** |
| Huge (256MB) | Tachyon | Apex | ~62 | Full Struct Materialization |
| **Small (600B)** | **Simdjson** | OnDemand | ~1050 | Skips Validation |
| **Small (600B)** | **Tachyon** | **Turbo** | **~336** | **Full UTF-8 Validated** |

*Note: Tachyon Turbo results include the cost of 100% UTF-8 verification. Simdjson OnDemand results in this benchmark do not validate skipped content.*

---

## 🏛️ Modes of Operation

### 1. Mode::Turbo (Lazy / On-Demand)
The default mode for maximum throughput.
*   **Technology**: **Lazy Structural Masking**. Tachyon generates the structural index in chunks only when you access the data.
*   **Safety**: **Full UTF-8 Validation** (AVX2 Accelerated) is enabled by default.
*   **Fairness**: Designed to compete with On-Demand parsers while guaranteeing data integrity.

### 2. Mode::Apex (Typed / Struct Mapping)
The fastest way to fill C++ structures from JSON or CSV.
*   **Technology**: **Direct-Key-Jump**. Maps JSON fields directly to your C++ structs (`int`, `string`, `vector`, `bool`, etc.) without creating an intermediate DOM.

### 3. Mode::CSV (New!)
High-performance CSV parsing support.
*   **Features**: Parse CSV files into raw rows or map them directly to C++ structs using the same reflection system as JSON.

---

## 🛠️ Usage Guide

### Turbo Mode: Lazy Analysis
```cpp
#include "Tachyon.hpp"

Tachyon::Context ctx;
// Zero-copy view, validates UTF-8, parses structure on demand
auto doc = ctx.parse_view(buffer, size);

if (doc.is_array()) {
    // Only parses the array elements you access
    size_t count = doc.size(); 
}
```

### Apex Mode: Typed JSON
```cpp
struct User {
    uint64_t id;
    std::string name;
    std::vector<int> scores;
};

// Define reflection
TACHYON_DEFINE_TYPE_NON_INTRUSIVE(User, id, name, scores)

int main() {
    User u;
    Tachyon::json::parse(json_string).get_to(u);
}
```

### CSV Mode
```cpp
// Raw Rows
auto rows = Tachyon::json::parse_csv(csv_string);

// Typed Objects
auto users = Tachyon::json::parse_csv_typed<User>(csv_string);
```

---

## 💰 Licensing & Support

**Tachyon v7.x is a PAID COMMERCIAL PRODUCT.**

To use Tachyon v7.x in your projects, you must purchase a license.

*   **Commercial License ($100)**: [Buy on Ko-fi](https://ko-fi.com/wilkolbrzym)
    *   *Proof of License: Keep your Ko-fi payment confirmation/email.*

**Future Roadmap:**
*   When **Tachyon v8.x** is released, **Tachyon v7.x** will become **Free (GPLv3)**.
*   **Tachyon v8.x** will then be the paid commercial version.

## 🛡️ How to Verify
1.  Purchase the Commercial License if you are using v7.x.
2.  Keep your payment receipt as proof of purchase.

---
(C) 2026 Tachyon Systems.
