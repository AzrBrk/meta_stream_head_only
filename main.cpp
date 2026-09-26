#include <iostream>
#include <type_traits>
#include <cstdint>
#include <tuple>
#include <string>

#include "meta_stream.hpp"
using namespace meta_ios;
using namespace exp_utilities;
using namespace meta_objects;

struct default_t{};

template <std::size_t Pass, std::size_t Indx>
struct selected_f:stream_op<opSkip|opCallIs>
{
  using type       = Idx<0>;
  template <class this_idx, class from_ins>
  using apply      = default_t;
  template <class this_idx, class from_ins>
  using pred       = std::bool_constant<(from_ins::value == Pass)>;
  template <class this_idx, class from_ins>
  using on_changed = Idx<Indx>;
};

struct break_on_change {
  template <class Stream>
  using apply = std::bool_constant<Stream::to::last_changed>;
};

template<std::size_t P, std::size_t I>
using selected_states = meta_make_states<selected_f<P, I>>;

template<std::size_t I>
struct dec_size:
    stream_op<opBreak>
{
  using type       = Idx<I>;
  template <class this_idx, class from_ins>
  using apply      = default_t;
  template <class this_idx, class from_ins>
  using pred       = std::bool_constant<(this_idx::value + 1 > exp_size<from_ins>)>;
  template <class this_idx, class from_ins>
  using on_changed = Idx<this_idx::value - exp_size<from_ins>>;
};

template<std::size_t I, class ...Tp>
decltype(auto) from_tuples_v(Tp &&...tps){
    using pass_recrd_o = typename transfer_until<meta_make_states<dec_size<I>>, meta_istream_list<Tp...>>::to;
    using all_types    = typename meta_all_transfer<meta_jostream<exp_list<>>, meta_istream_list<Tp...>>::to::type;
    constexpr std::size_t passed_count = __builtin_popcount(pass_recrd_o::flags & meta_objects::meta_states_details::FLAGS_LOW_MASK);
    constexpr std::size_t index        = pass_recrd_o::type::value;
    return meta_transfer_until<
                selected_states<passed_count, index>,
                index_sequence_istream<sizeof...(Tp)>,
                break_on_change
    >::for_each_forward([](auto index_stream, auto &&tp){ 
        return std::get<index_stream.value()>(tp);
    }, std::forward<Tp>(tps)...);
}

int main() {
    using tp_list = exp_list<std::tuple<int, std::string>, std::tuple<double, std::string>, std::tuple<char, std::string>>;
    tp_list::at<0> t{10, "hello 0"};
    tp_list::at<1> t1{2.33, "hello 1"};
    tp_list::at<2> t2{'l', "hello 2"};
    std::cout << from_tuples_v<3>(t, t1, t2) << std::endl;
}
