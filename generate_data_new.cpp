#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <random>

int main() {
    std::string filename = "huge.json";
    size_t target_size = 256 * 1024 * 1024; // 256MB

    std::ofstream f(filename, std::ios::binary);
    f << "[";

    std::string entry_template = R"({"id":123456,"name":"Item 123456","active":true,"scores":[1,2,3,4,5],"description":"This is a test object to fill up memory and test throughput."})";

    size_t current_size = 1; // '['
    bool first = true;

    while (current_size < target_size - 100) {
        if (!first) {
            f << ",";
            current_size++;
        }
        f << entry_template;
        current_size += entry_template.size();
        first = false;
    }

    f << "]";
    f.close();
    std::cout << "Generated huge.json (" << current_size << " bytes)" << std::endl;

    // Unaligned test
    std::ofstream f_un( "unaligned.json", std::ios::binary);
    // Pad with bytes to make it unaligned relative to 64-byte boundary if loaded at 0
    // But typically we simulate unalignment in the loader.
    // Here we just write valid JSON. We will offset the pointer in the test.
    f_un << R"({"test": "unaligned"})";
    f_un.close();

    return 0;
}
