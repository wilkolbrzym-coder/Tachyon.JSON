#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstdlib>
#include <atomic>

#define TACHYON_SKIP_NLOHMANN_ALIAS
#include "tachyon.hpp"
#include "include_benchmark/nlohmann_json.hpp"

using namespace std;

// -----------------------------------------------------------------------------
// METRICS
// -----------------------------------------------------------------------------
std::atomic<size_t> g_allocs{0};
void* operator new(size_t size) {
    g_allocs++;
    return malloc(size);
}
void operator delete(void* ptr) noexcept { free(ptr); }
void operator delete(void* ptr, size_t) noexcept { free(ptr); }

uint64_t rdtsc() {
    unsigned int lo, hi;
#ifndef _MSC_VER
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
#else
    return 0; // Skip on MSVC
#endif
    return ((uint64_t)hi << 32) | lo;
}

// -----------------------------------------------------------------------------
// DATA GENERATION
// -----------------------------------------------------------------------------
string gen_small() {
    return R"({ "id": 12345, "name": "Tachyon", "active": true, "scores": [1, 2, 3] })";
}

string gen_canada() {
    stringstream ss;
    ss << "{ \"type\": \"FeatureCollection\", \"features\": [";
    for (int i = 0; i < 1000; ++i) {
        if (i > 0) ss << ",";
        ss << "{ \"type\": \"Feature\", \"geometry\": { \"type\": \"Polygon\", \"coordinates\": [[ ";
        for (int j = 0; j < 40; ++j) {
            if (j > 0) ss << ",";
            ss << "[" << (-100.0 + i*0.001 + j*0.001) << "," << (40.0 + j*0.002) << "]";
        }
        ss << " ]] }, \"properties\": { \"prop0\": \"value0\", \"prop1\": " << i << " } }";
    }
    ss << "] }";
    return ss.str();
}

string gen_large() {
    stringstream ss;
    ss << "[";
    for (int i = 0; i < 200000; ++i) {
        if (i > 0) ss << ",";
        ss << R"({"id":)" << i << R"(,"data":"some string data )" << i << R"(","val":)" << (i * 1.1) << "}";
    }
    ss << "]";
    return ss.str();
}

// -----------------------------------------------------------------------------
// BENCHMARK ENGINE
// -----------------------------------------------------------------------------
template<typename Func>
void run_phase(const string& phase, const string& dataset, const string& lib, double data_mb, Func f) {
    g_allocs = 0;
    auto start = chrono::high_resolution_clock::now();
    uint64_t c1 = rdtsc();
    f();
    uint64_t c2 = rdtsc();
    auto end = chrono::high_resolution_clock::now();

    double ms = chrono::duration_cast<chrono::microseconds>(end - start).count() / 1000.0;
    double mbs = data_mb / (ms / 1000.0);
    uint64_t cycles = c2 - c1;

    cout << left << setw(10) << dataset
         << setw(10) << lib
         << setw(10) << phase
         << setw(10) << ms << " ms"
         << setw(10) << mbs << " MB/s"
         << setw(10) << g_allocs << " allocs"
         << setw(15) << cycles << " cycles" << endl;
}

int main() {
    cout << "Standard: " << __cplusplus << endl;
    cout << "Generating Data..." << endl;
    string small = gen_small();
    string canada = gen_canada();
    string large = gen_large();

    double small_mb = small.size() / 1024.0 / 1024.0;
    double canada_mb = canada.size() / 1024.0 / 1024.0;
    double large_mb = large.size() / 1024.0 / 1024.0;
    
    cout << "Data Ready. Canada: " << canada_mb * 1024 << "KB, Large: " << large_mb << "MB\n";
    cout << "-----------------------------------------------------------------------------------" << endl;
    cout << left << setw(10) << "DATA" << setw(10) << "LIB" << setw(10) << "PHASE" << setw(10) << "TIME" << setw(10) << "SPEED" << setw(10) << "ALLOCS" << setw(15) << "CYCLES" << endl;
    cout << "-----------------------------------------------------------------------------------" << endl;

    // Small
    {
        run_phase("Parse", "Small", "Nlohmann", small_mb, [&](){ return nlohmann::json::parse(small); });
        auto j = nlohmann::json::parse(small);
        run_phase("Dump", "Small", "Nlohmann", small_mb, [&](){ return j.dump(); });

        run_phase("Parse", "Small", "Tachyon", small_mb, [&](){ return tachyon::json::parse(small); });
        auto jt = tachyon::json::parse(small);
        run_phase("Dump", "Small", "Tachyon", small_mb, [&](){ return jt.dump(); });
    }

    // Canada
    {
        run_phase("Parse", "Canada", "Nlohmann", canada_mb, [&](){ return nlohmann::json::parse(canada); });
        auto j = nlohmann::json::parse(canada);
        run_phase("Dump", "Canada", "Nlohmann", canada_mb, [&](){ return j.dump(); });

        run_phase("Parse", "Canada", "Tachyon", canada_mb, [&](){ return tachyon::json::parse(canada); });
        auto jt = tachyon::json::parse(canada);
        run_phase("Dump", "Canada", "Tachyon", canada_mb, [&](){ return jt.dump(); });
    }

    // Large
    {
        run_phase("Parse", "Large", "Nlohmann", large_mb, [&](){ return nlohmann::json::parse(large); });
        auto j = nlohmann::json::parse(large);
        run_phase("Dump", "Large", "Nlohmann", large_mb, [&](){ return j.dump(); });

        run_phase("Parse", "Large", "Tachyon", large_mb, [&](){ return tachyon::json::parse(large); });
        auto jt = tachyon::json::parse(large);
        run_phase("Dump", "Large", "Tachyon", large_mb, [&](){ return jt.dump(); });
    }

    return 0;
}
