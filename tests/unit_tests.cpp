#include "../Tachyon.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <limits>
#include <numbers>
#include <random>
#include <fstream>
#include <sstream>

// 5x Intensity Test Suite
// Includes Fuzzing, Edge Cases, Deep Nesting, Large Data

#define ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while (0)

using namespace Tachyon;

// Helper to read file
std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

void test_basic_scalars() {
    std::cout << "[Test] Basic Scalars..." << std::endl;
    ASSERT(json::parse("true").get<bool>() == true);
    ASSERT(json::parse("false").get<bool>() == false);
    ASSERT(json::parse("null").is_null());
    ASSERT(json::parse("\"\"").get<std::string>() == "");
}

void test_numbers_extended() {
    std::cout << "[Test] Numbers Extended..." << std::endl;
    ASSERT(json::parse("0").get<int>() == 0);
    ASSERT(json::parse("-0").get<int>() == 0);
    ASSERT(json::parse("123456789").get<int>() == 123456789);
    ASSERT(json::parse("-123456789").get<int>() == -123456789);

    // Max/Min Int64
    auto max_i64 = std::numeric_limits<int64_t>::max();
    auto min_i64 = std::numeric_limits<int64_t>::min();
    ASSERT(json::parse(std::to_string(max_i64)).get<int64_t>() == max_i64);
    ASSERT(json::parse(std::to_string(min_i64)).get<int64_t>() == min_i64);

    // Double precision
    double pi = 3.141592653589793;
    ASSERT(std::abs(json::parse("3.141592653589793").get<double>() - pi) < 1e-15);
    ASSERT(json::parse("1e10").get<double>() == 1e10);
    ASSERT(json::parse("1E10").get<double>() == 1e10);
    ASSERT(json::parse("1e+10").get<double>() == 1e10);
    ASSERT(json::parse("1e-10").get<double>() == 1e-10);
}

void test_unicode_and_escapes() {
    std::cout << "[Test] Unicode & Escapes..." << std::endl;
    ASSERT(json::parse("\"\\\"\"").get<std::string>() == "\"");
    ASSERT(json::parse("\"\\\\\"").get<std::string>() == "\\");
    ASSERT(json::parse("\"\\/\"").get<std::string>() == "/");
    ASSERT(json::parse("\"\\b\"").get<std::string>() == "\b");
    ASSERT(json::parse("\"\\f\"").get<std::string>() == "\f");
    ASSERT(json::parse("\"\\n\"").get<std::string>() == "\n");
    ASSERT(json::parse("\"\\r\"").get<std::string>() == "\r");
    ASSERT(json::parse("\"\\t\"").get<std::string>() == "\t");

    // Unicode
    ASSERT(json::parse("\"\\u0041\"").get<std::string>() == "A");
    ASSERT(json::parse("\"\\u0024\"").get<std::string>() == "$");
    ASSERT(json::parse("\"\\u00A2\"").get<std::string>() == "\xC2\xA2"); // Cent sign
    ASSERT(json::parse("\"\\u20AC\"").get<std::string>() == "\xE2\x82\xAC"); // Euro sign

    // Surrogate Pairs (G clef U+1D11E)
    // \uD834\uDD1E
    ASSERT(json::parse("\"\\uD834\\uDD1E\"").get<std::string>() == "\xF0\x9D\x84\x9E");
}

void test_deep_nesting() {
    std::cout << "[Test] Deep Nesting..." << std::endl;
    int depth = 500;
    std::string s;
    for(int i=0; i<depth; ++i) s += "{\"a\":";
    s += "1";
    for(int i=0; i<depth; ++i) s += "}";

    auto j = json::parse(s);
    for(int i=0; i<depth; ++i) {
        j = j["a"];
    }
    ASSERT(j.get<int>() == 1);
}

void test_large_array() {
    std::cout << "[Test] Large Array..." << std::endl;
    std::string s = "[";
    for(int i=0; i<10000; ++i) {
        if(i > 0) s += ",";
        s += std::to_string(i);
    }
    s += "]";
    auto j = json::parse(s);
    ASSERT(j.size() == 10000);
    ASSERT(j[9999].get<int>() == 9999);
}

void fuzz_test() {
    std::cout << "[Test] Fuzzing..." << std::endl;
    std::mt19937 gen(123);
    std::uniform_int_distribution<> char_dist(32, 126);
    std::uniform_int_distribution<> len_dist(1, 1000);

    for(int i=0; i<100; ++i) {
        int len = len_dist(gen);
        std::string s;
        for(int k=0; k<len; ++k) s += (char)char_dist(gen);

        // Just ensure it doesn't crash
        try {
            json::parse(s);
        } catch (...) {
            // Expected failure for random garbage
        }
    }
}

void test_api_usability() {
    std::cout << "[Test] API Usability..." << std::endl;
    // Implicit conversions
    // Note: C++ prevents some implicit conversions if explicit constructors exist.
    // We didn't add implicit constructors to avoid ambiguity, but we have get<T>().

    auto j = json::parse(R"({"name": "Tachyon", "version": 6.3})");
    ASSERT(j["name"].get<std::string>() == "Tachyon");
    ASSERT(j["version"].get<double>() == 6.3);

    // Check non-existent
    ASSERT(j["missing"].is_null()); // Returns null json
}

void test_find_path_mock() {
    // Testing the stub to ensure it exists
    std::string_view p = Tachyon::find_path("{}", "a.b");
    ASSERT(p == "");
}

int main() {
    try {
        test_basic_scalars();
        test_numbers_extended();
        test_unicode_and_escapes();
        test_deep_nesting();
        test_large_array();
        test_api_usability();
        test_find_path_mock();
        fuzz_test();

        std::cout << "========================================" << std::endl;
        std::cout << "   ALL 5X TESTS PASSED   " << std::endl;
        std::cout << "========================================" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
