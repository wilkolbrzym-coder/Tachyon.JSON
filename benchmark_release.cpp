#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <atomic>
#include <iomanip>
#include <random>
#include <sstream>
#include <filesystem>
#include <fstream>

#include "tachyon.hpp"
#include "include_benchmark/nlohmann_json.hpp"

using namespace std;

// -----------------------------------------------------------------------------
// METRICS TRACKER
// -----------------------------------------------------------------------------
std::atomic<size_t> g_allocs{0};
void* operator new(size_t size) {
    g_allocs++;
    return malloc(size);
}
void operator delete(void* ptr) noexcept { free(ptr); }
void operator delete(void* ptr, size_t) noexcept { free(ptr); }

// -----------------------------------------------------------------------------
// DATA GENERATORS
// -----------------------------------------------------------------------------
std::string gen_small() {
    return R"({
        "project": "tachyon",
        "version": 8.0,
        "fast": true,
        "ids": [1, 2, 3, 4, 5],
        "meta": { "author": "unknown", "license": "MIT" }
    })";
}

std::string gen_canada() {
    stringstream ss;
    ss << "{ \"type\": \"FeatureCollection\", \"features\": [";
    for (int i = 0; i < 2000; ++i) {
        if (i > 0) ss << ",";
        ss << R"({ "type": "Feature", "geometry": { "type": "Polygon", "coordinates": [[ )";
        for (int j = 0; j < 40; ++j) { // 40 points
            if (j > 0) ss << ",";
            ss << "[" << (-100.0 + i*0.001 + j*0.001) << "," << (40.0 + j*0.002) << "]";
        }
        ss << " ]] }, \"properties\": { \"prop0\": \"value0\", \"prop1\": " << i << " } }";
    }
    ss << "] }";
    return ss.str();
}

std::string gen_large() {
    // 50MB of repetitive data
    stringstream ss;
    ss << "[";
    for (int i = 0; i < 500000; ++i) {
        if (i > 0) ss << ",";
        ss << R"({"id":)" << i << R"(,"name":"obj_)" << i << R"(","val":)" << (i * 0.5) << "}";
    }
    ss << "]";
    return ss.str();
}

// -----------------------------------------------------------------------------
// BENCHMARK ENGINE
// -----------------------------------------------------------------------------
template <typename Func>
void run_test(const string& name, const string& dataset, Func f) {
    // Warmup
    f();

    g_allocs = 0;
    auto start = chrono::high_resolution_clock::now();
    f();
    auto end = chrono::high_resolution_clock::now();

    double ms = chrono::duration_cast<chrono::microseconds>(end - start).count() / 1000.0;
    double mbs = (dataset.size() / 1024.0 / 1024.0) / (ms / 1000.0);

    cout << left << setw(20) << name
         << setw(15) << ms << " ms"
         << setw(15) << mbs << " MB/s"
         << setw(15) << g_allocs << " allocs" << endl;
}

int main() {
    cout << "Generating datasets..." << endl;
    string small = gen_small();
    string canada = gen_canada();
    string large = gen_large();

    cout << "Small size:  " << small.size() << " bytes" << endl;
    cout << "Canada size: " << canada.size() / 1024.0 << " KB" << endl;
    cout << "Large size:  " << large.size() / 1024.0 / 1024.0 << " MB" << endl;
    cout << "----------------------------------------------------------------" << endl;
    cout << left << setw(20) << "TEST" << setw(15) << "TIME" << setw(15) << "THROUGHPUT" << setw(15) << "ALLOCS" << endl;
    cout << "----------------------------------------------------------------" << endl;

    // SMALL
    run_test("Nlohmann (Small)", small, [&](){
        auto j = nlohmann::json::parse(small);
        volatile size_t s = j.size(); (void)s;
    });
    run_test("Tachyon (Small)", small, [&](){
        auto j = tachyon::json::parse(small);
        volatile size_t s = j.size(); (void)s;
    });

    // CANADA
    run_test("Nlohmann (Canada)", canada, [&](){
        auto j = nlohmann::json::parse(canada);
        volatile size_t s = j["features"].size(); (void)s;
    });
    run_test("Tachyon (Canada)", canada, [&](){
        auto j = tachyon::json::parse(canada);
        volatile size_t s = j["features"].size(); (void)s;
    });

    // LARGE
    run_test("Nlohmann (Large)", large, [&](){
        auto j = nlohmann::json::parse(large);
        volatile size_t s = j.size(); (void)s;
    });
    run_test("Tachyon (Large)", large, [&](){
        auto j = tachyon::json::parse(large);
        volatile size_t s = j.size(); (void)s;
    });

    return 0;
}
