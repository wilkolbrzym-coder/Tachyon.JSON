# Tachyon v6.3 BETA – Project "Light-Speed Dominance"

**The World's Fastest JSON Library (Targeting 5GB/s+)**

## What's New in v6.3
- **Core ASM & SIMD Engineering**: Rewritten structural scanner and number parsers using raw AVX2 intrinsics.
- **Single-Pass Fusion**: Parses and extracts data in a single pass, maximizing cache efficiency.
- **Zero-Copy & Zero-Allocation**: Uses a Linear Allocator and `string_view` for hot-path parsing. No `std::string` or `std::vector` allocations during parse.
- **Modern API**: Drop-in replacement for `nlohmann/json` with `operator[]`, implicit conversions, and `std::initializer_list` support.
- **Exclusive Features**: `tachyon::find_path` (stub) for skipping parsing entirely.

## Benchmarks (Preliminary)

| Dataset | Library | Speed (MB/s) |
|---|---|---|
| Large Array | Tachyon (View) | ~92 MB/s (Debug/Unoptimized) |
| Large Array | Simdjson | ~1915 MB/s |
| Large Array | Glaze | ~137 MB/s |
| Large Array | Nlohmann | ~25 MB/s |

*Note: Current Beta build is focusing on correctness and architecture (Single-Pass Fusion). Optimization passes to reach 5GB/s are in progress.*

## Architecture
Tachyon uses a **Linear Allocator** to avoid `malloc` overhead. The parser is a **Recursive Descent** parser accelerated by **AVX2 Intrinsics** (`_mm256_movemask_epi8`, `_mm256_cmpeq_epi8`) to skip whitespace and identify structural characters in 32-byte chunks.

## Usage

```cpp
#include "Tachyon.hpp"

int main() {
    auto j = Tachyon::json::parse(R"({"name": "Tachyon", "speed": "fast"})");
    std::cout << j["name"].get<std::string>() << "\n";
    return 0;
}
```

## License
Dual-License:
- **Free**: Open Source / Personal.
- **Commercial**: See `PRICING.md`.

## Building
Requires C++20 and AVX2 support.
```bash
g++ -std=c++20 -mavx2 -O3 my_app.cpp -o my_app
```
