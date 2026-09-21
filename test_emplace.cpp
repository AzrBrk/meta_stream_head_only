// Verify the rvalue emplace overload (with std::move) compiles and works.
// Build: g++ -std=c++23 test_emplace.cpp -o test_emplace.exe
#include <cstddef>
#include <iostream>
#include <string>
#include "meta_stream.hpp"
using namespace meta_ios;

int main() {
    alignas(16) std::byte buf[128]{};

    using seek_int = meta_aligned_iterator_details::seek_to<0, int>;
    seek_int s;
    int lval = 42;
    s.emplace(buf, lval);            // const& overload
    s.emplace(buf, std::move(lval)); // && overload (std::move, no copy)
    s.emplace(buf, 7);               // rvalue literal -> && overload

    using seek_str = meta_aligned_iterator_details::seek_to<0, std::string>;
    seek_str ss;
    ss.emplace(buf, std::string("hello")); // rvalue string, must move into place

    std::cout << "int at offset " << seek_int::value << " = " << s.get(buf) << "\n";
    std::cout << "string at offset " << seek_str::value << " = " << ss.get(buf) << "\n";
    return 0;
}
