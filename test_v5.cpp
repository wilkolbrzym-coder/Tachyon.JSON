#include "Tachyon.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <chrono>
#include <sstream>

using namespace Tachyon;

void test_basic_types() {
    std::cout << "Testing Basic Types..." << std::endl;
    Json null_val = nullptr;
    assert(null_val.is_null());
    assert(null_val.type() == Type::Null);

    Json bool_val = true;
    assert(bool_val.is_boolean());
    assert(bool_val.get<bool>() == true);

    Json int_val = 42;
    assert(int_val.is_number_int());
    assert(int_val.get<int>() == 42);

    Json double_val = 3.14159;
    assert(double_val.is_number_float());
    assert(std::abs(double_val.get<double>() - 3.14159) < 0.0001);

    Json str_val = "Hello Tachyon";
    assert(str_val.is_string());
    assert(str_val.get<std::string>() == "Hello Tachyon");
    std::cout << "Basic Types OK" << std::endl;
}

void test_array() {
    std::cout << "Testing Array..." << std::endl;
    Json arr = {1, 2, "three"};
    assert(arr.is_array());
    assert(arr.size() == 3);
    assert(arr[0].get<int>() == 1);
    assert(arr[1].get<int>() == 2);
    assert(arr[2].get<std::string>() == "three");

    arr.push_back(4.0);
    assert(arr.size() == 4);
    assert(arr[3].is_number_float());

    // Clear
    arr.clear();
    assert(arr.empty());
    assert(arr.size() == 0);

    std::cout << "Array OK" << std::endl;
}

void test_object() {
    std::cout << "Testing Object..." << std::endl;
    Json obj = {{"name", "Tachyon"}, {"version", 5}};
    assert(obj.is_object());
    assert(obj.size() == 2);
    assert(obj["name"].get<std::string>() == "Tachyon");
    assert(obj["version"].get<int>() == 5);

    // Test modification
    obj["new_key"] = true;
    assert(obj.size() == 3);
    assert(obj["new_key"].get<bool>() == true);

    // Test replacement
    obj["version"] = 6;
    assert(obj["version"].get<int>() == 6);

    // Test nested
    obj["nested"] = {{"a", 1}};
    assert(obj["nested"]["a"].get<int>() == 1);

    // Test comparison
    Json obj2 = {{"name", "Tachyon"}, {"new_key", true}, {"version", 6}, {"nested", {{"a", 1}}}};
    assert(obj == obj2);

    std::cout << "Object OK" << std::endl;
}

void test_advanced() {
    std::cout << "Testing Advanced..." << std::endl;

    // Error Handling
    std::string bad_json = "{ \"key\": "; // incomplete
    try {
        Json j = Json::parse(bad_json);
        assert(false); // Should not reach here
    } catch (const JsonParseException& e) {
        std::cout << "Caught expected parse error: " << e.what() << std::endl;
        assert(e.line() > 0);
    }

    // Unicode
    std::string unicode_json = "{\"emoji\": \"\\u263A\"}"; // Smiley face
    Json uj = Json::parse(unicode_json);
    std::string s = uj["emoji"].get<std::string>();
    // UTF-8 for U+263A is E2 98 BA
    assert(static_cast<unsigned char>(s[0]) == 0xE2);
    assert(static_cast<unsigned char>(s[1]) == 0x98);
    assert(static_cast<unsigned char>(s[2]) == 0xBA);

    // Surrogate Pair Test (U+1F600 = \uD83D\uDE00)
    std::string surrogate_json = "{\"smile\": \"\\uD83D\\uDE00\"}";
    Json sj = Json::parse(surrogate_json);
    std::string s2 = sj["smile"].get<std::string>();
    // U+1F600 in UTF-8: F0 9F 98 80
    assert(static_cast<unsigned char>(s2[0]) == 0xF0);
    assert(static_cast<unsigned char>(s2[1]) == 0x9F);
    assert(static_cast<unsigned char>(s2[2]) == 0x98);
    assert(static_cast<unsigned char>(s2[3]) == 0x80);

    // Deep Nesting
    std::string deep = "{\"a\":{\"b\":{\"c\":{\"d\":1}}}}";
    Json dj = Json::parse(deep);
    assert(dj["a"]["b"]["c"]["d"].get<int>() == 1);

    std::cout << "Advanced OK" << std::endl;
}

