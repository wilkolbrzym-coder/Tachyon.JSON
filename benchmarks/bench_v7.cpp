#include "Tachyon.hpp"
#include <iostream>
#include <chrono>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>

// Mock Simdjson if not present
#ifdef __has_include
#if __has_include("simdjson.h")
#include "simdjson.h"
#else
namespace simdjson { class dom { public: class parser { public: int parse(const std::string&, const char*) { return 0; } }; }; }
#endif
#else
namespace simdjson { class dom { public: class parser { public: int parse(const std::string&, const char*) { return 0; } }; }; }
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

    // Warmup
    Tachyon::Document doc;
    doc.parse(data);

    // Tachyon Benchmark
    auto start = high_resolution_clock::now();
    int iterations = 100;
    uint64_t checksum = 0;
    for (int i = 0; i < iterations; ++i) {
        doc.parse(data);
        // Verify results with Checksum to prevent DCE
        for(size_t j=0; j<doc.tape_len; ++j) checksum += doc.tape[j];
    }
    auto end = high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    double duration = diff.count();
    double gb_s = (data.size() * iterations) / (1024.0 * 1024.0 * 1024.0) / duration;

    std::cout << "Tachyon v7.0: " << std::fixed << std::setprecision(2) << gb_s << " GB/s (Checksum: " << checksum << ")\n";

    // Simdjson Benchmark
#if defined(SIMDJSON_H) || defined(SIMDJSON_HPP)
    simdjson::dom::parser parser;
    start = high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        auto error = parser.parse(data).error();
        (void)error;
    }
    end = high_resolution_clock::now();
    duration = duration_cast<duration<double>>(end - start).count();
    gb_s = (data.size() * iterations) / (1024.0 * 1024.0 * 1024.0) / duration;
    std::cout << "Simdjson:     " << gb_s << " GB/s\n";
#endif

    return 0;
}
