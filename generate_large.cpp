#include <iostream>
#include <fstream>
#include <vector>
#include <string>

int main() {
    std::ofstream out("large_file.json");
    out << "[";
    for (int i = 0; i < 500000; ++i) {
        if (i > 0) out << ",";
        // Mix of structures to mimic complexity
        out << R"({"id":)" << i << R"(,"name":"Entity_)" << i << R"(","values":[1.23456789012345678, -9876.54321, 1e-10, 12345678901234567890]})";
    }
    out << "]";
    out.close();
    return 0;
}
