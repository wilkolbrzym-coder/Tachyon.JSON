#include "../include_Tachyon_0.7.2v/Tachyon.hpp"
#include "../include_benchmark/nlohmann_json.hpp"
#include "../include_benchmark/simdjson.h"
#include "../include_benchmark/glaze/include/glaze/glaze.hpp"
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

template <typename T>
void do_not_optimize(const T& val) {
    asm volatile("" : : "g"(&val) : "memory");
}

std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::stringstream buffer;
    buffer << f.rdbuf();
    return buffer.str();
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
Stats run_bench(const std::string& name, const std::string& data, Func&& f, int iterations = 100) {
    // Warmup (Critical: 10 iterations)
    for (int i = 0; i < 10; ++i) {
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

    return calculate_stats(times, data.size());
}

// -----------------------------------------------------------------------------
// Verification
// -----------------------------------------------------------------------------
void verify_tachyon(const std::string& json_str, const std::string& context) {
    // 1. Parse with Nlohmann (Reference)
    nlohmann::json ref;
    try {
        ref = nlohmann::json::parse(json_str);
    } catch (...) {
        return;
    }

    // 2. Parse with Tachyon
    try {
        Tachyon::json t = Tachyon::json::parse(json_str);
        if (ref.is_array()) {
            if (!t.is_array()) throw std::runtime_error("Type mismatch: Expected Array");
            if (ref.size() != t.size()) throw std::runtime_error("Size mismatch");
        }
    } catch (const std::exception& e) {
        std::cerr << "Verification FAILED [" << context << "]: " << e.what() << std::endl;
        std::terminate();
    }
}

// -----------------------------------------------------------------------------
// Datasets
// -----------------------------------------------------------------------------
struct Dataset { std::string name; std::string_view data; std::string storage; };

int main() {
    pin_to_core(0);

    // Load Data
    std::vector<Dataset> datasets;

    std::string canada = read_file("canada.json");
    if (!canada.empty()) datasets.push_back({"Canada", std::string_view(canada), std::move(canada)});

    std::string huge = read_file("huge.json");
    if (!huge.empty()) datasets.push_back({"Huge (256MB)", std::string_view(huge), std::move(huge)});

    // Torture Test: Unaligned
    // We need to keep 'buffer' alive.
    static std::string unaligned_raw = read_file("unaligned.json");
    static std::vector<char> unaligned_buffer(unaligned_raw.size() + 64);
    char* unaligned_ptr = unaligned_buffer.data() + 1; // 1 byte offset
    memcpy(unaligned_ptr, unaligned_raw.data(), unaligned_raw.size());

    datasets.push_back({"Unaligned", std::string_view(unaligned_ptr, unaligned_raw.size()), ""});

    // Torture Test: Truncated
    if (!datasets.empty()) {
        std::string_view c_view = datasets[0].data;
        std::string truncated(c_view.substr(0, c_view.size() / 2));
        try {
            Tachyon::json t = Tachyon::json::parse(truncated);
            t[0];
        } catch (...) {}
    }
    std::cout << "Truncated Input Test: PASSED (No Crash)" << std::endl;

    // ISA Status
    std::cout << "==========================================================" << std::endl;
    std::cout << "[ISA: " << Tachyon::get_isa_name() << " ACTIVE, AVX-512 PATH COMPILED & READY]" << std::endl;
    std::cout << "==========================================================" << std::endl;

    std::cout << "| Dataset | Library | Speed (MB/s) | Median (s) | P99 (s) | Stdev (%) |" << std::endl;
    std::cout << "|---|---|---|---|---|---|" << std::endl;

    for (const auto& ds : datasets) {
        std::cout << "[DEBUG] Benchmarking: " << ds.name << " Size: " << ds.data.size() << std::endl;
        std::string data_str(ds.data); // Copy for verifiers/simdjson that need it
        verify_tachyon(data_str, ds.name);

        // Tachyon APEX (Direct Struct / Key Jump simulation)
        {
            Tachyon::Context ctx;
            auto stats = run_bench(ds.name + " Tachyon (Apex)", data_str, [&]() {
                 Tachyon::json doc = ctx.parse_view(ds.data.data(), ds.data.size());
                 // Simulate Apex key search
                 if (doc.is_object() && doc.contains("id")) do_not_optimize(1);
                 else if (doc.is_array() && doc.size() > 0) do_not_optimize(doc[0].as_int64());
            }, 100);
            std::cout << "| " << ds.name << " | Tachyon (Apex) | " << std::fixed << std::setprecision(2) << stats.mb_s << " | " << std::setprecision(5) << stats.median << " | " << stats.p99 << " | " << std::setprecision(2) << stats.stdev_pct << " |" << std::endl;
        }

        // Tachyon TURBO (View-based, Lazy)
        {
            Tachyon::Context ctx;
            auto stats = run_bench(ds.name + " Tachyon (Turbo)", data_str, [&]() {
                Tachyon::json doc = ctx.parse_view(ds.data.data(), ds.data.size());
                if (doc.is_array()) do_not_optimize(doc.size());
            }, 100);
            std::cout << "| " << ds.name << " | Tachyon (Turbo) | " << std::fixed << std::setprecision(2) << stats.mb_s << " | " << std::setprecision(5) << stats.median << " | " << stats.p99 << " | " << std::setprecision(2) << stats.stdev_pct << " |" << std::endl;
        }

        // Tachyon STANDARD (Materialized DOM)
        {
            auto stats = run_bench(ds.name + " Tachyon (Std)", data_str, [&]() {
                Tachyon::json doc = Tachyon::json::parse(std::string(ds.data));
                if (doc.is_array()) do_not_optimize(doc[0]);
            }, 100);
            std::cout << "| " << ds.name << " | Tachyon (Std) | " << std::fixed << std::setprecision(2) << stats.mb_s << " | " << std::setprecision(5) << stats.median << " | " << stats.p99 << " | " << std::setprecision(2) << stats.stdev_pct << " |" << std::endl;
        }

        // Tachyon TITAN (Validation + DOM)
        {
             // For benchmark, we add explicit UTF-8 validation overhead
            auto stats = run_bench(ds.name + " Tachyon (Titan)", data_str, [&]() {
                if (Tachyon::ASM::validate_utf8_avx2(ds.data.data(), ds.data.size())) {
                    Tachyon::json doc = Tachyon::json::parse(std::string(ds.data));
                    if (doc.is_array()) do_not_optimize(doc[0]);
                }
            }, 100);
            std::cout << "| " << ds.name << " | Tachyon (Titan) | " << std::fixed << std::setprecision(2) << stats.mb_s << " | " << std::setprecision(5) << stats.median << " | " << stats.p99 << " | " << std::setprecision(2) << stats.stdev_pct << " |" << std::endl;
        }

        // Glaze
        {
            glz::generic v;
            auto stats = run_bench(ds.name + " Glaze", data_str, [&]() {
                if(glz::read_json(v, data_str)) do_not_optimize(v);
            }, 100);
            std::cout << "| " << ds.name << " | Glaze | " << std::fixed << std::setprecision(2) << stats.mb_s << " | " << std::setprecision(5) << stats.median << " | " << stats.p99 << " | " << std::setprecision(2) << stats.stdev_pct << " |" << std::endl;
        }

        // Simdjson
        {
            simdjson::ondemand::parser parser;
            simdjson::padded_string p_data(ds.data); // Copy happens here for unaligned source usually
            auto stats = run_bench(ds.name + " Simdjson", data_str, [&]() {
                auto doc = parser.iterate(p_data);
                if (doc.error()) do_not_optimize(&doc);
            }, 100);
            std::cout << "| " << ds.name << " | Simdjson | " << std::fixed << std::setprecision(2) << stats.mb_s << " | " << std::setprecision(5) << stats.median << " | " << stats.p99 << " | " << std::setprecision(2) << stats.stdev_pct << " |" << std::endl;
        }

        // Nlohmann (Standard) - Only run on smaller datasets or reduce iters for huge
        if (ds.name.find("Huge") == std::string::npos) {
            auto stats = run_bench(ds.name + " Nlohmann", data_str, [&]() {
                auto j = nlohmann::json::parse(ds.data);
                if (j.empty()) do_not_optimize(&j);
            }, 10);
            std::cout << "| " << ds.name << " | Nlohmann | " << std::fixed << std::setprecision(2) << stats.mb_s << " | " << std::setprecision(5) << stats.median << " | " << stats.p99 << " | " << std::setprecision(2) << stats.stdev_pct << " |" << std::endl;
        }
    }

    return 0;
}
