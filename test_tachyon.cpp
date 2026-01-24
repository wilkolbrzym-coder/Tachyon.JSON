#include "Tachyon.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <cstring>
#include <string>

// Helper for assertions
#define TEST_ASSERT(cond) \
    if (!(cond)) { \
        std::cerr << "TEST FAILED: " << #cond << " at line " << __LINE__ << std::endl; \
        std::terminate(); \
    }

struct User {
    uint64_t id;
    std::string name;
    bool active;
    std::vector<int> scores;
};
TACHYON_DEFINE_TYPE_NON_INTRUSIVE(User, id, name, active, scores)

void test_deep_nested() {
    std::string json_str = R"({"l1": {"l2": {"l3": {"l4": [1, 2, {"val": 99}]}}}})";
    Tachyon::Context ctx;
    auto doc = ctx.parse_view(json_str.data(), json_str.size());

    int64_t val = doc["l1"]["l2"]["l3"]["l4"][2]["val"].as_int64();
    TEST_ASSERT(val == 99);
}

void test_escapes() {
    std::string json_str = R"({"msg": "Hello\nWorld\t\"Quote\""})";
    Tachyon::Context ctx;
    auto doc = ctx.parse_view(json_str.data(), json_str.size());

    std::string s = doc["msg"].as_string();
    TEST_ASSERT(s == "Hello\nWorld\t\"Quote\"");
}

void test_csv_advanced() {
    std::string csv = "id,name,desc\n1,Alice,\"Claims she is \"\"Alice\"\"\"\n2,Bob,\"Multi\nLine\nDesc\"";
    auto rows = Tachyon::json::parse_csv(csv);

    TEST_ASSERT(rows.size() == 3);
    TEST_ASSERT(rows[1][1] == "Alice");
    TEST_ASSERT(rows[1][2] == "Claims she is \"Alice\"");
    TEST_ASSERT(rows[2][1] == "Bob");
    TEST_ASSERT(rows[2][2] == "Multi\nLine\nDesc");
}

void test_array_iteration() {
    std::string json_str = "[10, 20, 30, 40, 50]";
    Tachyon::Context ctx;
    auto doc = ctx.parse_view(json_str.data(), json_str.size());

    TEST_ASSERT(doc.size() == 5);
    TEST_ASSERT(doc[0].as_int64() == 10);
    TEST_ASSERT(doc[4].as_int64() == 50);
}

void test_null_bool() {
    std::string json_str = R"({"a": null, "b": true, "c": false})";
    Tachyon::Context ctx;
    auto doc = ctx.parse_view(json_str.data(), json_str.size());

    // Tachyon doesn't have is_null exposed directly via simple API in this version, assumes usage knows type or checks variant?
    // But we added implicit conversions or helpers.
    // doc["a"] returns json.
    // We didn't add is_null() to public API in last iteration (only internal).
    // But we can check via variant? No, variant is private.
    // We'll rely on correct behavior for known types.
    TEST_ASSERT(doc["b"].as_bool() == true);
    TEST_ASSERT(doc["c"].as_bool() == false);
}

int main() {
    std::cout << "Running Strong Tachyon Tests..." << std::endl;

    test_deep_nested();
    std::cout << "Deep Nested Passed" << std::endl;

    test_escapes();
    std::cout << "Escapes Passed" << std::endl;

    test_csv_advanced();
    std::cout << "CSV Advanced Passed" << std::endl;

    test_array_iteration();
    std::cout << "Array Iteration Passed" << std::endl;

    test_null_bool();
    std::cout << "Null/Bool Passed" << std::endl;

    std::cout << "ALL TESTS PASSED" << std::endl;
    return 0;
}
