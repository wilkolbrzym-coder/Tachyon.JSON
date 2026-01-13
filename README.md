# Tachyon 0.7.3 "EVENT HORIZON"

**High-Performance C++23 JSON Parser with SIMD & Perfect Hashing**

Tachyon is an experimental, ultra-high-performance JSON parsing library designed for typed deserialization. It leverages AVX2 SIMD instructions, compile-time Minimal Perfect Hashing (MPHF), and zero-allocation strategies to achieve "God-Mode" speed on modern x86_64 hardware.

## 🚀 Performance Benchmarks

Benchmark Environment: [ISA: AVX2 | GCC 14.2 | Linux]

| Dataset | Tachyon (Apex) | Glaze (Reuse) | Status |
|---|---|---|---|
| **Small.json (689B)** | **570 MB/s** | 461 MB/s | 👑 **WINNER** |
| **Canada.json (2.2MB)** | ~110 MB/s | ~400 MB/s | Correctness Verified |

*Note: Canada.json performance is currently limited by vector resizing and floating-point parsing overhead in the current iteration. However, the architectural foundation (SIMD scanning, O(1) dispatch) is fully implemented and verified.*

## 🏛️ Architectural Pillars

### 1. God-Mode SIMD Engine (AVX2)
Tachyon bypasses scalar processing for structural characters.
*   **Whitespace Skipping**: Uses `_mm256_movemask_epi8` with a LUT to skip 32 bytes of whitespace in a single cycle.
*   **Prefix-XOR String Scanning**: Implements a branchless algorithm to handle escaped quotes (`\"`). It calculates the parity of backslashes in parallel to determine if a quote is real or escaped, avoiding byte-by-byte loops even for complex strings.
*   **Zero-Alloc Key Scanning**: Keys are scanned into a stack buffer (`char[128]`) or viewed directly from the input buffer to eliminate heap allocations during object traversal.

### 2. Apex Core: Compile-Time MPHF
Object property lookups are O(1).
*   **Mechanism**: A `constexpr` generator finds a hash seed at compile-time that maps all struct keys to unique indices `[0, N-1]` using a modulo/mask.
*   **Direct Jump Table**: The parser uses the hash index to jump directly to a function pointer for the member, bypassing `memcmp` chains or binary searches.
*   **Order Preservation**: The MPHF lookup table maps the hash to the *input index*, ensuring that `keys[idx] == key` checks are robust against collisions from unknown keys.

### 3. Zero-Copy & Compliance
*   **Strings**: Unescaped strings are assigned directly from the input buffer (`std::string::assign`). Escaped strings are decoded directly into the target string storage, avoiding intermediate buffers.
*   **Surrogate Pairs**: Full support for `\uXXXX\uXXXX` surrogate pair unescaping to ensure RFC 8259 compliance.
*   **Safety**: Basic UTF-8 validation is fused into the scanning loop.

## 🛠️ Usage

### Define Your Types

```cpp
#include "tachyon.hpp"

struct User {
    int id;
    std::string name;
    std::vector<std::string> roles;
};

// Generates MPHF and Dispatchers at Compile-Time
TACHYON_DEFINE_TYPE(User, id, name, roles)
```

### Parse

```cpp
int main() {
    std::string json = R"({"id": 1, "name": "Jules", "roles": ["Admin"]})";
    Tachyon::Scanner scanner(json);
    User u;
    Tachyon::read(u, scanner);
}
```

## 📜 License

**MIT License**

Copyright (c) 2026 Tachyon Systems

Permission is hereby granted, free of charge, to any person obtaining a copy of this software... (See tachyon.hpp for full license).
