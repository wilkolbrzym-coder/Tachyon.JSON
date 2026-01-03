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

    Value root = doc.root();
    assert(root.is_object());

    Value num = root["num"];
    assert(num.is_number());
    assert(num.get_int() == 123);

    Value flt = root["float"];
    assert(flt.is_number());
    // Fuzzy compare
    assert(flt.get_double() > 3.13 && flt.get_double() < 3.15);

    Value b = root["bool"];
    assert(b.is_true());

    Value s = root["str"];
    assert(s.is_string());
    assert(s.get_string() == "hello");
}

void test_array() {
    std::cout << "[Test] Array..." << std::endl;
    Document doc;
    doc.parse(R"([1, 2, 3])");
    Value root = doc.root();
    assert(root.is_array());
    assert(root.size() == 3);
    assert(root[0].get_int() == 1);
    assert(root[2].get_int() == 3);
}

void test_nested() {
    std::cout << "[Test] Nested..." << std::endl;
    Document doc;
    doc.parse(R"({"a": {"b": [10]}})");
    Value root = doc.root();
    Value val = root["a"]["b"][0];
    assert(val.get_int() == 10);
}

void test_escapes() {
     std::cout << "[Test] Escapes (Basic)..." << std::endl;
     // Note: v8.4 parser handles escapes by scanning over them,
     // but does not unescape them in-place (Zero-Copy).
     // The string_view will contain the raw chars including backslash.
     // To fully support escapes, we need a separate buffer or decode on demand.
     // For "Nuclear" speed, we often defer this.
     // Let's verify it scans correctly at least.
     Document doc;
     doc.parse(R"({"key": "value\"with\"quote"})");
     Value root = doc.root();
     std::string_view s = root["key"].get_string();
     assert(s == "value\\\"with\\\"quote"); // Raw
}

int main() {
    test_parsing();
    test_array();
    test_nested();
    test_escapes();
    std::cout << "ALL TESTS PASSED" << std::endl;
    return 0;
}
