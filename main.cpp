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
  template<this_ref list>
  using apply = list::pop_front;
  template<no_arguments>
  using initialize = to_exp_list_t<TL>;
};

template <this_ref list>
using pop_f = typename list::front;

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
using flags_test_o = meta_states_object<default_ops, main_states_f<identity_f>>;
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

using namespace meta_op_bits;



template<u64>
struct my_executor;

template<>
struct my_executor<0> {
  template<class this_object, class other_object>
  using apply = meta_object_invoke<this_object, other_object>;
};

template<>
struct my_executor<1> {
  template<class this_object, class other_object>
  using apply = meta_object_invoke<this_object, meta_invoke<other_object>>;
};

using flags_0 = flags_operator<0>;
using flags_1 = flags_operator<1>;


// ===== meta_object_invoke 概念冲突测试辅助元函数 =====
// （局部类不能含成员模板，元函数必须定义在命名空间作用域）

// MSO 测试用 Fn：无论收到完整对象还是 flags，都把 bit0 置 1 作为观测标记
struct mso_count_f {
  template <class this_flags, class...>
  using apply = typename this_flags::template opOr<0x1>;
};

// (B) 路径测试用 Fn：initialize 只接受带 ::front 的实参（拒绝无参初始化）
struct strict_init_f {
  template <class this_obj, class T>
  using apply = typename this_obj::template push_back<T>;
  template <class TL>
    requires requires { typename TL::front; }
  using initialize = TL;
};

int main(){
  using executor = states_executor<my_executor>;

  constexpr auto mask_of_flags_0 = flags_0::mask<bool, 1> | flags_0::mask<bool, 0>;

  constexpr bool test_uinitialized = is_uninitialized_meta_object<meta_istream<exp_list<int, double, char>>>;

  using result1 = meta_invoke<executor, flags_0, meta_ostream<>, meta_istream<exp_list<int, double, char>>>::type::at<0>;
  using result2 = meta_invoke<executor, flags_1, meta_ostream<>, meta_istream<exp_list<int, double, char>>>::type::at<0>;

  // ================== meta_object_invoke 概念冲突测试 ==================
  using istream_t = meta_istream<exp_list<int, double, char>>;
  using istream_init_t = meta_invoke<istream_t>;   // 已初始化 meta_ret_object
  using mso_t = meta_states_object<0, mso_count_f>;
  using timer_t = to_timer<meta_object<meta_empty_o, replace_f>, 3>;
  using strict_o = meta_object<strict_init_f>;     // 无法无参初始化的未初始化对象

  // --- 概念分类：每个对象种类落入且仅落入预期类别 ---
  static_assert(pure_meta_object_t<meta_ostream<>>);
  static_assert(is_uninitialized_meta_object<istream_t>);
  static_assert(meta_ret_object_t<istream_init_t>);
  static_assert(meta_states_object_t<mso_t>);

  // (C) pure <- pure：投递 OBJ2::type，结果是新的目标元对象
  static_assert(std::is_same_v<
      meta_object_invoke<meta_ostream<>, meta_ostream<exp_list<int>>>,
      meta_object<exp_list<exp_list<int>>, meta_quote::binary<add_f>>>);

  // (D) pure <- ret：投递 OBJ2::ret（istream 的 ret 是 front = int）
  static_assert(std::is_same_v<
      meta_object_invoke<meta_ostream<>, istream_init_t>,
      meta_object<exp_list<int>, meta_quote::binary<add_f>>>);

  // (A) OBJ2 可无参初始化：自动初始化后走 (D)，结果与 (D) 相同
  static_assert(std::is_same_v<
      meta_object_invoke<meta_ostream<>, istream_t>,
      meta_object<exp_list<int>, meta_quote::binary<add_f>>>);

  // (B) OBJ2 无法无参初始化：返回等待初始化参数的元函数
  static_assert(is_meta_function_v<meta_object_invoke<meta_ostream<>, strict_o>>);
  static_assert(std::is_same_v<
      meta_invoke<meta_object_invoke<meta_ostream<>, strict_o>, exp_list<double>>,
      meta_object<exp_list<exp_list<double>>, meta_quote::binary<add_f>>>);

  // (F) pure <- MSO：投递 OBJ2::flags
  static_assert(std::is_same_v<
      meta_object_invoke<meta_ostream<>, mso_t>,
      meta_object<exp_list<flags_operator<0>>, meta_quote::binary<add_f>>>);

  // (G) MSO <- pure / ret：投递完整的 OBJ2 对象本身
  static_assert(meta_object_invoke<mso_t, meta_ostream<exp_list<int>>>::value == 0x1);
  static_assert(meta_object_invoke<mso_t, istream_init_t>::value == 0x1);

  // (H) MSO <- MSO：投递 OBJ2::flags
  static_assert(meta_object_invoke<mso_t, flags_test_o>::value == 0x1);

  // Args... 转发路径（MSO 目标，Fn 变参接收）
  static_assert(meta_object_invoke<mso_t, meta_ostream<exp_list<char>>, double>::value == 0x1);

  // --- meta_timer_object：补上 ::function 后概念成立 ---
  static_assert(meta_object_t<timer_t>);
  static_assert(meta_timer_object_t<timer_t>);
  static_assert(!pure_meta_object_t<timer_t>);   // 不会被 pure 路径抢走
  static_assert(!meta_ret_object_t<timer_t>);

  // (E) pure <- timer：投递 OBJ2::type（payload），结果是新的目标元对象
  static_assert(std::is_same_v<
      meta_object_invoke<meta_ostream<>, timer_t>,
      meta_object<exp_list<meta_empty_o>, meta_quote::binary<add_f>>>);

  // MSO <- timer：走 (G)，投递完整 timer 对象
  static_assert(meta_object_invoke<mso_t, timer_t>::value == 0x1);

}

