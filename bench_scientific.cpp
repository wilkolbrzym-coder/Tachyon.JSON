#include "Tachyon.hpp"
#include "nlohmann_json.hpp"
#include "simdjson.h"
#include "glaze/glaze.hpp"
#include <chrono>
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <sched.h>
#include <fstream>
#include <sstream>

// -----------------------------------------------------------------------------
// Utilities
// -----------------------------------------------------------------------------

void pin_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
}

void do_not_optimize(const void* p) {
    asm volatile("" : : "g"(p) : "memory");
}

std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::stringstream buffer;
    buffer << f.rdbuf();
    return buffer.str();
}

std::string generate_large_in_mem(size_t size_mb) {
    std::string s;
    s.reserve(size_mb * 1024 * 1024);
    s += "[";
    for (size_t i = 0; i < 500000; ++i) {
        if (i > 0) s += ",";
        s += R"({"id":)" + std::to_string(i) + R"(,"name":"Item )" + std::to_string(i) + R"(","active":true,"scores":[1,2,3,4,5]})";
        if (s.size() > size_mb * 1024 * 1024) break;
    }
    s += "]";
    return s;
}

std::string generate_nested_in_mem(int depth) {
    std::string s;
    s.reserve(depth * 10);
    for (int i = 0; i < depth; ++i) s += R"({"a":)";
    s += "1";
    for (int i = 0; i < depth; ++i) s += "}";
    return s;
}

struct Stats {
    double median;
    double p99;
    double stdev_pct;
    double mb_s;
};

Stats calculate_stats(std::vector<double>& times_sec, size_t bytes) {
    std::sort(times_sec.begin(), times_sec.end());
    size_t n = times_sec.size();
    double median = times_sec[n / 2];
    double p99 = times_sec[size_t(n * 0.99)];

    double sum = std::accumulate(times_sec.begin(), times_sec.end(), 0.0);
    double mean = sum / n;
    double sq_sum = 0.0;
    for (double t : times_sec) sq_sum += (t - mean) * (t - mean);
    double stdev = std::sqrt(sq_sum / n);

    return {median, p99, (stdev / mean) * 100.0, (bytes / 1024.0 / 1024.0) / median};
}

// -----------------------------------------------------------------------------
// Benchmarks
// -----------------------------------------------------------------------------

template <typename Func>
Stats run_bench(const std::string& name, const std::string& data, Func&& f, int iterations = 1000) {
    // Run 3 times, report MAX speed (Minimum Median Time)
    Stats best_stats = {0, 0, 0, 0};

    for (int run = 0; run < 3; ++run) {
        // Warmup (Adaptive)
        int warmup = iterations / 2;
        for (int i = 0; i < warmup; ++i) {
            f();
        }

        // Measure
        std::vector<double> times;
        times.reserve(iterations);
        for (int i = 0; i < iterations; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            f();
            auto end = std::chrono::high_resolution_clock::now();
            times.push_back(std::chrono::duration<double>(end - start).count());
        }

        Stats current = calculate_stats(times, data.size());
        if (current.mb_s > best_stats.mb_s) {
            best_stats = current;
        }
    }
    return best_stats;
}

int main() {
    pin_to_core(0);

    std::cout << "Generating/Loading datasets..." << std::endl;
    std::string large = generate_large_in_mem(25);
    std::string nested = generate_nested_in_mem(1000);
    std::string canada = read_file("canada.json");

    if (canada.empty()) {
        std::cerr << "Warning: canada.json not found. Run 'make deps' or download it." << std::endl;
    }

    struct Dataset { std::string name; const std::string& data; };
    std::vector<Dataset> datasets;
    datasets.push_back({"Large Array", large});
    if (!canada.empty()) datasets.push_back({"Canada", canada});
    datasets.push_back({"Nested", nested});

    std::cout << "| Dataset | Library | Speed (MB/s) | Median (s) | P99 (s) | Stdev (%) |" << std::endl;
    std::cout << "|---|---|---|---|---|---|" << std::endl;

    for (const auto& ds : datasets) {
        // Tachyon
        {
            Tachyon::Document doc;
            doc.parse_view(ds.data.data(), ds.data.size());
            auto stats = run_bench(ds.name + " Tachyon", ds.data, [&]() {
                doc.parse_view(ds.data.data(), ds.data.size());
                do_not_optimize(doc.bitmask_ptr);
            }, 1000);
            std::cout << "| " << ds.name << " | Tachyon | " << std::fixed << std::setprecision(2) << stats.mb_s << " | " << std::setprecision(5) << stats.median << " | " << stats.p99 << " | " << std::setprecision(2) << stats.stdev_pct << " |" << std::endl;
        }

        // Glaze (Generic)
        {
            glz::generic v;
            auto stats = run_bench(ds.name + " Glaze", ds.data, [&]() {
                if(glz::read_json(v, ds.data)) do_not_optimize(&v);
            }, 100); // Reduced iterations
            std::cout << "| " << ds.name << " | Glaze | " << std::fixed << std::setprecision(2) << stats.mb_s << " | " << std::setprecision(5) << stats.median << " | " << stats.p99 << " | " << std::setprecision(2) << stats.stdev_pct << " |" << std::endl;
        }

        // Simdjson
        {
            simdjson::ondemand::parser parser;
            simdjson::padded_string p_data(ds.data);
            auto stats = run_bench(ds.name + " Simdjson", ds.data, [&]() {
                auto doc = parser.iterate(p_data);
                if (doc.error()) do_not_optimize(&doc);
            }, 1000);
            std::cout << "| " << ds.name << " | Simdjson | " << std::fixed << std::setprecision(2) << stats.mb_s << " | " << std::setprecision(5) << stats.median << " | " << stats.p99 << " | " << std::setprecision(2) << stats.stdev_pct << " |" << std::endl;
        }

        // Nlohmann
        {
            auto stats = run_bench(ds.name + " Nlohmann", ds.data, [&]() {
                auto j = nlohmann::json::parse(ds.data);
                if (j.empty()) do_not_optimize(&j);
            }, 10); // Very low iterations
            std::cout << "| " << ds.name << " | Nlohmann | " << std::fixed << std::setprecision(2) << stats.mb_s << " | " << std::setprecision(5) << stats.median << " | " << stats.p99 << " | " << std::setprecision(2) << stats.stdev_pct << " |" << std::endl;
        }
    }

    return 0;
}
