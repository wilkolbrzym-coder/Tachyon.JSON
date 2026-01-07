#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <random>

void generate_file(const std::string& filename, size_t size_mb) {
    std::ofstream out(filename);
    out << "[";
    size_t current_size = 1;
    bool first = true;

    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist_score(0.0, 1000.0);
    std::uniform_int_distribution<int> dist_id(1, 1000000);

    int i = 0;
    while (current_size < size_mb * 1024 * 1024) {
        if (!first) { out << ","; current_size++; }
        first = false;

        std::string entry = R"({"id":)" + std::to_string(dist_id(rng)) +
                            R"(,"score":)" + std::to_string(dist_score(rng)) +
                            R"(,"name":"Entity_)" + std::to_string(i) + R"("})";
        out << entry;
        current_size += entry.size();
        i++;
    }
    out << "]";
    std::cout << "Generated " << filename << " (" << current_size / (1024*1024) << " MB)" << std::endl;
}

int main() {
    generate_file("large_file.json", 50);
    generate_file("massive_file.json", 100);
    return 0;
}
