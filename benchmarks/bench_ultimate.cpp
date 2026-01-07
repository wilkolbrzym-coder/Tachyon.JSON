#include "../include/Tachyon.hpp"
#include <iostream>
#include <chrono>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>

// Mock Simdjson if not present
#ifdef __has_include
#if __has_include("../include/simdjson.h")
#include "../include/simdjson.h"
#define HAS_SIMDJSON 1
#else
#define HAS_SIMDJSON 0
namespace simdjson { class dom { public: class parser { public: struct res { int error() { return 0; } }; res parse(const std::string&) { return {}; } }; }; }
#endif
#else
#define HAS_SIMDJSON 0
namespace simdjson { class dom { public: class parser { public: struct res { int error() { return 0; } }; res parse(const std::string&) { return {}; } }; }; }
#endif

// Glaze
#ifdef __has_include
#if __has_include("../glaze/include/glaze/glaze.hpp")
#include "../glaze/include/glaze/glaze.hpp"
#define HAS_GLAZE 1
#else
#define HAS_GLAZE 0
#endif
#else
#define HAS_GLAZE 0
#endif

using namespace std::chrono;

std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::stringstream buffer;
    buffer << f.rdbuf();
    return buffer.str();
}

struct BenchResult {
    std::string name;
    double throughput_gb;
    uint64_t checksum;
};

void run_test(const std::string& filename) {
    std::string data = read_file(filename);
    if (data.empty()) {
        std::cerr << "[Error] Failed to load " << filename << "\n";
        return;
    }
    std::string padded_data = data;
#if HAS_SIMDJSON
    padded_data.resize(padded_data.size() + simdjson::SIMDJSON_PADDING);
#else
    padded_data.resize(padded_data.size() + 1024);
#endif

    std::cout << "-----------------------------------------------------------------\n";
    std::cout << " File: " << filename << " (" << std::fixed << std::setprecision(2) << (data.size() / 1024.0 / 1024.0) << " MB)\n";
    std::cout << "-----------------------------------------------------------------\n";

    int iterations = 20; // 50MB file needs fewer iters to be fast
    std::vector<BenchResult> results;

    // 1. Tachyon v7.2
    {
        // Warmup
        auto json = Tachyon::json::parse(data);

        auto start = high_resolution_clock::now();
        uint64_t checksum = 0;
        for (int i = 0; i < iterations; ++i) {
            auto j = Tachyon::json::parse(data);
            if (j.is_array() || j.is_object()) checksum += 1;
        }
        auto end = high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;
        double gb_s = (data.size() * iterations) / (1024.0 * 1024.0 * 1024.0) / diff.count();
        results.push_back({"Tachyon v7.2", gb_s, checksum});
    }

    // 2. Simdjson (On-Demand)
    {
#if HAS_SIMDJSON
        simdjson::ondemand::parser parser;
        // Warmup
        {
            auto doc = parser.iterate(padded_data);
            for (auto x : doc) {}
        }

        auto start = high_resolution_clock::now();
        uint64_t checksum = 0;
        for (int i = 0; i < iterations; ++i) {
            auto doc = parser.iterate(padded_data);
            // Must iterate to parse
            for (auto x : doc) { checksum++; }
        }
        auto end = high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;
        double gb_s = (data.size() * iterations) / (1024.0 * 1024.0 * 1024.0) / diff.count();
        results.push_back({"Simdjson (OD)", gb_s, checksum});
#else
        results.push_back({"Simdjson (OD)", 0.0, 0});
#endif
    }

    // 3. Glaze
    {
#if HAS_GLAZE
        glz::generic j;
        if (glz::read_json(j, data)) {}

        auto start = high_resolution_clock::now();
        uint64_t checksum = 0;
        for (int i = 0; i < iterations; ++i) {
            glz::generic j_doc;
            auto err = glz::read_json(j_doc, data);
            if (!err) checksum += 1;
        }
        auto end = high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;
        double gb_s = (data.size() * iterations) / (1024.0 * 1024.0 * 1024.0) / diff.count();
        results.push_back({"Glaze", gb_s, checksum});
#else
        results.push_back({"Glaze", 0.0, 0});
#endif
    }

    for (const auto& r : results) {
        std::cout << std::left << std::setw(25) << r.name;
        if (r.throughput_gb > 0.0) {
            std::cout << std::fixed << std::setprecision(2) << r.throughput_gb << " GB/s   ";
        } else {
            std::cout << "N/A           ";
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}

int main(int argc, char** argv) {
    std::cout << "==============================================================\n";
    std::cout << " Tachyon v7.2 ULTIMATE BENCHMARK\n";
    std::cout << "==============================================================\n\n";

    run_test("canada.json");
    if (std::ifstream("large_file.json").good()) {
        run_test("large_file.json");
    }

    return 0;
}
