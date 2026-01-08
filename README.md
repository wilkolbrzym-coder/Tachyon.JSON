# Tachyon 0.7.2 "QUASAR" - The World's Fastest JSON Library

**Mission Critical Status: ACTIVE**  
**Codename: QUASAR**  
**Author: WilkOlbrzym-Coder**  
**License: Business Source License 1.1 (BSL)**

---

## 🚀 Performance: At the Edge of Physics

Tachyon 0.7.2 is not just a library; it is a weapon of mass optimization. Built with a "Dual-Engine" architecture targeting AVX2 and AVX-512, it pushes x86 hardware to its absolute physical limits.

### 🏆 Benchmark Results: AVX-512 ("God Mode")
*Environment: [ISA: AVX-512 | ITERS: 50 | WARMUP: 20]*

At the throughput levels shown below, the margin of error is so minuscule that **Tachyon** and **Simdjson** are effectively tied for the world record. Depending on the CPU's thermal state and background noise, either library may win by a fraction of a percent.

| Dataset | Library | Speed (MB/s) | Median Time (s) | Status |
|---|---|---|---|---|
| **Canada.json** | **Tachyon (Turbo)** | **10,538.41** | 0.000203 | 👑 **JOINT WORLD RECORD** |
| Canada.json | Simdjson (Fair) | 10,247.31 | 0.000209 | Extreme Parity |
| Canada.json | Glaze (Reuse) | 617.48 | 0.003476 | Obsolete |
| **Huge (256MB)** | **Simdjson (Fair)** | **2,574.96** | 0.099419 | 👑 **JOINT WORLD RECORD** |
| Huge (256MB) | Tachyon (Turbo) | 2,545.57 | 0.100566 | Extreme Parity |
| Huge (256MB) | Glaze (Reuse) | 379.94 | 0.673788 | Obsolete |

### 🏆 Benchmark Results: AVX2 Baseline
| Dataset | Library | Speed (MB/s) | Status |
|---|---|---|---|
| **Canada.json** | **Tachyon (Turbo)** | **6,174.24** | 🥇 **Dominant** |
| Canada.json | Simdjson (Fair) | 3,312.34 | Defeated |
| **Huge (256MB)** | **Tachyon (Turbo)** | **1,672.49** | 🥇 **Dominant** |
| Huge (256MB) | Simdjson (Fair) | 1,096.11 | Defeated |

---

## 🏛️ The Four Pillars of Quasar

### 1. Mode::Turbo (The Throughput King)
Optimized for Big Data analysis where every nanosecond counts.
*   **Technology**: **Vectorized Depth Skipping**. Tachyon identifies object boundaries using SIMD and "teleports" over nested content to find array elements at memory-bus speeds.

### 2. Mode::Apex (The Typed Speedster)
The fastest way to fill C++ structures from JSON.
*   **Technology**: **Direct-Key-Jump**. Instead of building a DOM, Apex uses vectorized key searches to find fields and maps them directly to structs using zero-materialization logic.

### 3. Mode::Standard (The Balanced Warrior)
Classic DOM-based access with maximum flexibility.
*   **Features**: Full **JSONC** support (single-line and block comments) and materialized access to all fields.

### 4. Mode::Titan (The Tank)
Enterprise-grade safety for untrusted data.
*   **Hardening**: Includes **AVX-512 UTF-8 validation** kernels and strict bounds checking to prevent crashes or exploits on malformed input.

---

## 🛠️ Usage Guide

### Turbo Mode: Fast Analysis
Best for counting elements or calculating statistics on huge buffers.

```cpp
#include "Tachyon.hpp"

Tachyon::Context ctx;
auto doc = ctx.parse_view(buffer, size); // Zero-copy view

if (doc.is_array()) {
    // Uses the "Safe Depth Skip" AVX path for record-breaking speed
    size_t count = doc.size(); 
}
```

### Apex Mode: Direct Struct Mapping
Skip the DOM entirely and extract data into your own types.

```cpp
struct User {
    int64_t id;
    std::string name;
};

// Non-intrusive metadata
TACHYON_DEFINE_TYPE_NON_INTRUSIVE(User, id, name)

int main() {
    Tachyon::json j = Tachyon::json::parse(json_string);
    User u;
    j.get_to(u); // Apex Direct-Key-Jump fills the struct instantly
}
```

---

## 🧠 Architecture: The Dual-Engine
Tachyon detects your hardware at runtime and hot-swaps the parsing kernel.
*   **AVX2 Engine**: 32-byte-per-cycle classification using `vpshufb` tables.
*   **AVX-512 Engine**: 64-byte-per-cycle classification leveraging `k-mask` registers for branchless filtering.

---

## 🛡️ Licensing & Support Policy

**Business Source License 1.1 (BSL)**

Tachyon is licensed under the BSL. It is "Source-Available" software that automatically converts to the **MIT License** on **January 1, 2030**.

### Commercial Tiers:
*   **Free (Tier 0)**: Annual Revenue < $1M USD. **FREE** for production use. Attribution required.
*   **Paid (Tier 1-4)**: Annual Revenue > $1M USD. Requires a commercial agreement for production use.
    *   $1M - $5M Revenue: $2,499 (One-time payment).
    *   Over $5M Revenue: Annual subscription models.

### Bug-Fix Policy:
*   **Best Effort:** The Author provides a "Best Effort" bug-fix policy. If a reproducible critical bug is reported, the Author aims to provide a fix or workaround within **14 business days**.
*   **No Liability:** If a bug cannot be resolved within this timeframe or at all, the Author **assumes no legal responsibility or liability**. 

**PROHIBITION**: Unauthorized copying, modification, or extraction of the core SIMD structural kernels for use in other projects is strictly prohibited. The software is provided **"AS IS"** without any product warranty.

---

*(C) 2026 Tachyon Systems. Engineered by WilkOlbrzym-Coder.*