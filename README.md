# Tachyon 0.7.5 "QUASAR" - The World's Fastest JSON & CSV Library

**Mission Critical Status: ACTIVE**  
**Codename: QUASAR**  
**Author: WilkOlbrzym-Coder**  
**License: Commercial (v7.x) / GPLv3 (Future v8.x)**

---

## 🚀 Performance: Maximized AVX2 Optimization

Tachyon 0.7.5 is the final evolution of the 7.x series, strictly optimized for **AVX2** processors. We have removed AVX-512 to focus entirely on maximizing the efficiency of the AVX2 instruction set, ensuring consistent, record-breaking performance across all modern x86 CPUs.

Every line of code has been hand-tuned to ensure that Tachyon dominates in both small-file (600 bytes) and large-file (256MB+) scenarios.

### 🏆 Benchmark Targets
*Environment: [ISA: AVX2 | ITERS: 2000 | MEDIAN CALCULATION]*

Tachyon aims to win against all competitors, including Simdjson and Glaze, by using intelligent "Lazy/On-Demand" parsing logic that only does the work you ask for.

*   **Canada.json**: Optimized for maximum throughput using Turbo Mode.
*   **Huge.json**: Optimized for memory bandwidth saturation.
*   **Small Files**: Optimized for low-latency startup.

---

## 🏛️ Modes of Operation

### 1. Mode::Turbo (Lazy / On-Demand)
The default mode for maximum throughput.
*   **Technology**: **Lazy Structural Masking**. Tachyon generates the structural index in chunks only when you access the data. If you skip a field, Tachyon skips the parsing.
*   **Fairness**: Matches Simdjson OnDemand behavior but with a highly optimized AVX2 kernel.
*   **Features**: **Full UTF-8 Validation** (AVX2 Accelerated) is enabled by default for safety.

### 2. Mode::Apex (Typed / Struct Mapping)
The fastest way to fill C++ structures from JSON or CSV.
*   **Technology**: **Direct-Key-Jump**. Maps JSON fields directly to your C++ structs (`int`, `string`, `vector`, `bool`, etc.) without creating an intermediate DOM.
*   **Equivalent**: Replaces Glaze/Nlohmann for typed parsing.

### 3. Mode::CSV (New!)
High-performance CSV parsing support.
*   **Features**: Parse CSV files into raw rows or map them directly to C++ structs using the same reflection system as JSON.

*(Note: Mode::Titan has been removed in favor of a unified, safe Turbo mode)*

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
*   This cycle ensures that cutting-edge performance supports development, while older stable versions eventually become open source.

## 🛡️ How to Verify
1.  Purchase the Commercial License if you are using v7.x.
2.  Keep your payment receipt as proof of purchase.

---
(C) 2026 Tachyon Systems.
