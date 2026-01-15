#include <iostream>
#include <string>
#include <vector>
#include <cassert>

// Drop-in: Include tachyon, assume nlohmann namespace exists
#include "tachyon.hpp"

// Test Struct
struct Person {
    std::string name;
    int age;
};

// ADL Serializer
void to_json(nlohmann::json& j, const Person& p) {
    j = nlohmann::json::object();
    j["name"] = p.name;
    j["age"] = p.age;
}

void from_json(const nlohmann::json& j, Person& p) {
    p.name = j["name"].get<std::string>();
    p.age = j["age"].get<int>();
}

int main() {
    // 1. Basic Usage
    nlohmann::json j;
    j["pi"] = 3.141;
    j["happy"] = true;
    j["name"] = "Niels";

    std::string s = j.dump();
    assert(s.find("Niels") != std::string::npos);

    // 2. Struct Conversion
    Person p{"Alice", 30};
    nlohmann::json j_p = p; // Implicit conversion via to_json?
    // Nlohmann supports implicit conversion if `to_json` is found?
    // Usually `j = p;` or `json j = p;`.
    // My implementation constructor uses `to_json`.

    assert(j_p["name"] == "Alice");
    assert(j_p["age"] == 30);

    // 3. Round trip
    Person p2 = j_p.get<Person>();
    assert(p2.name == "Alice");
    assert(p2.age == 30);

    // 4. Items loop
    for (auto item : j.items()) {
        std::cout << item.key() << ": " << item.value() << "\n";
    }

    std::cout << "Compatibility Test Passed!" << std::endl;
    return 0;
}
