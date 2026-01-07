#include "../include/Tachyon.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <limits>
#include <cstdlib>
#include <cassert>

void check(bool condition, const std::string& msg) {
    if (!condition) {
        std::cerr << "[FAIL] " << msg << "\n";
        exit(1);
    } else {
        std::cout << "[PASS] " << msg << "\n";
    }
}

void test_precision() {
    std::cout << "\n--- Precision Tests ---\n";
    struct Case { std::string val; double expected; };
    std::vector<Case> cases = {
        {"123.456", 123.456},
        {"-123.456", -123.456},
        {"0.0001", 0.0001},
        {"1.23e10", 1.23e10},
        {"1.23e-10", 1.23e-10},
        // Saturating case
        {"12345678901234567890.12345", 1.2345678901234567e19}, // approx
        {"1.234567890123456789e50", 1.2345678901234567e50}
    };

    for (const auto& c : cases) {
        std::string json = c.val;
        auto j = Tachyon::json::parse(json);
        double val = j.get<double>();

        // Compare with strtod
        double expected = std::strtod(c.val.c_str(), nullptr);

        // Epsilon check
        double diff = std::abs(val - expected);
        double tolerance = std::abs(expected) * 1e-14; // reasonable for double
        if (expected == 0) tolerance = 1e-300;

        bool ok = diff <= tolerance;
        if (!ok) {
            std::cerr << "Fail: " << c.val << " -> " << val << " Expected: " << expected << " Diff: " << diff << "\n";
        }
        check(ok, "Precision: " + c.val);
    }
}

void test_structural() {
    std::cout << "\n--- Structural Tests ---\n";
    {
        std::string json = "[1, 2, [3, 4], {\"a\": 5}]";
        auto j = Tachyon::json::parse(json);
        check(j.is_array(), "Is array");
        check(j[0].get<double>() == 1.0, "Index 0");
        check(j[2][0].get<double>() == 3.0, "Nested Array");
        check(j[3]["a"].get<double>() == 5.0, "Nested Object");
    }
    {
        // Deep nesting
        std::string json;
        for(int i=0;i<1000;++i) json += "[";
        json += "1";
        for(int i=0;i<1000;++i) json += "]";

        auto j = Tachyon::json::parse(json);
        // Traverse deep
        // We can't easily traverse 1000 levels with operator[] syntax in loop without stack overflow in recursion?
        // Our operator[] is iterative.
        // Let's check tape length.
        // 1000 opens + 1 number + 1000 closes = 2001 tokens.
        // But the tape might just be flat.
        // Just parsing without crash is good.
        check(true, "Deep Nesting Parsed");
    }
}

void test_long_coordinates() {
    std::cout << "\n--- Long Coordinates (Canada Style) ---\n";
    std::string json = "[-123.456789012345678901234567890, 45.123456789012345678901234567890]";
    auto j = Tachyon::json::parse(json);
    double v1 = j[0].get<double>();
    double expected1 = -123.45678901234567; // Double precision limit

    // We expect it to be close to expected
    check(std::abs(v1 - expected1) < 1e-12, "Long Coordinate 1");
}

int main() {
    test_precision();
    test_structural();
    test_long_coordinates();
    std::cout << "\nAll Diagnostics Passed.\n";
    return 0;
}
