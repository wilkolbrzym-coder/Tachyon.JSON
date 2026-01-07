#include "../include/Tachyon.hpp"
#include <iostream>
#include <chrono>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>

// Mock Simdjson if not present
#ifdef __has_include
#if __has_include("../include/simdjson.h")
#include "../include/simdjson.h"
#else
namespace simdjson { class dom { public: class parser { public: struct res { int error() { return 0; } }; res parse(const std::string&) { return {}; } }; }; }
#endif
#else
namespace simdjson { class dom { public: class parser { public: struct res { int error() { return 0; } }; res parse(const std::string&) { return {}; } }; }; }
#endif

// Mock Nlohmann if not present
#ifdef __has_include
#if __has_include("nlohmann/json.hpp")
#include "nlohmann/json.hpp"
#else
namespace nlohmann { class json { public: static int parse(const std::string&) { return 0; } }; }
#endif
#else
namespace nlohmann { class json { public: static int parse(const std::string&) { return 0; } }; }
#endif

using namespace std::chrono;

std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::stringstream buffer;
    buffer << f.rdbuf();
    return buffer.str();
}

int main(int argc, char** argv) {
    std::string filename = "canada.json";
    if (argc > 1) filename = argv[1];

    std::string data = read_file(filename);
    if (data.empty()) {
        std::cerr << "Failed to load " << filename << "\n";
        return 1;
    }

    std::cout << "Benchmarking " << filename << " (" << data.size() / 1024.0 / 1024.0 << " MB)\n";

    int iterations = 100;

    // Tachyon Benchmark
    {
        Tachyon::Document doc; // Re-use buffer to measure throughput
        // Warmup
        doc.parse(data);

        auto start = high_resolution_clock::now();
        uint64_t checksum = 0;
        for (int i = 0; i < iterations; ++i) {
            doc.parse(data);
            // Verify results with Checksum to prevent DCE
            // Fast loop over tape
            uint64_t* t = doc.tape;
            size_t n = doc.tape_len;
            for(size_t j=0; j<n; ++j) checksum += t[j];
        }
        auto end = high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;
        double duration = diff.count();
        double gb_s = (data.size() * iterations) / (1024.0 * 1024.0 * 1024.0) / duration;

        std::cout << "Tachyon v7.0: " << std::fixed << std::setprecision(2) << gb_s << " GB/s (Checksum: " << checksum << ")\n";
    }

    // Simdjson Benchmark
#if defined(SIMDJSON_H) || defined(SIMDJSON_HPP)
    {
        simdjson::dom::parser parser;
        // Warmup
        auto result = parser.parse(data);

        auto start = high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            auto error = parser.parse(data).error();
            if (error) { std::cerr << "Simdjson error\n"; break; }
        }
        auto end = high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;
        double duration = diff.count();
        double gb_s = (data.size() * iterations) / (1024.0 * 1024.0 * 1024.0) / duration;
        std::cout << "Simdjson:     " << gb_s << " GB/s\n";
    }
#endif

    return 0;
}
