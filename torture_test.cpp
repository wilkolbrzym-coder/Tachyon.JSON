#include "tachyon.hpp"
#include <iostream>
#include <vector>
#include <cassert>
#include <string>

// Replicate Canada.json structure for testing
struct Geo {
    std::string type;
    std::vector<std::vector<std::vector<double>>> coordinates;
};

TACHYON_DEFINE_TYPE(Geo, type, coordinates)

void test_utf8_escapes() {
    std::cout << "Testing UTF-8 Escapes & Surrogates..." << std::endl;

    // 1. Basic Escape
    {
        std::string json = R"("Hello\nWorld")";
        Tachyon::Scanner s(json);
        char buf[1024];
        auto res = s.scan_string(buf, 1024);
        if (res != "Hello\nWorld") {
            std::cerr << "FAIL: Basic Escape. Got: " << res << std::endl;
            exit(1);
        }
    }

    // 2. Unicode BMP (\uXXXX)
    {
        std::string json = R"("\u00A9 2026")"; // Copyright symbol
        Tachyon::Scanner s(json);
        char buf[1024];
        auto res = s.scan_string(buf, 1024);
        // UTF-8 for U+00A9 is C2 A9
        std::string expected = "\xC2\xA9 2026";
        if (res != expected) {
            std::cerr << "FAIL: BMP Escape. Got size " << res.size() << std::endl;
             for(char c : res) std::cerr << std::hex << (int)(unsigned char)c << " ";
             std::cerr << std::endl;
            exit(1);
        }
    }

    // 3. Surrogate Pair (Emoji)
    {
        // U+1F600 (Grinning Face) = \uD83D\uDE00
        std::string json = R"("\uD83D\uDE00")";
        Tachyon::Scanner s(json);
        char buf[1024];
        auto res = s.scan_string(buf, 1024);
        // UTF-8 for U+1F600 is F0 9F 98 80
        std::string expected = "\xF0\x9F\x98\x80";
        if (res != expected) {
            std::cerr << "FAIL: Surrogate Pair. Got size " << res.size() << std::endl;
            for(char c : res) std::cerr << std::hex << (int)(unsigned char)c << " ";
            std::cerr << std::endl;
            exit(1);
        }
    }

    std::cout << "PASS: Escapes & Surrogates" << std::endl;
}

void test_boundaries() {
    std::cout << "Testing 32-byte Boundaries..." << std::endl;
    std::string padding(64, ' ');
    std::string json = padding + R"("BoundaryCheck")";

    for (int i = 0; i < 64; ++i) {
        std::string p(i, ' ');
        std::string j = p + R"("TestString")";
        Tachyon::Scanner s(j);
        s.skip_whitespace();
        char buf[1024];
        auto res = s.scan_string(buf, 1024);
        if (res != "TestString") {
             std::cerr << "FAIL: Boundary offset " << i << std::endl;
             exit(1);
        }
    }
    std::cout << "PASS: Boundaries" << std::endl;
}

void test_malformed() {
    std::cout << "Testing Malformed JSON..." << std::endl;

    auto check_fail = [](std::string j) {
        try {
            Tachyon::Scanner s(j);
            char buf[1024];
            s.scan_string(buf, 1024);
            std::cerr << "FAIL: Should have thrown on: " << j << std::endl;
            exit(1);
        } catch (const Tachyon::Error&) {
            // Good
        }
    };

    check_fail(R"("Unterminated)");
    check_fail(R"("\uD83D")"); // Missing low surrogate
    check_fail(R"("Bad Escape \q")");

    std::cout << "PASS: Malformed" << std::endl;
}

struct Nested {
    std::vector<Nested> children;
};
TACHYON_DEFINE_TYPE(Nested, children)

void test_deep_nesting() {
    std::cout << "Testing Deep Nesting..." << std::endl;
    std::string json;
    int depth = 500;
    for(int i=0; i<depth; ++i) json += R"({"children":[)";
    for(int i=0; i<depth; ++i) json += R"(]})";

    Tachyon::Scanner s(json);
    Nested n;
    Tachyon::read(n, s);

    int count = 0;
    Nested* curr = &n;
    while (!curr->children.empty()) {
        count++;
        curr = &curr->children[0];
    }
    if (count != depth - 1) { // 500 objects, 499 links
         std::cerr << "FAIL: Nesting depth mismatch. Got " << count << " Expected " << depth - 1 << std::endl;
         exit(1);
    }
    std::cout << "PASS: Deep Nesting" << std::endl;
}

void test_canada_struct() {
    std::cout << "Testing Canada-like Struct (Geo)..." << std::endl;
    std::string json = R"({
        "type": "Polygon",
        "coordinates": [
            [
                [-10.0, 10.0], [-10.0, 20.0], [-20.0, 20.0], [-10.0, 10.0]
            ]
        ]
    })";

    Tachyon::Scanner s(json);
    Geo g;
    Tachyon::read(g, s);

    if (g.type != "Polygon") { std::cerr << "FAIL: Type mismatch" << std::endl; exit(1); }
    if (g.coordinates.size() != 1) { std::cerr << "FAIL: L1 size" << std::endl; exit(1); }
    if (g.coordinates[0].size() != 4) { std::cerr << "FAIL: L2 size" << std::endl; exit(1); }
    if (g.coordinates[0][0][0] != -10.0) { std::cerr << "FAIL: Value mismatch" << std::endl; exit(1); }

    std::cout << "PASS: Canada Struct" << std::endl;
}

int main() {
    try {
        test_utf8_escapes();
        test_boundaries();
        test_malformed();
        test_deep_nesting();
        test_canada_struct();
        std::cout << "ALL TORTURE TESTS PASSED." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Uncaught exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
