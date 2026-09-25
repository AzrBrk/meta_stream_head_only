// meta_stream.hpp 性能 Benchmark
// 用法: g++ -std=c++23 -O2 -o benchmark benchmark.cpp && ./benchmark

#include <iostream>
#include <tuple>
#include <chrono>
#include <string>
#include <vector>
#include <iomanip>

#include "meta_stream.hpp"
using namespace meta_ios;
using namespace exp_utilities;
using namespace meta_objects;
using namespace meta_ios::io_stream_transform_details;

template <std::size_t N>
struct IdxWithBreak : Idx<N> {
    static constexpr std::uint64_t opr_code = stream_op_bits::OP_DEFAULT;
};

struct dec_size {
  template <class this_idx, class from_ins>
  using apply = Idx<this_idx::value - exp_size<from_ins>>;
  template <class this_idx, class from_ins>
  using pred = std::bool_constant<(this_idx::value + 1 > exp_size<from_ins>)>;
  template <class this_idx, class from_ins>
  using on_changed = Idx<this_idx::value - exp_size<from_ins>>;
};

template <std::size_t Pass, std::size_t Indx>
struct selected_f {
  template <class this_idx, class from_ins>
  using apply = this_idx;
  template <class this_idx, class from_ins>
  using pred = std::bool_constant<(from_ins::value == Pass)>;
  template <class this_idx, class from_ins>
  using on_changed = Idx<Indx>;
};

struct break_on_change {
  template <class Stream>
  using apply = std::bool_constant<Stream::to::last_changed>;
};

template<std::size_t pass_c, std::size_t target_idx>
auto meta_get_selected(auto && ...tp){
    using selected = selected_f<pass_c, target_idx>;
    return meta_transfer_until<
      meta_states_object<Idx<0>, selected, meta_quote::binary<selected::template pred>>,
      index_sequence_istream<sizeof...(tp)>,
      break_on_change
    >::for_each_forward(
        [](auto index_stream, auto&& t) -> decltype(auto) {
            return std::get<index_stream.value()>(t);
        },
        std::forward<decltype(tp)>(tp)...
    );
}

template<std::size_t I>
decltype(auto) meta_my_get(auto && ...tps){
    using pass_o = transfer_until<
        meta_states_object<IdxWithBreak<I>, dec_size, meta_quote::binary<dec_size::pred>>, 
        meta_istream_list<std::remove_cvref_t<decltype(tps)>...>
    >::to;
    
    constexpr std::size_t pass_cnt = __builtin_popcount(pass_o::flags);
    constexpr std::size_t target_index = pass_o::type::value; 
    
    return meta_get_selected<pass_cnt, target_index>(std::forward<decltype(tps)>(tps)...);
}

using clk = std::chrono::high_resolution_clock;
using us = std::chrono::microseconds;

int main() {
    std::cout << "=== meta_stream.hpp Benchmark ===" << std::endl;
    std::cout << "Compiler: " << __VERSION__ << std::endl;
    std::cout << std::endl;
    
    std::tuple<int, double, std::string> t1{1, 2.5, "hello"};
    std::tuple<int, int, int> t2{10, 20, 30};
    std::tuple<double, double> t3{1.1, 2.2};
    
    const int N = 100000;
    
    std::cout << "--- 跨 tuple 按索引查找 (I=5) ---" << std::endl;
    
    // 标准库：先 concat 再 get
    {
        auto start = clk::now();
        volatile double sink = 0;
        for (int i = 0; i < N; i++) {
            auto cat = std::tuple_cat(t1, t2, t3);
            sink = std::get<5>(cat);
        }
        auto end = clk::now();
        auto dur = std::chrono::duration_cast<us>(end - start).count();
        std::cout << std::setw(30) << std::left << "std::tuple_cat + get" << std::setw(10) << dur << " us" << std::endl;
    }
    
    // meta_stream 实现
    {
        auto start = clk::now();
        volatile double sink = 0;
        for (int i = 0; i < N; i++) {
            sink = meta_my_get<5>(t1, t2, t3);
        }
        auto end = clk::now();
        auto dur = std::chrono::duration_cast<us>(end - start).count();
        std::cout << std::setw(30) << std::left << "meta_stream my_get" << std::setw(10) << dur << " us" << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "--- 编译期大小查询 ---" << std::endl;
    
    {
        auto start = clk::now();
        volatile std::size_t sink = 0;
        for (int i = 0; i < N; i++) {
            sink = std::tuple_size_v<decltype(std::tuple_cat(t1, t2, t3))>;
        }
        auto end = clk::now();
        auto dur = std::chrono::duration_cast<us>(end - start).count();
        std::cout << std::setw(30) << std::left << "std::tuple_size_v" << std::setw(10) << dur << " us" << std::endl;
    }
    
    {
        auto start = clk::now();
        volatile std::size_t sink = 0;
        for (int i = 0; i < N; i++) {
            using result = meta_all_transfer<
                meta_jostream<exp_list<>>,
                meta_istream_list<std::tuple<int, double, std::string>, std::tuple<int, int, int>, std::tuple<double, double>>
            >;
            sink = exp_size<typename result::to_t>;
        }
        auto end = clk::now();
        auto dur = std::chrono::duration_cast<us>(end - start).count();
        std::cout << std::setw(30) << std::left << "meta_all_transfer exp_size" << std::setw(10) << dur << " us" << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "--- 多次查找（不同索引）---" << std::endl;
    
    {
        auto start = clk::now();
        volatile int sink = 0;
        for (int i = 0; i < N; i++) {
            auto cat = std::tuple_cat(t1, t2, t3);
            sink = std::get<0>(cat) + std::get<3>(cat) + std::get<5>(cat);
        }
        auto end = clk::now();
        auto dur = std::chrono::duration_cast<us>(end - start).count();
        std::cout << std::setw(30) << std::left << "std:: 3x get from cat" << std::setw(10) << dur << " us" << std::endl;
    }
    
    {
        auto start = clk::now();
        volatile int sink = 0;
        for (int i = 0; i < N; i++) {
            sink = meta_my_get<0>(t1, t2, t3) 
                 + meta_my_get<3>(t1, t2, t3)
                 + meta_my_get<5>(t1, t2, t3);
        }
        auto end = clk::now();
        auto dur = std::chrono::duration_cast<us>(end - start).count();
        std::cout << std::setw(30) << std::left << "meta_ 3x my_get" << std::setw(10) << dur << " us" << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "=== 测试完成 ===" << std::endl;
    std::cout << "迭代次数: " << N << std::endl;
    std::cout << "tuple 数量: 3, 元素总数: 7" << std::endl;
    
    return 0;
}
