#include "Tachyon.hpp"
#include <iostream>
#include <cassert>

void test_unsafe() {
    std::string valid = "{\"key\": \"val\"}";
    Tachyon::Context<false> ctx;
    auto doc = ctx.parse_view(valid.data(), valid.size());
    if (doc["key"].as_string() != "val") {
        std::cerr << "Unsafe mode failed logic" << std::endl;
        exit(1);
    }
}

int main() {
    test_unsafe();
    std::cout << "Unsafe toggle test passed." << std::endl;
    return 0;
}
