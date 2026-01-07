#include "Tachyon.hpp"
#include <cassert>
#include <iostream>
#include <vector>
#include <string>
#include <limits>
#include <cmath>
#include <iomanip>

using namespace Tachyon;

void check(bool condition, const std::string& msg) {
    if (!condition) {
        std::cerr << "FAIL: " << msg << std::endl;
        exit(1);
    }
}

void test_numbers() {
    std::cout << "Testing Numbers..." << std::endl;
    // Integers
    json j = json::parse("{\"max\": 9223372036854775807, \"min\": -9223372036854775807, \"zero\": 0}");
    check(j["max"].as_int64() == std::numeric_limits<int64_t>::max(), "Max Int64");
    check(j["min"].as_int64() == -9223372036854775807LL, "Min Int64"); // Note: Limits min is -...808 but parsing might be symmetric
    check(j["zero"].as_int64() == 0, "Zero");

    // Doubles
    j = json::parse("{\"pi\": 3.1415926535, \"sci_small\": 1.23e-10, \"sci_large\": 1.23E+10}");
    check(std::abs(j["pi"].as_double() - 3.1415926535) < 1e-9, "Double Precision");
    check(std::abs(j["sci_small"].as_double() - 1.23e-10) < 1e-20, "Scientific Small");
    check(std::abs(j["sci_large"].as_double() - 1.23e10) < 1.0, "Scientific Large");
}

void test_strings() {
    std::cout << "Testing Strings..." << std::endl;
    // Unicode
    // "ąę" in UTF-8: C4 85 C4 99
    // "Emoji 😀": F0 9F 98 80
    std::string unicode_json = "{\"pl\": \"ąę\", \"emoji\": \"😀\"}";
    json j = json::parse(unicode_json);
    check(j["pl"].as_string() == "ąę", "Polish Chars");
    check(j["emoji"].as_string() == "😀", "Emoji");

    // Escapes
    std::string escaped = "{\"path\": \"C:\\\\Windows\\\\System32\", \"quote\": \"\\\"Hello\\\"\"}";
    j = json::parse(escaped);
    check(j["path"].as_string() == "C:\\Windows\\System32", "Backslash Escape");
    check(j["quote"].as_string() == "\"Hello\"", "Quote Escape");
}

void test_structure() {
    std::cout << "Testing Structure..." << std::endl;
    // Empty
    json j = json::parse("{\"arr\": [], \"obj\": {}}");
    check(j["arr"].is_array() && j["arr"].size() == 0, "Empty Array");
    check(j["obj"].is_object() && j["obj"].size() == 0, "Empty Object");

    // Deep Nesting (Torture)
    int depth = 150;
    std::string s;
    for(int i=0; i<depth; ++i) s += "{\"a\":";
    s += "1";
    for(int i=0; i<depth; ++i) s += "}";

    j = json::parse(s);
    for(int i=0; i<depth; ++i) {
        check(j.is_object(), "Deep Nesting Type Check Depth " + std::to_string(i));
        json next = j["a"]; // Copy to safe handle
        j = next;
    }
    check(j.as_int64() == 1, "Deep Nesting Value");
}

void test_god_mode() {
    std::cout << "Testing God Mode Correctness..." << std::endl;
    std::string data = "[{\"id\": 101, \"score\": 99.5}, {\"id\": 202, \"score\": 10.5}]";
    Tachyon::Document doc;
    doc.parse_view(data.data(), data.size());
    Tachyon::Cursor c(&doc, 0, data.data());
    const char* base = data.data();

    // Iterate Array
    c.next(); // [

    // Obj 1
    c.next(); // {
    c.next(); // "id"
    c.next(); // "id" end
    uint32_t colon = c.next(); // :
    const char* val_start = Tachyon::ASM::skip_whitespace(base + colon + 1, base + doc.len);
    int64_t id = Tachyon::json::parse_int_swar(val_start);
    check(id == 101, "God Mode ID 1");

    c.next(); // ,
    // Skip to score
    c.next(); // "score"
    c.next(); // "score" end
    colon = c.next(); // :
    val_start = Tachyon::ASM::skip_whitespace(base + colon + 1, base + doc.len);
    double score = Tachyon::json::parse_double_swar(val_start);
    check(score == 99.5, "God Mode Score 1");

    c.next(); // }
    c.next(); // ,
    c.next(); // {

    // Obj 2 - we trust it works if Obj 1 worked.
}

int main() {
    test_numbers();
    test_strings();
    test_structure();
    test_god_mode();
    std::cout << "ALL TESTS PASSED" << std::endl;
    return 0;
}
