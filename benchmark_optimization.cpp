#include "Tachyon.hpp"
#include "simdjson.h"
#include <chrono>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <iomanip>
#include <algorithm>

using namespace std;

string read_file(const string& path) {
    ifstream f(path, ios::binary | ios::ate);
    if (!f) return "";
    auto size = f.tellg();
    f.seekg(0);
    string s;
    s.resize(size);
    f.read(&s[0], size);
    s.append(simdjson::SIMDJSON_PADDING, ' ');
    return s;
}

int main() {
    string small_str = read_file("small.json");
    string large_str = read_file("canada_large.json");

    if (small_str.empty() || large_str.empty()) {
        cerr << "Files not found!" << endl;
        return 1;
    }

    size_t small_size = small_str.size() - simdjson::SIMDJSON_PADDING;
    size_t large_size = large_str.size() - simdjson::SIMDJSON_PADDING;

    cout << "Benchmarking..." << endl;

    // 1. Small File Latency
    {
        cout << "\n--- Small File (600B) Latency ---" << endl;
        int iters = 1000;

        // Simdjson
        simdjson::ondemand::parser parser;
        auto start = chrono::high_resolution_clock::now();
        for(int i=0; i<iters; ++i) {
            simdjson::padded_string_view psv(small_str.data(), small_size, small_str.capacity());
            auto doc = parser.iterate(psv);
            std::string_view s;
            doc["name"].get(s);
            if (i==0 && s != "Small File Test") cerr << "Simdjson mismatch" << endl;
        }
        auto end = chrono::high_resolution_clock::now();
        double d_simd = chrono::duration<double>(end - start).count();
        cout << "Simdjson: " << (d_simd/iters)*1e9 << " ns/op" << endl;

        // Tachyon
        Tachyon::Context ctx;
        start = chrono::high_resolution_clock::now();
        for(int i=0; i<iters; ++i) {
            auto doc = ctx.parse_view(small_str.data(), small_size);
            string s = doc["name"].as_string();
            if (i==0 && s != "Small File Test") cerr << "Tachyon mismatch" << endl;
        }
        end = chrono::high_resolution_clock::now();
        double d_tach = chrono::duration<double>(end - start).count();
        cout << "Tachyon:  " << (d_tach/iters)*1e9 << " ns/op" << endl;
    }

    // 2. Large File Throughput
    {
        cout << "\n--- Large File (256MB) Throughput ---" << endl;
        int iters = 5;

        // Simdjson
        simdjson::ondemand::parser parser;

        // Warmup
        {
             simdjson::padded_string_view psv(large_str.data(), large_size, large_str.capacity());
             auto doc = parser.iterate(psv);
             for(auto feat : doc["features"]) { feat["geometry"]["type"]; }
        }

        auto start = chrono::high_resolution_clock::now();
        for(int i=0; i<iters; ++i) {
            simdjson::padded_string_view psv(large_str.data(), large_size, large_str.capacity());
            auto doc = parser.iterate(psv);
            for(auto feat : doc["features"]) {
                feat["geometry"]["type"];
            }
        }
        auto end = chrono::high_resolution_clock::now();
        double d_simd = chrono::duration<double>(end - start).count();
        double mb = (large_size * iters) / 1024.0 / 1024.0;
        cout << "Simdjson: " << mb / d_simd << " MB/s" << endl;

        // Tachyon
        Tachyon::Context ctx;

        // Warmup
        {
            auto doc = ctx.parse_view(large_str.data(), large_size);
            auto arr = doc["features"];
            size_t sz = arr.size();
            for(size_t k=0; k<sz; ++k) { arr[k]["geometry"]["type"].as_string(); }
        }

        start = chrono::high_resolution_clock::now();
        for(int i=0; i<iters; ++i) {
            auto doc = ctx.parse_view(large_str.data(), large_size);
            auto arr = doc["features"];
            size_t sz = arr.size();
            for(size_t k=0; k<sz; ++k) {
                arr[k]["geometry"]["type"].as_string();
            }
        }
        end = chrono::high_resolution_clock::now();
        double d_tach = chrono::duration<double>(end - start).count();
        cout << "Tachyon:  " << mb / d_tach << " MB/s" << endl;
    }

    return 0;
}
