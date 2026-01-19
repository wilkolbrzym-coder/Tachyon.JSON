#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstdlib>

// Force C++11 check
#if __cplusplus < 201103L
#error "This benchmark requires C++11 or later"
#endif

#define TACHYON_SKIP_NLOHMANN_ALIAS
#include "tachyon.hpp"
#include "include_benchmark/nlohmann_json.hpp"

using namespace std;

// -----------------------------------------------------------------------------
// METRICS
// -----------------------------------------------------------------------------
long long current_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

// -----------------------------------------------------------------------------
// DATA GENERATORS
// -----------------------------------------------------------------------------
std::string gen_canada() {
    stringstream ss;
    ss << "{ \"type\": \"FeatureCollection\", \"features\": [";
    for (int i = 0; i < 2000; ++i) {
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

// -----------------------------------------------------------------------------
// BENCHMARK
// -----------------------------------------------------------------------------
template <typename Func>
void run_test(const string& name, const string& dataset, Func f) {
    long long start = current_time_ms();
    f();
    long long end = current_time_ms();

    double sec = (end - start) / 1000.0;
    double mb = dataset.size() / 1024.0 / 1024.0;
    double speed = mb / sec;

    cout << left << setw(20) << name
         << setw(10) << (end - start) << " ms"
         << setw(10) << speed << " MB/s" << endl;
}

int main() {
    cout << "C++ Standard: " << __cplusplus << endl;
    cout << "Generating dataset..." << endl;
    string canada = gen_canada();
    cout << "Dataset Size: " << canada.size() / 1024.0 << " KB" << endl;
    cout << "------------------------------------------------" << endl;

    // Correctness check
    {
        auto j1 = tachyon::json::parse(canada);
        auto j2 = nlohmann::json::parse(canada);
        if (j1.size() != j2.size()) {
            cerr << "Correctness FAIL! Sizes differ." << endl;
            return 1;
        }
    }

    // Benchmark
    run_test("Nlohmann", canada, [&](){
        auto j = nlohmann::json::parse(canada);
        volatile size_t s = j.size(); (void)s;
    });

    run_test("Tachyon (Legacy)", canada, [&](){
        auto j = tachyon::json::parse(canada);
        volatile size_t s = j.size(); (void)s;
    });

    return 0;
}
