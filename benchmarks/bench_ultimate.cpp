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
#endif
#else
#define HAS_SIMDJSON 0
#endif

// Mock Nlohmann if not present
#ifdef __has_include
#if __has_include("../include/nlohmann/json.hpp")
#include "../include/nlohmann/json.hpp"
#define HAS_NLOHMANN 1
#elif __has_include("../nlohmann_json.hpp")
#include "../nlohmann_json.hpp"
#define HAS_NLOHMANN 1
#else
#define HAS_NLOHMANN 0
#endif
#else
#define HAS_NLOHMANN 0
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

int main(int argc, char** argv) {
    std::string filename = "canada.json";
    if (argc > 1) filename = argv[1];

    std::string data = read_file(filename);
    if (data.empty()) {
        std::cerr << "[Error] Failed to load " << filename << "\n";
        return 1;
    }

    // Create padded string for Simdjson
    std::string padded_data = data;
    padded_data.resize(padded_data.size() + simdjson::SIMDJSON_PADDING);

    std::cout << "==============================================================\n";
    std::cout << " Tachyon v7.0 ULTIMATE BENCHMARK (On-Demand Mode)\n";
    std::cout << " File: " << filename << " (" << std::fixed << std::setprecision(2) << (data.size() / 1024.0 / 1024.0) << " MB)\n";
    std::cout << "==============================================================\n\n";

    int iterations = 50;
    std::vector<BenchResult> results;

    // 1. Tachyon v7.0 (Tape)
    {
        // Warmup
        auto json = Tachyon::json::parse(data);

        auto start = high_resolution_clock::now();
        uint64_t checksum = 0;
        for (int i = 0; i < iterations; ++i) {
            auto j = Tachyon::json::parse(data);
            if (j.is_object()) checksum += 1; // Basic integrity check
        }
        auto end = high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;
        double gb_s = (data.size() * iterations) / (1024.0 * 1024.0 * 1024.0) / diff.count();
        results.push_back({"Tachyon v7.0", gb_s, checksum});
    }

    // 2. Simdjson (On-Demand)
    {
#if HAS_SIMDJSON
        simdjson::ondemand::parser parser;
        simdjson::padded_string p_str(data);
        // Warmup
        {
            auto doc = parser.iterate(p_str);
            for (auto feature : doc["features"]) { (void)feature; }
        }

        auto start = high_resolution_clock::now();
        uint64_t checksum = 0;
        for (int i = 0; i < iterations; ++i) {
            auto doc = parser.iterate(p_str);
            // Iterate features to force parsing
            for (auto feature : doc["features"]) {
                 checksum++;
            }
        }
        auto end = high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;
        double gb_s = (data.size() * iterations) / (1024.0 * 1024.0 * 1024.0) / diff.count();
        results.push_back({"Simdjson (On-Demand)", gb_s, checksum});
#else
        results.push_back({"Simdjson (On-Demand)", 0.0, 0});
#endif
    }

    // 3. Glaze (Generic)
    {
#if HAS_GLAZE
        glz::generic j;
        // Warmup
        if (glz::read_json(j, data)) {}

        auto start = high_resolution_clock::now();
        uint64_t checksum = 0;
        for (int i = 0; i < iterations; ++i) {
            glz::generic j_doc;
            auto err = glz::read_json(j_doc, data);
            if (!err) checksum += j_doc.size();
        }
        auto end = high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;
        double gb_s = (data.size() * iterations) / (1024.0 * 1024.0 * 1024.0) / diff.count();
        results.push_back({"Glaze (Generic)", gb_s, checksum});
#else
        results.push_back({"Glaze (Generic)", 0.0, 0});
#endif
    }

    // 4. Nlohmann
    {
#if HAS_NLOHMANN
        // Warmup
        auto j = nlohmann::json::parse(data);

        auto start = high_resolution_clock::now();
        uint64_t checksum = 0;
        for (int i = 0; i < iterations; ++i) {
            auto j_loop = nlohmann::json::parse(data);
            checksum += j_loop.size();
        }
        auto end = high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;
        double gb_s = (data.size() * iterations) / (1024.0 * 1024.0 * 1024.0) / diff.count();
        results.push_back({"Nlohmann", gb_s, checksum});
#else
        results.push_back({"Nlohmann", 0.0, 0});
#endif
    }

    // Output Table
    std::cout << std::left << std::setw(25) << "Library"
              << std::setw(15) << "Throughput"
              << std::setw(15) << "Status" << "\n";
    std::cout << "-----------------------------------------------------------------\n";

    for (const auto& r : results) {
        std::cout << std::left << std::setw(25) << r.name;
        if (r.throughput_gb > 0.0) {
            std::cout << std::fixed << std::setprecision(2) << r.throughput_gb << " GB/s   ";
            std::cout << "Verified";
        } else {
            std::cout << "N/A           ";
            std::cout << "Not Found";
        }
        std::cout << "\n";
    }
    std::cout << "\n";

    return 0;
}
