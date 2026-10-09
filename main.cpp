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

namespace timer_condition_details {
struct timer_receiver {
  template <class timer, class...>
  struct apply : timer {};
};
};
using meta_timer_cond_o =
    meta_object<void, timer_condition_details::timer_receiver>;
  

using namespace meta_op_bits;

// skip_at<I>：当 flags 的当前索引等于 I 时，关闭回调并关闭 slot1 的 apply；
// 当索引等于 I+1 时恢复。不含 inc_index_f（由 meta_states_object 默认追加）。
template<u64 I>
struct skip_at{
  template<class flags_t>
  using apply = flags_fold<flags_t,
      deact_if_f<G_NOT_SKIP, (flags_t::value & D_INDEX_MASK) == I>,
      deact_if_f<(static_cast<u64>(S_APPLY) << SLOT1_SHIFT), (flags_t::value & D_INDEX_MASK) == I>,
      set_if_f<G_NOT_SKIP, (flags_t::value & D_INDEX_MASK) == I + 1>,
      set_if_f<(static_cast<u64>(S_APPLY) << SLOT1_SHIFT), (flags_t::value & D_INDEX_MASK) == I + 1>
    >;
};

// list 专用 probe：meta_basic_istream 的 ::type 是 exp_list。
// 耗尽判定：::type 为空，或本轮产出（::ret）已是 end_of_list。
// 命中时清 G_CONTINUE 且关 slot1（不把 end_of_list 喂给接收器）。
struct list_probe_f{
  template<class T>
  using apply = T;  // 满足 meta_function_t 约束（占位）

  template<class MOBJ, class = void>
  struct detect : std::false_type {};  // 无 ::type：未耗尽（initializer 形态）
  template<class MOBJ>
  struct detect<MOBJ, std::void_t<typename MOBJ::type>>
      : std::bool_constant<
            length_equal<typename MOBJ::type, 0> ||
            std::is_same_v<typename MOBJ::ret, exp_utilities::literal_types::end_of_list>> {};

  // 耗尽：清 G_CONTINUE（终止）+ 关 slot1 S_APPLY（不喂 end_of_list）
  template<class MOBJ>
  using step = deact_if_f<
      G_CONTINUE | (static_cast<u64>(S_APPLY) << SLOT1_SHIFT),
      detect<MOBJ>::value>;
};

// meta_states_object 直接作为条件元对象：list_probe_f 探测生成器耗尽，
// skip_at<2> 在第 2 步关闭接收器 apply（跳过）并关回调，第 3 步恢复。
using flags_test_o = meta_states_object<flags_operator<default_ops>, list_probe_f, skip_at<2>>;
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

  // 最终 type（G_PACK 下为 loop_pack）。取其接收器累积列表 state::type 打印。
  using result_list = typename loop_result::type::state::type;
  std::cout << "state list: ";
  print_list<result_list>();

}

