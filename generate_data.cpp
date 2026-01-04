#include <iostream>
#include <fstream>
#include <string>
#include <vector>

void generate_large(size_t size_mb) {
    std::ofstream out("large.json");
    out << "[";
    size_t bytes = 1;
    int i = 0;
    while (bytes < size_mb * 1024 * 1024) {
        if (i > 0) { out << ","; bytes++; }
        std::string s = R"({"id":)" + std::to_string(i) + R"(,"name":"Item )" + std::to_string(i) + R"(","active":true,"scores":[1,2,3,4,5]})";
        out << s;
        bytes += s.size();
        i++;
    }
    out << "]";
    out.close();
}

void generate_nested(int depth) {
    std::ofstream out("nested.json");
    for (int i = 0; i < depth; ++i) out << R"({"a":)";
    out << "1";
    for (int i = 0; i < depth; ++i) out << "}";
    out.close();
}

int main() {
    generate_large(25); // 25MB
    generate_nested(1000); // 1000 levels
    return 0;
}
