#include "../include/Tachyon.hpp"
#include <cassert>
#include <iostream>

void test_simple() {
    std::string json = R"({"key": "value", "num": 123, "float": 3.14, "bool": true, "arr": [1, 2, 3]})";
    Tachyon::Document doc;
    doc.parse(json);

    // Manual Tape Inspection
    // { -> START_OBJ
    // key -> STRING "key"
    // value -> STRING "value"
    // ...

    assert(doc.tape_len > 0);
    // Verify tape contents roughly
    // 0: START_OBJ
    assert(Tachyon::get_type(doc.tape[0]) == Tachyon::T_OBJ_START);

    std::cout << "test_simple passed\n";
}

void test_nested() {
    std::string json = "[[[[[]]]]]";
    Tachyon::Document doc;
    doc.parse(json);
    assert(doc.tape_len == 10); // 5 start, 5 end
    std::cout << "test_nested passed\n";
}

int main() {
    test_simple();
    test_nested();
    std::cout << "All tests passed.\n";
    return 0;
}
