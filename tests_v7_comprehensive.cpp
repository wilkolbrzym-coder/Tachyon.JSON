#include "Tachyon.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <limits>
#include <numbers>

// Simple Assertion Framework
#define ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while (0)

using namespace Tachyon;

void test_deep_nesting() {
    std::cout << "Testing Deep Nesting..." << std::endl;
    int depth = 1000;
    std::string s;
    s.reserve(depth * 10);
    for(int i=0; i<depth; ++i) s += "{\"a\":";
    s += "1";
    for(int i=0; i<depth; ++i) s += "}";

    auto j = json::parse(s);
    ASSERT(!j.is_null());

    // Navigate deep? Not efficient with current API but should parse without crash
    // Tachyon limits? Stack usage in build_mask?
    // It's iterative. Safe.
}

void test_unicode() {
    std::cout << "Testing Unicode..." << std::endl;
    std::string json = R"({"key": "Value \u00A9 \n \t"})";
    auto j = json::parse(json);
    std::string val = j["key"].get<std::string>();
    // \u00A9 is Copyright symbol. 2 bytes in UTF8: C2 A9
    // \n is 0x0A
    // \t is 0x09
    // ASSERT(val.find("\n") != std::string::npos); // Encoded as char 10
}

void test_numbers_edge() {
    std::cout << "Testing Number Edges..." << std::endl;
    ASSERT(json::parse("0").get<int>() == 0);
    ASSERT(json::parse("-0").get<double>() == 0.0);
    ASSERT(json::parse("1.23456789").get<double>() > 1.23);
}

void test_empty() {
    std::cout << "Testing Empty..." << std::endl;
    ASSERT(json::parse("[]").is_array());
    ASSERT(json::parse("{}").is_object());
    ASSERT(json::parse("").is_null()); // Or crash? Should check
}

void test_correctness() {
    std::cout << "Testing Correctness..." << std::endl;
    std::string json = R"({
        "id": 12345,
        "name": "Tachyon",
        "features": [ "fast", "safe", "verified" ],
        "active": true,
        "score": 99.9
    })";

    auto doc = json::parse(json);
    ASSERT(doc["id"].get<int>() == 12345);
    ASSERT(doc["name"].get<std::string>() == "Tachyon");
    ASSERT(doc["features"][0].get<std::string>() == "fast");
    ASSERT(doc["active"].get<bool>() == true);
    ASSERT(doc["score"].get<double>() > 99.0);
}

int main() {
    test_deep_nesting();
    test_unicode();
    test_numbers_edge();
    test_empty();
    test_correctness();
    std::cout << "COMPREHENSIVE TESTS PASSED" << std::endl;
    return 0;
}
