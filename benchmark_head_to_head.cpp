#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <random>
#include <atomic>
#include <sstream>

#include "tachyon.hpp"
#include "include_benchmark/nlohmann_json.hpp"

// ALLOCATION TRACKER
std::atomic<size_t> g_alloc_count{0};
std::atomic<size_t> g_alloc_bytes{0};

void* operator new(size_t size) {
    g_alloc_count++;
    g_alloc_bytes += size;
    return malloc(size);
}
void operator delete(void* ptr) noexcept {
    free(ptr);
}
void operator delete(void* ptr, size_t) noexcept {
    free(ptr);
}

// RDTSC Utils
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

inline uint64_t rdtsc() {
    unsigned int lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}

// DATA GENERATOR
void generate_large_json(const std::string& filename) {
    std::cout << "Generating " << filename << " (~100MB)..." << std::endl;
    std::ofstream out(filename);
    out << "{ \"type\": \"FeatureCollection\", \"features\": [";
    // 100MB target. Each entry is approx 2KB. 50,000 entries.
    for (int i = 0; i < 50000; ++i) {
        if (i > 0) out << ",";
        out << R"({ "type": "Feature", "properties": { "name": "Region )" << i << R"(", "id": )" << i << R"(, "value": )" << (i * 1.234) << R"( }, "geometry": { "type": "Polygon", "coordinates": [[ )";
        for (int j = 0; j < 50; ++j) {
            if (j > 0) out << ",";
            out << "[" << (double)i/100.0 + j << "," << (double)j/100.0 + i << "]";
        }
        out << "]] } }";
    }
    out << "] }";
}

// BENCHMARK RUNNER
struct Result {
    double throughput_mbs;
    double cycles_per_byte;
    size_t alloc_count;
    double duration_sec;
};

template<typename JsonType>
Result run_bench(const std::string& data) {
    // Reset counters
    g_alloc_count = 0;
    g_alloc_bytes = 0;

    auto start_time = std::chrono::high_resolution_clock::now();
    uint64_t start_cycles = rdtsc();

    // 1. Parse
    auto j = JsonType::parse(data);

    // 2. Access 5 nested keys
    volatile double sum = 0;
    // features[0].properties.value
    // features[1000].geometry.type
    // features[last].properties.id
    // features[middle].type
    // root.type

    // Note: This access pattern assumes the structure generated above
    // nlohmann/tachyon should support array indexing and object lookup

    // Access logic requires knowledge of the structure.
    // The structure is { "features": [ ... ] }
    // We access "features" array.

    auto& features = j["features"];

    // Access 1
    sum += (double)features[0]["properties"]["value"];
    // Access 2
    std::string type = features[1000]["geometry"]["type"];
    // Access 3
    sum += (int)features[features.size()-1]["properties"]["id"];
    // Access 4
    std::string featType = features[features.size()/2]["type"];
    // Access 5
    std::string rootType = j["type"];

    // 3. Modify 1 value
    features[0]["properties"]["name"] = "Modified Name";

    // 4. Serialize back
    std::string out = j.dump();

    uint64_t end_cycles = rdtsc();
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> sec = end_time - start_time;

    // Prevent optimization
    (void)sum;
    (void)type;
    (void)featType;
    (void)rootType;
    (void)out.size();

    double mbs = (data.size() / (1024.0 * 1024.0)) / sec.count();
    double cpb = (double)(end_cycles - start_cycles) / data.size();

    return {mbs, cpb, g_alloc_count.load(), sec.count()};
}

int main() {
    std::string filename = "large_dataset.json";
    {
        std::ifstream f(filename);
        if (!f.good()) {
            generate_large_json(filename);
        }
    }

    std::string data;
    {
        std::ifstream t(filename);
        std::stringstream buffer;
        buffer << t.rdbuf();
        data = buffer.str();
    }
    std::cout << "Data Size: " << data.size() / (1024.0 * 1024.0) << " MB\n" << std::endl;

    std::cout << "Running Nlohmann..." << std::endl;
    Result res_nlohmann = run_bench<nlohmann::json>(data);

    std::cout << "Running Tachyon..." << std::endl;
    // For Tachyon, if we want to test "Zero malloc", we might need to reset the arena if we had one.
    // But here we rely on the library logic.
    Result res_tachyon = run_bench<tachyon::json>(data);

    std::cout << "\n=== HEAD-TO-HEAD BENCHMARK ===\n" << std::endl;
    std::cout << std::left << std::setw(15) << "Library"
              << std::setw(15) << "Throughput"
              << std::setw(15) << "Duration"
              << std::setw(15) << "Allocs"
              << std::setw(15) << "Cycles/Byte" << std::endl;
    std::cout << std::string(75, '-') << std::endl;

    std::cout << std::left << std::setw(15) << "Nlohmann"
              << res_nlohmann.throughput_mbs << " MB/s    "
              << res_nlohmann.duration_sec << " s       "
              << res_nlohmann.alloc_count << "          "
              << res_nlohmann.cycles_per_byte << std::endl;

    std::cout << std::left << std::setw(15) << "Tachyon"
              << res_tachyon.throughput_mbs << " MB/s    "
              << res_tachyon.duration_sec << " s       "
              << res_tachyon.alloc_count << "          "
              << res_tachyon.cycles_per_byte << std::endl;

    return 0;
}
