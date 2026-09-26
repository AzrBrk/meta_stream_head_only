// meta_stream.hpp 真· concat 对比 Benchmark
// 用法: g++ -std=c++23 -O2 -o benchmark_true_concat benchmark_true_concat.cpp && ./benchmark_true_concat

#include <iostream>
#include <tuple>
#include <chrono>
#include <string>
#include <iomanip>
#include <utility>

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

// ============== 真· meta_stream 版本的 my_tuple_cat ==============
// 内部就是遍历所有索引，调用 my_get<I>，把结果收集到 tuple 里
template <class... Tuples>
auto my_tuple_cat(Tuples&&... tps) {
    // 先编译期算总元素数
    using all_transfer = meta_all_transfer<
        meta_jostream<exp_list<>>,
        meta_istream_list<std::remove_cvref_t<Tuples>...>
    >;
    using cat_list = typename all_transfer::to_t;
    constexpr std::size_t total = exp_size<cat_list>;
    
    // 然后遍历每个索引，调用 my_get<I>，收集到 tuple 里
    auto build = [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        return std::tuple<decltype(meta_my_get<Is>(std::forward<Tuples>(tps)...))...>{
            meta_my_get<Is>(std::forward<Tuples>(tps)...)...
        };
    };
    
    return build(std::make_index_sequence<total>{});
}

using clk = std::chrono::high_resolution_clock;
using us = std::chrono::microseconds;

int main() {
    std::cout << "=== meta_stream.hpp 真· concat 对比 Benchmark ===" << std::endl;
    std::cout << "Compiler: " << __VERSION__ << std::endl;
    std::cout << std::endl;
    
    std::tuple<int, double, std::string> t1{1, 2.5, "hello"};
    std::tuple<int, int, int> t2{10, 20, 30};
    std::tuple<double, double> t3{1.1, 2.2};
    
    const int N = 100000;
    
    std::cout << "--- tuple concat 运行时开销 ---" << std::endl;
    
    // 标准库 tuple_cat
    {
        auto start = clk::now();
        volatile std::size_t sink = 0;
        for (int i = 0; i < N; i++) {
            auto cat = std::tuple_cat(t1, t2, t3);
            sink = std::tuple_size_v<decltype(cat)>;
        }
        auto end = clk::now();
        auto dur = std::chrono::duration_cast<us>(end - start).count();
        std::cout << std::setw(35) << std::left << "std::tuple_cat" << std::setw(10) << dur << " us" << std::endl;
    }
    
    // meta_stream my_tuple_cat（真·自己的逻辑）
    {
        auto start = clk::now();
        volatile std::size_t sink = 0;
        for (int i = 0; i < N; i++) {
            auto cat = my_tuple_cat(t1, t2, t3);
            sink = std::tuple_size_v<decltype(cat)>;
        }
        auto end = clk::now();
        auto dur = std::chrono::duration_cast<us>(end - start).count();
        std::cout << std::setw(35) << std::left << "my_tuple_cat (meta_stream)" << std::setw(10) << dur << " us" << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "--- concat 后 get 元素 ---" << std::endl;
    
    {
        auto start = clk::now();
        volatile double sink = 0;
        for (int i = 0; i < N; i++) {
            auto cat = std::tuple_cat(t1, t2, t3);
            sink = std::get<0>(cat) + std::get<3>(cat) + std::get<5>(cat);
        }
        auto end = clk::now();
        auto dur = std::chrono::duration_cast<us>(end - start).count();
        std::cout << std::setw(35) << std::left << "std:: concat + 3x get" << std::setw(10) << dur << " us" << std::endl;
    }
    
    {
        auto start = clk::now();
        volatile double sink = 0;
        for (int i = 0; i < N; i++) {
            auto cat = my_tuple_cat(t1, t2, t3);
            sink = std::get<0>(cat) + std::get<3>(cat) + std::get<5>(cat);
        }
        auto end = clk::now();
        auto dur = std::chrono::duration_cast<us>(end - start).count();
        std::cout << std::setw(35) << std::left << "my_tuple_cat + 3x get" << std::setw(10) << dur << " us" << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "--- 单次跨 tuple get（不 concat）---" << std::endl;
    
    {
        auto start = clk::now();
        volatile double sink = 0;
        for (int i = 0; i < N; i++) {
            sink = meta_my_get<5>(t1, t2, t3);
        }
        auto end = clk::now();
        auto dur = std::chrono::duration_cast<us>(end - start).count();
        std::cout << std::setw(35) << std::left << "meta_my_get<5> (不 concat)" << std::setw(10) << dur << " us" << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "=== 测试完成 ===" << std::endl;
    std::cout << "迭代次数: " << N << std::endl;
    std::cout << "tuple 数量: 3, 元素总数: 7" << std::endl;
    
    return 0;
}
