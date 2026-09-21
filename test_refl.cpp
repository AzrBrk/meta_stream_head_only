// Verify the library-side reflection adapter compiles & runs.
// Build: g++ -std=c++26 -freflection test_refl.cpp -o test_refl.exe
#include <iostream>
#include <meta>
#include "meta_stream.hpp"
using namespace meta_ios;

struct Demo { char a; double b; int c; };

int main() {
    meta_for<meta_aligned_iterator,
             meta_istream<reflected_member_types<Demo>::type>>::for_each(
        [](auto stream) {
            std::cout << "offset=" << stream.value()
                      << "  type=" << stream.target_type().name() << "\n";
        });
}
