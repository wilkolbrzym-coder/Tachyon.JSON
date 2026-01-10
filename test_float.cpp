#include <iostream>
#include <iomanip>
#include <cmath>
#include <array>
#include <charconv>
#include <cstring>

static constexpr int MAX_EXP = 308;
static constexpr std::array<double, MAX_EXP + 1> generate_pow10() {
    std::array<double, MAX_EXP + 1> table{};
    long double v = 1.0;
    for (int i = 0; i <= MAX_EXP; ++i) {
        table[i] = static_cast<double>(v);
        if (i < MAX_EXP) v *= 10.0;
    }
    return table;
}
static constexpr auto pow10_table = generate_pow10();

double parse_custom(uint64_t m, int e) {
    if (e >= 0) return m * pow10_table[e];
    return m / pow10_table[-e];
}

int main() {
    // 1.23e-100
    // m = 123, e = -102
    double val_std = 1.23e-100;
    double val_cust = parse_custom(123, -102);

    std::cout << std::setprecision(20);
    std::cout << "Std:    " << val_std << "\n";
    std::cout << "Custom: " << val_cust << "\n";
    std::cout << "Diff:   " << (val_std - val_cust) << "\n";

    // Canada example: -65.6198888773706
    // m = 656198888773706, e = -13
    double val_can = 65.6198888773706;
    double val_cust_can = parse_custom(656198888773706ULL, -13);
    std::cout << "Canada Std:    " << val_can << "\n";
    std::cout << "Canada Custom: " << val_cust_can << "\n";
    std::cout << "Diff:          " << (val_can - val_cust_can) << "\n";

    return 0;
}
