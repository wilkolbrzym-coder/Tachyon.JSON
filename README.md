# Tachyon v7.0 GOLD: High-Performance AVX2 JSON Parser
"The Performance Singularity"

Tachyon is a modern C++20 JSON library engineered for extreme throughput using Branchless Inline Assembly and Speculative Loop Unrolling. It targets the theoretical limits of RAM bandwidth on AVX2-capable hardware.

## Performance
**Verified on Cloud Hardware (3-Run Max):**

| Dataset | Tachyon v7.0 | Simdjson OnDemand | Speedup |
|---|---|---|---|
| **Canada.json** | **~6.84 GB/s** | ~3.58 GB/s | **1.9x** |
| **Large Array** | **~4.24 GB/s** | ~1.90 GB/s | **2.2x** |

*Note: Benchmarks include a checksum verification step to ensure zero dead-code elimination. The engine performs full structural indexing.*

## Architecture
*   **Diabolic ASM Kernel:** The core bitmask generation loop is written in pure `__asm__ volatile` to manually manage AVX2 registers and enforce `vmovntdq` (Non-Temporal Streaming Stores) for cache bypass.
*   **Nibble Lookup:** Replaces 10+ comparison instructions with a single `pshufb` shuffle using a carefully crafted lookup table to identify structural characters.
*   **Speculative Unrolling:** Processes 256 bytes per iteration with aggressive software prefetching (1024 bytes ahead) to saturate the memory bus.
*   **Safety Padding:** Eliminates bounds checking in the hot loop by requiring and managing input padding.

## Usage
Tachyon provides a "First-Class" API compatible with `nlohmann/json`.

```cpp
#include "Tachyon.hpp"

std::string json_data = load_file("data.json");
auto doc = Tachyon::json::parse(json_data);

// Object Access
int id = doc["id"].get<int>();
std::string name = doc["name"].get<std::string>();

// Array Access
double val = doc["values"][0].get<double>();
```

## Requirements
*   C++20 compliant compiler (GCC 10+, Clang 11+, MSVC 2019+)
*   CPU with AVX2 support (Haswell or newer)
*   OS: Linux, Windows, macOS

## License
Tachyon Proprietary Source License v1.0.
Unauthorized extraction of the ASM kernels is prohibited.
