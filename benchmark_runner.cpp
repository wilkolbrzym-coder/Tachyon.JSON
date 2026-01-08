#include "Tachyon.hpp"
#include "simdjson.h"
#include <glaze/glaze.hpp>
#include <chrono>
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <numeric>
#include <algorithm>
#include <sched.h>
#include <fstream>
#include <cstring>

// Zapobiega "wycinaniu" kodu
template <typename T>
void do_not_optimize(const T& val) {
    asm volatile("" : : "g"(&val) : "memory");
}

void pin_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
}

std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return "";
    auto size = f.tellg();
    f.seekg(0);
    std::string s;
    s.resize(size);
    f.read(&s[0], size);
    s.append(128, ' '); 
    return s;
}

struct Stats { 
    double mb_s; 
    double median_time; 
};

Stats calculate_stats(std::vector<double>& times, size_t bytes) {
    std::sort(times.begin(), times.end());
    double median = times[times.size() / 2];
    double mb_s = (bytes / 1024.0 / 1024.0) / median;
    return { mb_s, median };
}

int main() {
    pin_to_core(0);
    
    std::string canada_data = read_file("canada.json");
    std::string huge_data = read_file("huge.json");

    if (canada_data.empty() || huge_data.empty()) {
        std::cerr << "BŁĄD: Pliki JSON nie zostały znalezione!" << std::endl;
        return 1;
    }

    struct Job { std::string name; const char* ptr; size_t size; };
    std::vector<Job> jobs = {
        {"Canada", canada_data.data(), canada_data.size() - 128},
        {"Huge (256MB)", huge_data.data(), huge_data.size() - 128}
    };

    std::cout << "==========================================================" << std::endl;
    std::cout << "[PROTOKÓŁ: ZERO BIAS - ULTRA PRECISION TEST]" << std::endl;
    std::cout << "[ISA: " << Tachyon::get_isa_name() << " | ITERS: 50 | WARMUP: 20]" << std::endl;
    std::cout << "==========================================================" << std::endl;
    std::cout << std::fixed << std::setprecision(12);

    for (const auto& job : jobs) {
        const int iters = 50;
        const int warmup = 20;

        std::cout << "\n>>> Dataset: " << job.name << " (" << job.size << " bytes)" << std::endl;
        std::cout << "| Library | Speed (MB/s) | Median Time (s) |" << std::endl;
        std::cout << "|---|---|---|" << std::endl;

        // --- 1. SIMDJSON (IDZIE PIERWSZY) ---
        {
            simdjson::ondemand::parser parser;
            simdjson::padded_string_view p_view(job.ptr, job.size, job.size + 64);
            std::vector<double> times;
            
            // Rozgrzewka Cache
            for(int i = 0; i < warmup; ++i) {
                auto doc = parser.iterate(p_view);
                if (job.name.find("Huge") != std::string::npos) {
                    for (auto val : doc.get_array()) { do_not_optimize(val); }
                } else { do_not_optimize(doc["type"]); }
            }

            // Pomiar
            for (int i = 0; i < iters; ++i) {
                auto start = std::chrono::high_resolution_clock::now();
                auto doc = parser.iterate(p_view);
                if (job.name.find("Huge") != std::string::npos) {
                    for (auto val : doc.get_array()) { do_not_optimize(val); }
                } else { do_not_optimize(doc["type"]); }
                auto end = std::chrono::high_resolution_clock::now();
                times.push_back(std::chrono::duration<double>(end - start).count());
            }
            auto s = calculate_stats(times, job.size);
            std::cout << "| Simdjson (Fair) | " << std::setw(12) << std::setprecision(2) << s.mb_s 
                      << " | " << std::setprecision(12) << s.median_time << " |" << std::endl;
        }

        // --- 2. TACHYON (IDZIE DRUGI) ---
        {
            Tachyon::Context ctx;
            std::vector<double> times;

            // Rozgrzewka Cache
            for(int i = 0; i < warmup; ++i) {
                Tachyon::json doc = ctx.parse_view(job.ptr, job.size);
                if (doc.is_array()) do_not_optimize(doc.size());
                else do_not_optimize(doc.contains("type"));
            }

            // Pomiar
            for (int i = 0; i < iters; ++i) {
                auto start = std::chrono::high_resolution_clock::now();
                Tachyon::json doc = ctx.parse_view(job.ptr, job.size);
                if (doc.is_array()) do_not_optimize(doc.size());
                else do_not_optimize(doc.contains("type"));
                auto end = std::chrono::high_resolution_clock::now();
                times.push_back(std::chrono::duration<double>(end - start).count());
            }
            auto s = calculate_stats(times, job.size);
            std::cout << "| Tachyon (Turbo) | " << std::setw(12) << std::setprecision(2) << s.mb_s 
                      << " | " << std::setprecision(12) << s.median_time << " |" << std::endl;
        }

        // --- 3. GLAZE ---
        {
            std::vector<double> times;
            glz::generic v;
            
            // Rozgrzewka
            for(int i = 0; i < warmup; ++i) {
                std::string_view sv(job.ptr, job.size);
                glz::read_json(v, sv);
            }

            // Pomiar
            for (int i = 0; i < iters; ++i) {
                auto start = std::chrono::high_resolution_clock::now();
                std::string_view sv(job.ptr, job.size);
                glz::read_json(v, sv);
                auto end = std::chrono::high_resolution_clock::now();
                times.push_back(std::chrono::duration<double>(end - start).count());
            }
            auto s = calculate_stats(times, job.size);
            std::cout << "| Glaze (Reuse)   | " << std::setprecision(2) << s.mb_s 
                      << " | " << std::setprecision(12) << s.median_time << " |" << std::endl;
        }
    }
    return 0;
}