void test_parser() {
    std::cout << "Testing Parser..." << std::endl;
    std::string json_str = R"({
        "key": "value",
        "list": [1, 2, 3],
        "obj": { "inner": true },
        "num": 123.456,
        "unicode": "\u0041"
    })";

    Json j = Json::parse(json_str);
    assert(j["key"].get<std::string>() == "value");
    assert(j["list"].size() == 3);
    assert(j["list"][1].get<int>() == 2);
    assert(j["obj"]["inner"].get<bool>() == true);
    assert(j["unicode"].get<std::string>() == "A");

    std::cout << "Parser OK" << std::endl;
}

void test_serializer() {
    std::cout << "Testing Serializer..." << std::endl;
    Json j = {{"a", 1}, {"b", {1, 2}}};
    std::string dump = j.dump();
    std::cout << "Dump: " << dump << std::endl;

    Json j2 = Json::parse(dump);
    assert(j2["a"].get<int>() == 1);
    assert(j2["b"][0].get<int>() == 1);
    std::cout << "Serializer OK" << std::endl;
}

void test_performance() {
    std::cout << "Testing Performance..." << std::endl;

    // 1. Parse Speed
    std::cout << "[Parse Speed]" << std::endl;
    std::string big_json = "[";
    for(int i=0; i<10000; ++i) {
        big_json += R"({"id":)" + std::to_string(i) + R"(, "name": "Item )" + std::to_string(i) + R"("})";
        if(i < 9999) big_json += ",";
    }
    big_json += "]";

    auto start = std::chrono::high_resolution_clock::now();
    Json parsed = Json::parse(big_json);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << "Parsed 10,000 objects in " << elapsed.count() << " ms" << std::endl;
    assert(parsed.size() == 10000);
    // Verify sorted optimization: Check last element access (should be fast if sorted logic works, though it's inside array)
    // Access object inside array
    assert(parsed[9999]["id"].get<int>() == 9999);

    // 2. Build Speed (Object)
    std::cout << "[Build Speed - operator[]]" << std::endl;
    start = std::chrono::high_resolution_clock::now();
    Json root;
    int build_count = 2000;
    for(int i=0; i<build_count; ++i) {
        root[std::to_string(i)] = i;
    }
    end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;
    std::cout << "Inserted " << build_count << " items in " << elapsed.count() << " ms" << std::endl;

    // 3. Access Speed
    std::cout << "[Access Speed]" << std::endl;
    // Force sort if not already? (It sorts on first access if we implemented it, or if build was chaotic)
    // Actually, operator[] marks unsorted.
    // So this will be slow unless we manually sort or trigger it.
    // Let's trigger a sort via a dummy find or implementation detail?
    // User can't explicitly call sort() on Json.
    // However, find_impl calls lower_bound ONLY IF m_sorted is true.
    // operator[] appends and sets m_sorted = false.
    // So this access loop will be O(N^2) if we don't sort.
    // BUT, ObjectMap::find_impl doesn't auto-sort (because it's const or non-const find).
    // We should probably auto-sort on first read access if feasible?
    // No, `find_impl` is const, can't sort.
    // Modifiable `operator[]` calls `find_impl`.

    start = std::chrono::high_resolution_clock::now();
    int sum = 0;
    for(int i=0; i<build_count; ++i) {
        sum += root[std::to_string(i)].get<int>();
    }
    end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;
    std::cout << "Read " << build_count << " items in " << elapsed.count() << " ms" << std::endl;
}

int main() {
    try {
        test_basic_types();
        test_array();
        test_object();
        test_advanced();
        test_parser();
        test_serializer();
        test_performance();
        std::cout << "ALL TESTS PASSED" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test Failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
