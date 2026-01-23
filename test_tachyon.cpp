#include "Tachyon.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <cstring>

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

void test_json_basic() {
    std::string json_str = R"({"id": 1, "name": "Test", "active": true, "scores": [1, 2, 3]})";
    Tachyon::Context ctx;
    auto doc = ctx.parse_view(json_str.data(), json_str.size());

    TEST_ASSERT(doc.contains("id"));
    TEST_ASSERT(doc["id"].as_int64() == 1);
    TEST_ASSERT(doc["name"].as_string() == "Test");
    TEST_ASSERT(doc["active"].as_bool() == true);
    TEST_ASSERT(doc["scores"].is_array());
    TEST_ASSERT(doc["scores"].size() == 3);
}

void test_apex_typed() {
    std::string json_str = R"({"id": 99, "name": "Apex", "active": false, "scores": [10, 20]})";
    User u;
    Tachyon::json::parse(json_str).get_to(u);

    TEST_ASSERT(u.id == 99);
    TEST_ASSERT(u.name == "Apex");
    TEST_ASSERT(u.active == false);
    TEST_ASSERT(u.scores.size() == 2);
    TEST_ASSERT(u.scores[0] == 10);
    TEST_ASSERT(u.scores[1] == 20);
}

void test_csv_basic() {
    std::string csv = "name,age\nAlice,30\nBob,25";
    auto rows = Tachyon::json::parse_csv(csv);
    TEST_ASSERT(rows.size() == 3); // Header + 2 rows
    TEST_ASSERT(rows[0][0] == "name");
    TEST_ASSERT(rows[1][0] == "Alice");
    TEST_ASSERT(rows[2][1] == "25");
}

struct Person {
    std::string name;
    int age;
};
TACHYON_DEFINE_TYPE_NON_INTRUSIVE(Person, name, age)

void test_csv_typed() {
    std::string csv = "name,age\nAlice,30\nBob,25";
    auto people = Tachyon::json::parse_csv_typed<Person>(csv);
    TEST_ASSERT(people.size() == 2);
    TEST_ASSERT(people[0].name == "Alice");
    TEST_ASSERT(people[0].age == 30);
    TEST_ASSERT(people[1].name == "Bob");
    TEST_ASSERT(people[1].age == 25);
}

void test_utf8_validation() {
    // Valid UTF-8
    std::string valid = "{\"key\": \"Zażółć gęślą jaźń\"}";
    try {
        Tachyon::json::parse(valid);
    } catch (...) {
        TEST_ASSERT(false && "Should not throw on valid UTF-8");
    }

    // Invalid UTF-8 (Truncated sequence / Invalid byte)
    // 0xFF is invalid in UTF-8
    std::string invalid = "{\"key\": \"\xFF\"}";
    bool caught = false;
    try {
        auto doc = Tachyon::json::parse(invalid);
        // Force access to trigger lazy validation
        if (doc.contains("key")) {
             doc["key"].as_string();
        }
    } catch (const std::exception& e) {
        caught = true;
    }
    TEST_ASSERT(caught);
}

void test_large_lazy() {
    // 1000 items
    std::string big = "[";
    for(int i=0; i<1000; ++i) {
        if(i>0) big += ",";
        big += std::to_string(i);
    }
    big += "]";

    Tachyon::Context ctx;
    auto doc = ctx.parse_view(big.data(), big.size());
    TEST_ASSERT(doc.size() == 1000);
}

int main() {
    std::cout << "Running Tachyon Tests..." << std::endl;
    test_json_basic();
    std::cout << "JSON Basic Passed" << std::endl;
    test_apex_typed();
    std::cout << "Apex Typed Passed" << std::endl;
    test_csv_basic();
    std::cout << "CSV Basic Passed" << std::endl;
    test_csv_typed();
    std::cout << "CSV Typed Passed" << std::endl;
    test_utf8_validation();
    std::cout << "UTF-8 Validation Passed" << std::endl;
    test_large_lazy();
    std::cout << "Large Lazy Passed" << std::endl;
    std::cout << "ALL TESTS PASSED" << std::endl;
    return 0;
}
