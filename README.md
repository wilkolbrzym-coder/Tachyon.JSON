# Tachyon 0.7.2 "QUASAR" - Technical Specification

**System Status:** Active  
**Codename:** QUASAR  
**Lead Developer:** WilkOlbrzym-Coder  
**License:** MIT License  

---

## 1. Performance Analysis

Tachyon 0.7.2 is a low-level optimization framework designed for high-throughput JSON processing. The library utilizes a "Dual-Engine" architecture targeting AVX2 and AVX-512 instruction sets, pushing hardware performance to the theoretical limits of the x86 platform.

### 1.1. Benchmark Metrics: AVX-512 (Extended Throughput)
*Environment: ISA: AVX-512 | Iterations: 50 | Warmup: 20*

| Dataset | Library | Throughput (MB/s) | Median Latency (s) | Global Standing |
|:---|:---|:---|:---|:---|
| **Canada.json** | **Tachyon (Turbo)** | **10,538.41** | 0.000203 | Joint World Record |
| Canada.json | Simdjson (Fair) | 10,247.31 | 0.000209 | Competitive Parity |
| **Huge (256MB)** | **Simdjson (Fair)** | **2,574.96** | 0.099419 | Joint World Record |
| Huge (256MB) | Tachyon (Turbo) | 2,545.57 | 0.100566 | Competitive Parity |

### 1.2. Benchmark Metrics: AVX2 Baseline
| Dataset | Library | Throughput (MB/s) | Performance Status |
|:---|:---|:---|:---|
| **Canada.json** | **Tachyon (Turbo)** | **6,174.24** | Dominant |
| Canada.json | Simdjson (Fair) | 3,312.34 | Performance Deficit |
| **Huge (256MB)** | **Tachyon (Turbo)** | **1,672.49** | Dominant |
| Huge (256MB) | Simdjson (Fair) | 1,096.11 | Performance Deficit |

---

## 2. Core Functional Pillars

### 2.1. Mode::Turbo (Throughput Optimization)
Optimized for large-scale data analysis where processing speed is the primary constraint.
*   **Vectorized Depth Skipping:** Tachyon identifies structural boundaries using SIMD registers, allowing the parser to bypass nested content and locate array elements at memory-bus speeds.

### 2.2. Mode::Apex (Typed Deserialization)
Designed for direct mapping of JSON data into C++ structures.
*   **Direct-Key-Jump:** This mode bypasses Document Object Model (DOM) materialization. It uses vectorized key searches to map fields directly to structures, reducing memory overhead and CPU cycles.

### 2.3. Mode::Standard (Comprehensive Access)
A balanced DOM-based implementation for general-purpose applications.
*   **Features:** Full support for JSONC (including single-line and block comments) and materialized access to all data fields.

### 2.4. Mode::Titan (Hardened Security)
Enterprise-grade security layer for processing untrusted or malformed data.
*   **Validation:** Implementation of AVX-512 UTF-8 validation kernels and strict boundary checking to mitigate potential buffer overflow exploits.

---

## 3. Implementation Examples

### 3.1. High-Speed Structural Analysis
```cpp
#include "Tachyon.hpp"

Tachyon::Context ctx;
auto doc = ctx.parse_view(buffer, size); // Zero-copy memory view

if (doc.is_array()) {
    // Utilizes the AVX-512 Safe Depth Skip path
    size_t count = doc.size(); 
}
```

### 3.2. Direct Structure Mapping
```cpp
struct User {
    int64_t id;
    std::string name;
};

// Non-intrusive metadata declaration
TACHYON_DEFINE_TYPE_NON_INTRUSIVE(User, id, name)

int main() {
    Tachyon::json j = Tachyon::json::parse(json_string);
    User u;
    j.get_to(u); // Direct-Key-Jump fills the structure directly
}
```

---

## 4. Licensing Information

**MIT License**

Copyright (c) 2026 Tachyon Systems

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

---

<div style="border: 2px solid #FF0000; padding: 15px; background-color: #FFF5F5; color: #D8000C; font-family: sans-serif;">
<strong>PROJECT STATUS: DISCONTINUED</strong><br>
Active development and maintenance of the Tachyon project have been terminated. This repository is no longer monitored. The software is provided as-is without any further updates or technical support.
</div>