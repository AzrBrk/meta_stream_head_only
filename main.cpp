#include <iostream>
#include <type_traits>
#include <cstdint>
#include <tuple>
#include <string>

#include "meta_stream.hpp"
using namespace meta_ios;
using namespace exp_utilities;
using namespace meta_objects;

struct states_replace:stream_op<opSkip | opCallIs>{
    using type = char;
    template<class this_type, class from_ins>
    using apply = this_type;
    template<class type_type, class from_ins>
    using on_changed = from_ins;
    template<class this_type, class from_ins>
    using pred = std::bool_constant<(sizeof(this_type)<=sizeof(from_ins))>;  
};

int main() {
    meta_transfer_until<
        meta_make_states<states_replace>, meta_istream_list<char, double, char, int, double, std::string>
        >::for_each([](auto stream){
            std::cout << stream.target_type().name() << std::endl;
        });
}
