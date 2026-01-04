#include "../Tachyon.hpp"
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

void test_scalars() {
    std::cout << "Testing Scalars..." << std::endl;

    // Boolean
    ASSERT(json::parse("true").get<bool>() == true);
    ASSERT(json::parse("false").get<bool>() == false);

    // Null
    ASSERT(json::parse("null").is_null());

    // Empty String
    ASSERT(json::parse("\"\"").get<std::string>() == "");
}

void test_numbers() {
    std::cout << "Testing Numbers..." << std::endl;

    // Integers
    ASSERT(json::parse("123").get<int>() == 123);
    ASSERT(json::parse("-123").get<int>() == -123);
    ASSERT(json::parse("0").get<int>() == 0);
    ASSERT(json::parse(std::to_string(std::numeric_limits<int64_t>::max())).get<int64_t>() == std::numeric_limits<int64_t>::max());
    ASSERT(json::parse(std::to_string(std::numeric_limits<int64_t>::min())).get<int64_t>() == std::numeric_limits<int64_t>::min());

    // Floats
    double pi = json::parse("3.14159").get<double>();
    ASSERT(std::abs(pi - 3.14159) < 1e-6);

    double sci = json::parse("1.23e-10").get<double>();
    ASSERT(std::abs(sci - 1.23e-10) < 1e-13);
}

void test_strings() {
    std::cout << "Testing Strings..." << std::endl;

    // Basic
    ASSERT(json::parse("\"hello\"").get<std::string>() == "hello");

    // Escapes
    ASSERT(json::parse("\"\\\"\"").get<std::string>() == "\"");
    ASSERT(json::parse("\"\\\\\"").get<std::string>() == "\\");
    ASSERT(json::parse("\"\\n\"").get<std::string>() == "\n");
    ASSERT(json::parse("\"\\t\"").get<std::string>() == "\t");
    ASSERT(json::parse("\"\\r\"").get<std::string>() == "\r");
    ASSERT(json::parse("\"\\b\"").get<std::string>() == "\b");
    ASSERT(json::parse("\"\\f\"").get<std::string>() == "\f");
    ASSERT(json::parse("\"\\u0041\"").get<std::string>() == "A"); // Unicode A
}

void test_objects() {
    std::cout << "Testing Objects..." << std::endl;

    auto j = json::parse(R"({"a": 1, "b": "test", "c": true})");
    ASSERT(j.is_object());
    ASSERT(j["a"].get<int>() == 1);
    ASSERT(j["b"].get<std::string>() == "test");
    ASSERT(j["c"].get<bool>() == true);

    // Nested
    auto j2 = json::parse(R"({"nested": {"x": 10}})");
    ASSERT(j2["nested"]["x"].get<int>() == 10);
}

void test_arrays() {
    std::cout << "Testing Arrays..." << std::endl;

    auto j = json::parse("[1, 2, 3]");
    ASSERT(j.is_array());
    ASSERT(j.size() == 3);
    ASSERT(j[0].get<int>() == 1);
    ASSERT(j[1].get<int>() == 2);
    ASSERT(j[2].get<int>() == 3);

    // Mixed
    auto j2 = json::parse(R"([1, "two", true, null])");
    ASSERT(j2.size() == 4);
    ASSERT(j2[1].get<std::string>() == "two");
    ASSERT(j2[3].is_null());
}

void test_error_handling() {
    std::cout << "Testing Error Handling..." << std::endl;
    // (Optional tests)
}

int main() {
    try {
        test_scalars();
        test_numbers();
        test_strings();
        test_objects();
        test_arrays();
        test_error_handling();

        std::cout << "========================================" << std::endl;
        std::cout << "   PASSED   " << std::endl;
        std::cout << "========================================" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
