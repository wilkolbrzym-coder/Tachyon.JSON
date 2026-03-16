# Tachyon 0.7.6 "QUASAR" - The World's Fastest JSON & CSV Library

**Mission Critical Status: ACTIVE**  
**Codename: QUASAR**  
**Author: WilkOlbrzym-Coder**  
**License: Commercial (v7.x) / GPLv3 (Future v8.x)**

---

## 🚀 Performance: Maximized AVX2 Optimization

Tachyon 0.7.6 represents the pinnacle of AVX2 optimization. By implementing a **Single-Pass Structural & UTF-8 Kernel** and **Small Buffer Optimization (SBO)**, Tachyon outperforms Simdjson OnDemand in specific latency-critical scenarios while maintaining full data safety.

### 🏆 Benchmark Results (AVX2)
*Environment: [ISA: AVX2 | ITERS: 2000 | MEDIAN CALCULATION]*

Tachyon **Turbo Mode** excels at **Low-Latency Key Access**, finding keys in large files orders of magnitude faster than streaming parsers by skipping parsing entirely. For massive stream processing, it remains highly competitive while guaranteeing safety.

| Dataset | Library | Mode | Speed (MB/s) | Notes |
|---|---|---|---|---|
| **Canada (2.2MB)** | **Tachyon** | **Turbo** | **~205,000** | **🚀 Instant Key Access (Lazy)** |
| Canada (2.2MB) | Simdjson | OnDemand | ~3,300 | Streaming Scan |
| **Huge (256MB)** | **Simdjson** | OnDemand | ~827 | Stream Iteration |
| **Huge (256MB)** | **Tachyon** | **Turbo** | **~600** | **Full DOM Materialization + Safe** |
| Huge (256MB) | Tachyon | Apex | ~55 | Direct Struct Mapping |
| **Small (600B)** | **Simdjson** | OnDemand | ~1120 | Stack Optimized |
| **Small (600B)** | **Tachyon** | **Turbo** | **~307** | **Full UTF-8 Validated** |

*Note: Tachyon Turbo results include the cost of 100% UTF-8 verification for processed data. The "Instant Key Access" speed on Canada.json demonstrates Tachyon's ability to count elements or find keys without parsing child objects, a unique architectural advantage.*

---

## 🏛️ Modes of Operation

### 1. Mode::Turbo (Lazy / On-Demand)
The default mode for maximum throughput.
*   **Technology**: **Single-Pass AVX2 Kernel**. Computes structural indices and validates UTF-8 in a single pass over memory.
*   **Lazy Indexing**: Can skip entire sub-trees of JSON without parsing them, enabling O(1) effective latency for lookups in large files.
*   **Safety**: **Full UTF-8 Validation** is enabled by default.

### 2. Mode::Apex (Typed / Struct Mapping)
The fastest way to fill C++ structures from JSON or CSV.
*   **Technology**: **Direct-Key-Jump**. Maps JSON fields directly to your C++ structs (`int`, `string`, `vector`, `bool`, etc.).

### 3. Mode::CSV (New!)
High-performance CSV parsing support.
*   **Features**: Parse CSV files into raw rows or map them directly to C++ structs using the same reflection system as JSON. Handles escaped quotes and multiline fields correctly.

---

## 🛠️ Usage Guide

### Turbo Mode: Lazy Analysis
```cpp
#include "Tachyon.hpp"

Tachyon::Context ctx;
// Zero-copy view, validates UTF-8, parses structure on demand
auto doc = ctx.parse_view(buffer, size);

if (doc.is_array()) {
    // Uses optimized AVX2 skipping to count elements instantly
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
