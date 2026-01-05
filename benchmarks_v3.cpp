#include "Tachyon.hpp"
#include "nlohmann_json.hpp"
#include "simdjson.h"
#include "glaze/glaze.hpp"
#include <chrono>
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <sched.h>
#include <fstream>
#include <sstream>

void pin_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
}

void do_not_optimize(const void* p) {
    asm volatile("" : : "g"(p) : "memory");
}

std::string generate_large(size_t size_mb) {
    std::string s;
    s.reserve(size_mb * 1024 * 1024);
    s += "[";
    for (size_t i = 0; i < 500000; ++i) {
        if (i > 0) s += ",";
        s += R"({"id":)" + std::to_string(i) + R"(,"name":"Item )" + std::to_string(i) + R"(","active":true,"scores":[1,2,3,4,5]})";
        if (s.size() > size_mb * 1024 * 1024) break;
    }
    s += "]";
    return s;
}

std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::stringstream buffer;
    buffer << f.rdbuf();
    return buffer.str();
}

int main() {
    pin_to_core(0);
    std::string large = generate_large(25);

    std::cout << "| Library | Speed (MB/s) |" << std::endl;
    std::cout << "|---|---|" << std::endl;

    // Tachyon
    {
        Tachyon::Document doc;
        doc.parse_view(large.data(), large.size());
        auto start = std::chrono::high_resolution_clock::now();
        for(int i=0; i<100; ++i) {
            doc.parse_view(large.data(), large.size());
            do_not_optimize(doc.tape_data);
        }
        auto end = std::chrono::high_resolution_clock::now();
        double sec = std::chrono::duration<double>(end - start).count();
        std::cout << "| Tachyon | " << (large.size() * 100 / 1024.0 / 1024.0 / sec) << " |" << std::endl;
    }

    // Simdjson
    {
        simdjson::ondemand::parser parser;
        simdjson::padded_string p_data(large);
        auto start = std::chrono::high_resolution_clock::now();
        for(int i=0; i<100; ++i) {
            auto doc = parser.iterate(p_data);
            if(doc.error()) std::cerr << "Err" << std::endl;
        }
        auto end = std::chrono::high_resolution_clock::now();
        double sec = std::chrono::duration<double>(end - start).count();
        std::cout << "| Simdjson | " << (large.size() * 100 / 1024.0 / 1024.0 / sec) << " |" << std::endl;
    }

    // Glaze
    {
        glz::generic v;
        auto start = std::chrono::high_resolution_clock::now();
        for(int i=0; i<100; ++i) {
            if(glz::read_json(v, large)) do_not_optimize(&v);
        }
        auto end = std::chrono::high_resolution_clock::now();
        double sec = std::chrono::duration<double>(end - start).count();
        std::cout << "| Glaze | " << (large.size() * 100 / 1024.0 / 1024.0 / sec) << " |" << std::endl;
    }

    return 0;
}
