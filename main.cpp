#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <utility>
#include <vector>

#include"meta_objects.hpp"

using namespace meta_objects;
using namespace exp_utilities;
using namespace meta_quote;
using namespace meta_loop;

using meta_length_cond_o =
    meta_object<void, length_is_not<4>>;

template<class TL>
struct list_dec{
  template<class this_list>
  using apply = this_list::pop_front;
  template<class...>
  using initialize = to_exp_list_t<TL>;
};

template <class this_list>
using pop_f = typename this_list::front;

template <class TL>
using meta_basic_istream =
    meta_ret_object<list_dec<TL>, unary<pop_f>>;
 // namespace meta_basic_istream_detail

 template<class TL>
 using meta_istream = meta_basic_istream<TL>;



template <class none_list_type, class T>
struct add_impl {
  using type = exp_utilities::literal_types::no_exist_type;
};

template <template <class...> class L, class T, class... Tys>
struct add_impl<L<Tys...>, T> {
  using type = L<Tys..., T>;
};

template <class this_list, class T>
using add_f = typename add_impl<this_list, T>::type;

template <class TL = exp_list<>>
using meta_basic_ostream = meta_object<TL, binary<add_f>>;

template<class TL = exp_list<>>
using meta_ostream = meta_basic_ostream<TL>;

namespace timer_condition_details {
struct timer_receiver {
  template <class timer, class...>
  struct apply : timer {};
};
};
using meta_timer_cond_o =
    meta_object<void, timer_condition_details::timer_receiver>;
  

using namespace meta_op_bits;
using namespace meta_loop::meta_looper_detail::states_based_condition_details;

// 条件元对象：Fn 什么都不做（identity_f），耗尽终止由 slot0_states_f 自动检测
// （生成器产出 end_of_list 时自动清 G_CONTINUE）。
using flags_test_o = meta_states_object<flags_operator<default_ops>, main_states_f<identity_f>>;
struct replace_f
{
  template <class this_obj, class T>
  using apply = T;  // 用生成器产出替换 state
  template<class from_gen>
  using initialize = from_gen;
};
template<class TL>
void print_list(){
  if constexpr(exp_utilities::literal_types::no_exist<TL>) return;
  using timer = to_timer<meta_object<meta_empty_o, replace_f>, exp_size<TL>>;
  meta_invoke<meta_looper<
    meta_timer_cond_o,
    timer,
    meta_basic_istream<TL>
    >
  >::for_each([](auto t){
    std::cout << typeid(t).name() << ' ';
  });
  std::cout << std::endl;
}

namespace meta_aligned_iterator_details {
template <std::size_t start, class from_ins>
struct seek_to {
  static constexpr std::size_t align = alignof(from_ins);
  // O(1): round start up to the next multiple of align.
  // alignof is always a power of 2, so this bit math replaces the
  // old per-byte scan via transfer_until + index_istream.
  static constexpr std::size_t value = (start + align - 1) & ~(align - 1);
  using advance_t =
      std::integral_constant<std::size_t, value + sizeof(from_ins)>;
  using type = from_ins;
  void emplace(std::byte* ptr, type const& val) { new (ptr + value) type{val}; }
  void emplace(std::byte* ptr, type&& val) {
    new (ptr + value) type{std::move(val)};
  }
  void destroy(std::byte* ptr) {
    std::destroy_at(reinterpret_cast<type*>(ptr + value));
  }
  std::byte* address_of(std::byte* base_ptr) const { return base_ptr + value; }
  type& get(std::byte* ptr) { return *reinterpret_cast<type*>(ptr + value); }
  type const& c_get(const std::byte* ptr) const {
    return *reinterpret_cast<const type*>(ptr + value);
  }
};
struct advance_f {
  template <class this_seek, class from_is>
  using apply = seek_to<this_seek::advance_t::value, from_is>;
  template <class from_is>
  using initialize = seek_to<0, from_is>;
};
}  // namespace meta_aligned_iterator_details
/// <summary>
/// for meta_aligned_iterator, it is a meta_ostream_t that can seek to an
/// aligned address for a specific type in a byte stream, and it can also
/// advance to the next aligned address for the next type.
/// </summary>
using meta_aligned_iterator = meta_object<meta_aligned_iterator_details::advance_f>;


int main(){
  int i = 0;
  using loop_result = meta_invoke<
    meta_looper<
      flags_test_o,                                        // 条件：meta_states_object
      meta_basic_ostream<>,                                // 接收器 state
      meta_basic_istream<exp_list<int, double, char, long>> // 生成器
    >
  >;
  loop_result::for_each([&]<class T>(T t){
    if constexpr (requires { typename T::state; }) {
      // G_PACK：stage 是 loop_pack
      std::cout << "pack idx=" << T::index
                << "  state_length=" << T::state::type::length
                << "  cache=" << typeid(typename T::cache).name()
                << "  times=" << ++i << std::endl;
    } else if constexpr (requires { T::length; }) {
      std::cout << "ostream length=" << T::length << "  times=" << ++i << std::endl;
    } else {
      std::cout << "stage  times=" << ++i << std::endl;
    }
    return i;
  });
  std::cout << "final i=" << i << std::endl;

  // 最终 type 是 loop_pack（G_PACK）。取其接收器累积列表 state::type 打印。
  using result_list = typename loop_result::type::state::type;
  std::cout << "state list: ";
  print_list<result_list>();

}

