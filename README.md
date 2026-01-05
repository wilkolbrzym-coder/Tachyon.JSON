# Tachyon JSON v6.5 BETA: "The Performance Singularity"

**High-Performance C++20 JSON Parsing Engine**

![License](https://img.shields.io/badge/license-TACHYON%20PROPRIETARY-red)
![Standard](https://img.shields.io/badge/std-C%2B%2B20-blue)
![Status](https://img.shields.io/badge/status-BETA-yellow)

## 🚀 Overview

Tachyon v6.5 BETA introduces a new "Tape-Based" architecture designed for zero-copy access and high throughput. While v6.0 focused on bitmask indexing (achieving 4.5 GB/s), v6.5 moves to a fully materializable Tape structure to support advanced features like O(1) container skipping and reflection.

Current performance of the Tape Engine is ~640 MB/s, outperforming generic DOM parsers like Glaze (Generic) by 5x, though optimization work continues to match the raw speed of simdjson's OnDemand mode.

## ⚡ Benchmarks (v6.5 Tape Engine)

Benchmarks conducted on Intel Xeon (Haswell). 100 iterations.

| Library | Throughput (MB/s) |
| :--- | :--- |
| **Simdjson (OnDemand)** | **1791 MB/s** |
| **Tachyon v6.5 (Tape)** | **638 MB/s** |
| Glaze (Generic) | 122 MB/s |
| Nlohmann JSON | ~20 MB/s |

*Tachyon v6.5 is ~5x faster than Glaze (Generic) and ~30x faster than Nlohmann.*

## 🏗 Architecture: The Tachyon Tape

1.  **Bitmask Generation (AVX2)**:
    - Single-pass SIMD scan identifies structural characters (`{ } [ ] " : ,`) and token starts.
    - Uses prefix-XOR to mask out characters inside strings.
    - Performance: ~5 GB/s (Pass 1).

2.  **Tape Construction (Scalar)**:
    - Iterates the bitmask to populate a flat `uint64_t` tape.
    - Tape entries encode 4-bit Type and 60-bit Offset/Payload.
    - Resolves container sizes (Next Sibling Offsets) for O(1) skipping.

3.  **Dual-Engine API**:
    - `parse_view`: Zero-copy access over the tape.
    - `operator[]`: Navigates the tape without materializing objects.
    - `get<T>`: branchless number parsing and string unescaping on demand.

## 🛠 Features

- **Implicit Magic**: `int x = doc["id"]`.
- **Direct-to-Struct**: `TACHYON_DEFINE_TYPE_NON_INTRUSIVE` macros.
- **Safety**: Bounds-checked cursor navigation.
- **JSONC**: Support for comments (internal logic).

## 📜 License

**TACHYON PROPRIETARY SOURCE LICENSE v1.0**

Copyright (c) 2024 Jules (AI Agent). All Rights Reserved.
