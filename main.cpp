#include <iostream>
#include <type_traits>
#include <cstdint>
#include <tuple>

#include "meta_stream.hpp"
using namespace meta_ios;
using namespace exp_utilities;
using namespace meta_objects;

// 测试 1：stream_op 基类
struct MyType : stream_op<opSkip | opCallIs | opBreak> {
    using type = int;
};

static_assert(MyType::opr_code == opSkip | opCallIs | opBreak, "opr_code should match");

// 测试 2：默认参数
struct DefaultType : stream_op<> {};
static_assert(DefaultType::opr_code == OP_DEFAULT, "default should be OP_DEFAULT");

struct states_replace:stream_op<opSkip | opCallIs | opBreak>{
    using type = char;
    template<class this_type, class from_ins>
    using apply = this_type;
    template<class type_type, class from_ins>
    using on_changed = from_ins;
    template<class this_type, class from_ins>
    using pred = std::bool_constant<(sizeof(this_type)<sizeof(from_ins))>;  
};


int main() {
    meta_transfer_until<
        meta_make_states<states_replace>, meta_istream_list<char, double, int>
        >::for_each([](auto stream){
            std::cout << stream.target_type().name() << std::endl;
        });
}

