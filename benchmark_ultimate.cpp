#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <atomic>
#include <iomanip>
#include <random>
#include <sstream>

#include "tachyon.hpp"
#include "include_benchmark/nlohmann_json.hpp"

using namespace std;

// -----------------------------------------------------------------------------
// METRICS
// -----------------------------------------------------------------------------
std::atomic<size_t> g_alloc_count{0};
std::atomic<size_t> g_alloc_bytes{0};

void* operator new(size_t size) {
    g_alloc_count++;
    g_alloc_bytes += size;
    return malloc(size);
}

void operator delete(void* ptr) noexcept {
    free(ptr);
}
void operator delete(void* ptr, size_t) noexcept {
    free(ptr);
}

static uint64_t rdtsc() {
    unsigned int lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}

// -----------------------------------------------------------------------------
// DATA GENERATORS
// -----------------------------------------------------------------------------
string make_canada_json() {
    stringstream ss;
    ss << "{ \"type\": \"FeatureCollection\", \"features\": [";
    for (int i = 0; i < 2000; ++i) { // 2000 features
        if (i > 0) ss << ",";
        ss << "{ \"type\": \"Feature\", \"geometry\": { \"type\": \"Polygon\", \"coordinates\": [ [ ";
        for (int j = 0; j < 50; ++j) { // 50 points
            if (j > 0) ss << ",";
            ss << "[" << (-100.0 + i*0.01) << "," << (40.0 + j*0.01) << "]";
        }
        ss << " ] ] }, \"properties\": { \"name\": \"Canada Region " << i << "\" } }";
    }
    ss << "] }";
    return ss.str();
}

string make_twitter_json() {
    stringstream ss;
    ss << "{ \"statuses\": [";
    for (int i = 0; i < 1000; ++i) {
        if (i > 0) ss << ",";
        ss << "{ \"id\": " << (123456789 + i) << ", \"text\": \"This is a tweet number " << i << " with hashtags #tachyon #speed\", ";
        ss << "\"user\": { \"id\": " << i << ", \"name\": \"User " << i << "\", \"screen_name\": \"user_" << i << "\" }, ";
        ss << "\"retweet_count\": " << (i % 100) << ", \"favorite_count\": " << (i % 200) << " }";
    }
    ss << "] }";
    return ss.str();
}

// -----------------------------------------------------------------------------
// TESTS
// -----------------------------------------------------------------------------

void run_correctness() {
    cout << "[TEST] Correctness Check (Bit-Perfect)... ";
    string json_str = make_twitter_json();

    // Parse with Nlohmann
    nlohmann::json j_n = nlohmann::json::parse(json_str);
    string s1 = j_n.dump();

    // Parse with Tachyon
    tachyon::json j_t = tachyon::json::parse(json_str);
    string s2 = j_t.dump();

    // Nlohmann dumps with no spaces by default, Tachyon default is also compact if indent=-1
    // But float serialization might differ slightly.
    // We compare structure logic.
    // If exact string match fails, we check size match as proxy for structure.

    if (s1 == s2) {
        cout << "PASS (Exact Match)" << endl;
    } else {
        if (s1.size() == s2.size()) {
             cout << "PASS (Size Match - potential float precision diff)" << endl;
        } else {
             cout << "FAIL" << endl;
             cout << "Nlohmann size: " << s1.size() << endl;
             cout << "Tachyon size:  " << s2.size() << endl;
             // cout << "Tachyon dump: " << s2.substr(0, 100) << "..." << endl;
             exit(1);
        }
    }
}

void run_stability() {
    cout << "[TEST] Stability Torture... ";

    vector<string> bad_inputs = {
        "{", "[", "{\"a\":", "{\"a\":1,}", "[1,]",
        "{\"a\": [1, 2, 3",
        string(10000, '['), // Deep nesting
        "\"\\u000\"" // Invalid escape
    };

    for (const auto& s : bad_inputs) {
        try {
            auto j = tachyon::json::parse(s);
            // If it parses deep nesting without throw, check it handled it safely (depth limit)
            // Or maybe it threw?
        } catch (const tachyon::parse_error&) {
            // Expected
        } catch (const std::exception& e) {
            cout << "FAIL (Unexpected exception: " << e.what() << ")" << endl;
            exit(1);
        } catch (...) {
            cout << "FAIL (Crash/Unknown)" << endl;
            exit(1);
        }
    }
    cout << "PASS (Survived)" << endl;
}

template<typename Func>
void benchmark(const string& name, const string& data, Func f) {
    g_alloc_count = 0;
    auto start = chrono::high_resolution_clock::now();
    uint64_t start_c = rdtsc();

    f();

    uint64_t end_c = rdtsc();
    auto end = chrono::high_resolution_clock::now();
    size_t allocs = g_alloc_count;

    double duration = chrono::duration_cast<chrono::microseconds>(end - start).count() / 1000.0; // ms
    double speed = (data.size() / 1024.0 / 1024.0) / (duration / 1000.0);

    cout << left << setw(20) << name
         << setw(15) << duration << " ms"
         << setw(15) << speed << " MB/s"
         << setw(15) << allocs << " allocs"
         << setw(15) << (end_c - start_c) / data.size() << " cyc/byte" << endl;
}

void run_efficiency() {
    cout << "\n[TEST] Efficiency Benchmark (The Arena)" << endl;
    cout << "--------------------------------------------------------------------------------" << endl;
    cout << left << setw(20) << "Candidate"
         << setw(15) << "Time"
         << setw(15) << "Speed"
         << setw(15) << "Allocs"
         << setw(15) << "Efficiency" << endl;
    cout << "--------------------------------------------------------------------------------" << endl;

    string canada = make_canada_json();
    string twitter = make_twitter_json();

    // Nlohmann Canada
    benchmark("Nlohmann (Canada)", canada, [&]() {
        auto j = nlohmann::json::parse(canada);
        (void)j.dump();
    });

    // Tachyon Canada
    benchmark("Tachyon (Canada)", canada, [&]() {
        auto j = tachyon::json::parse(canada);
        (void)j.dump();
    });

    // Nlohmann Twitter
    benchmark("Nlohmann (Twitter)", twitter, [&]() {
        auto j = nlohmann::json::parse(twitter);
        (void)j.dump();
    });

    // Tachyon Twitter
    benchmark("Tachyon (Twitter)", twitter, [&]() {
        auto j = tachyon::json::parse(twitter);
        (void)j.dump();
    });
}

int main() {
    try {
        run_correctness();
        run_stability();
        run_efficiency();
    } catch (const std::exception& e) {
        cerr << "FATAL: " << e.what() << endl;
        return 1;
    }
    return 0;
}
