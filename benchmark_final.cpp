#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <random>
#include <atomic>

#include "tachyon.hpp"
#include "include_benchmark/nlohmann_json.hpp"

// ALLOCATION TRACKER
std::atomic<size_t> g_alloc_count{0};
void* operator new(size_t size) {
    g_alloc_count++;
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

// DATA GENERATORS
void generate_canada(const std::string& filename) {
    std::ofstream out(filename);
    out << "{ \"type\": \"FeatureCollection\", \"features\": [";
    for (int i = 0; i < 5000; ++i) { // Reduced count for reasonable benchmark time but still large (~20MB)
        if (i > 0) out << ",";
        out << R"({ "type": "Feature", "properties": { "name": "Canada Region )" << i << R"(" }, "geometry": { "type": "Polygon", "coordinates": [[ )";
        for (int j = 0; j < 20; ++j) {
            if (j > 0) out << ",";
            out << "[" << (double)i/100.0 << "," << (double)j/100.0 << "]";
        }
        out << "]] } }";
    }
    out << "] }";
}

void generate_unicode(const std::string& filename) {
    std::ofstream out(filename);
    out << "[";
    for (int i = 0; i < 10000; ++i) {
        if (i > 0) out << ",";
        out << "\"English\", \"Русский text\", \"中文 characters\", \"Emoji 🚀 check\", \"Math ∀x∈R\"";
    }
    out << "]";
}

// BENCHMARK RUNNER
struct Result {
    double speed_mbs;
    double cycles_per_byte;
    size_t allocs;
};

template<typename Func>
Result run_bench(const std::string& data, Func f) {
    // Warmup
    f();

    g_alloc_count = 0;
    auto start = std::chrono::high_resolution_clock::now();
    uint64_t start_cycles = rdtsc();

    f();

    uint64_t end_cycles = rdtsc();
    auto end = std::chrono::high_resolution_clock::now();

    size_t allocs = g_alloc_count;
    std::chrono::duration<double> sec = end - start;

    double mbs = (data.size() / (1024.0 * 1024.0)) / sec.count();
    double cpb = (double)(end_cycles - start_cycles) / data.size();

    return {mbs, cpb, allocs};
}

int main() {
    std::cout << "Generating Data..." << std::endl;
    generate_canada("canada.json");
    generate_unicode("unicode.json");

    std::string canada_str, unicode_str;
    { std::ifstream t("canada.json"); std::stringstream buffer; buffer << t.rdbuf(); canada_str = buffer.str(); }
    { std::ifstream t("unicode.json"); std::stringstream buffer; buffer << t.rdbuf(); unicode_str = buffer.str(); }

    std::cout << "Canada Size: " << canada_str.size() / 1024.0 << " KB" << std::endl;
    std::cout << "Unicode Size: " << unicode_str.size() / 1024.0 << " KB" << std::endl;

    std::cout << "\n=== BENCHMARK: EAGER PARSE (FAIR FIGHT) ===\n" << std::endl;
    std::cout << std::left << std::setw(15) << "Dataset"
              << std::setw(15) << "Library"
              << std::setw(15) << "Speed (MB/s)"
              << std::setw(15) << "Cycles/Byte"
              << std::setw(15) << "Allocs" << std::endl;
    std::cout << std::string(75, '-') << std::endl;

    auto print_row = [](const std::string& d, const std::string& l, const Result& r) {
        std::cout << std::left << std::setw(15) << d
                  << std::setw(15) << l
                  << std::setw(15) << r.speed_mbs
                  << std::setw(15) << r.cycles_per_byte
                  << std::setw(15) << r.allocs << std::endl;
    };

    // Canada Nlohmann
    print_row("canada.json", "Nlohmann", run_bench(canada_str, [&](){
        auto j = nlohmann::json::parse(canada_str);
        (void)j.size();
    }));

    // Canada Tachyon
    print_row("canada.json", "Tachyon", run_bench(canada_str, [&](){
        auto j = tachyon::json::parse(canada_str);
        (void)j.size();
    }));

    // Unicode Nlohmann
    print_row("unicode.json", "Nlohmann", run_bench(unicode_str, [&](){
        auto j = nlohmann::json::parse(unicode_str);
        (void)j.size();
    }));

    // Unicode Tachyon
    print_row("unicode.json", "Tachyon", run_bench(unicode_str, [&](){
        auto j = tachyon::json::parse(unicode_str);
        (void)j.size();
    }));

    return 0;
}
