#include"meta_stream.hpp"
#include<iostream>

using namespace meta_ios;

int main() {
    using L = transfer<1, meta_ostream<exp_list<>>, meta_istream_list<int, double, char>>::to::type;

    std::cout << typeid(L).name();
}