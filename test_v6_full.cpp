#include "Tachyon.hpp"
#include <iostream>
#include <cassert>
#include <cstring>
#include <string>

using namespace Tachyon;

void test_parsing() {
    std::cout << "[Test] Parsing..." << std::endl;
    Document doc;
    doc.parse(R"({"num": 123, "float": 3.14, "bool": true, "str": "hello"})");

    assert(doc["num"].type == Type::NumberInt);
    assert(doc["num"].i == 123);
    assert(doc["float"].type == Type::NumberFloat);
    assert(doc["bool"].type == Type::True);
    assert(doc["str"].type == Type::String);

    // Check string content
    std::string s(doc["str"].s.ptr, doc["str"].s.len);
    assert(s == "hello");
}

void test_array() {
    std::cout << "[Test] Array..." << std::endl;
    Document doc;
    doc.parse(R"([1, 2, 3])");
    assert(doc.root.type == Type::Array);
    assert(doc.root.a.size == 3);
    assert(doc.root.a.elements[0].i == 1);
    assert(doc.root.a.elements[2].i == 3);
}

void test_nested() {
    std::cout << "[Test] Nested..." << std::endl;
    Document doc;
    doc.parse(R"({"a": {"b": [10]}})");
    Value* val = doc.pointer("/a/b/0");
    assert(val != nullptr);
    assert(val->i == 10);
}

void test_serialization() {
    std::cout << "[Test] Serialization..." << std::endl;
    Document doc;
    std::string json = R"({"key":123,"arr":[1,2]})";
    doc.parse(json);
    std::string out = doc.dump();
    assert(out == json);
}

void test_safety() {
    std::cout << "[Test] Safety..." << std::endl;
    Document doc;
    // Malformed JSON should not crash (though behavior depends on error handling strategy,
    // here we check it doesn't segfault on simple bad inputs)
    try {
        doc.parse(R"({"unclosed": "string)");
        // Should stop or throw
    } catch (...) {}

    // Access non-existent
    try {
        doc["invalid"];
        assert(false);
    } catch (const std::exception& e) {
        assert(std::string(e.what()) == "Key not found");
    }
}

int main() {
    test_parsing();
    test_array();
    test_nested();
    test_serialization();
    test_safety();
    std::cout << "ALL TESTS PASSED" << std::endl;
    return 0;
}
