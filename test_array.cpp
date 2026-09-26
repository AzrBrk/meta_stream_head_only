#include <iostream>
#include <type_traits>
#include <cstdint>
#include <array>

#include "meta_stream.hpp"
using namespace exp_utilities;

// Custom index type with size_t value (not Idx)
template <std::size_t N>
struct MyIndex {
    static constexpr std::size_t value = N;
};

// Another custom type with value of convertible type
template <int N>
struct IntIndex {
    static constexpr int value = N;  // int, convertible to size_t
};

// Custom wrapper (not exp_list)
template <class... Ts>
struct MyWrapper {};

int main() {
    // 1. Original: exp_list<Idx<...>>
    using L1 = exp_list<Idx<1>, Idx<3>, Idx<5>>;
    using A1 = to_meta_array_t<L1>;
    std::cout << "1. exp_list<Idx...>: ";
    auto a1 = A1::array();
    for (auto v : a1) std::cout << v << ' ';
    std::cout << std::endl;

    // 2. exp_list with custom index types
    using L2 = exp_list<MyIndex<2>, MyIndex<4>, MyIndex<6>>;
    using A2 = to_meta_array_t<L2>;
    std::cout << "2. exp_list<MyIndex...>: ";
    auto a2 = A2::array();
    for (auto v : a2) std::cout << v << ' ';
    std::cout << std::endl;

    // 3. Custom wrapper with Idx
    using L3 = MyWrapper<Idx<10>, Idx<20>>;
    using A3 = to_meta_array_t<L3>;
    std::cout << "3. MyWrapper<Idx...>: ";
    auto a3 = A3::array();
    for (auto v : a3) std::cout << v << ' ';
    std::cout << std::endl;

    // 4. Custom wrapper with custom types
    using L4 = MyWrapper<MyIndex<7>, MyIndex<8>, MyIndex<9>>;
    using A4 = to_meta_array_t<L4>;
    std::cout << "4. MyWrapper<MyIndex...>: ";
    auto a4 = A4::array();
    for (auto v : a4) std::cout << v << ' ';
    std::cout << std::endl;

    // 5. Mixed types all with value
    using L5 = exp_list<Idx<1>, MyIndex<2>, std::integral_constant<std::size_t, 3>>;
    using A5 = to_meta_array_t<L5>;
    std::cout << "5. Mixed value types: ";
    auto a5 = A5::array();
    for (auto v : a5) std::cout << v << ' ';
    std::cout << std::endl;

    // 6. int value convertible to size_t
    using L6 = exp_list<IntIndex<11>, IntIndex<22>>;
    using A6 = to_meta_array_t<L6>;
    std::cout << "6. int value (convertible): ";
    auto a6 = A6::array();
    for (auto v : a6) std::cout << v << ' ';
    std::cout << std::endl;

    // 7. Empty list
    using L7 = exp_list<>;
    using A7 = to_meta_array_t<L7>;
    std::cout << "7. Empty list, length: " << A7::length << std::endl;

    std::cout << "\nAll positive tests passed!" << std::endl;
    return 0;
}
