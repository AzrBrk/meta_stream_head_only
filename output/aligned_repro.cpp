// 复现 type_safe_aligned_iterator 的核心算法（不靠 pipe，不写任何偏特化）
//
//   1) 类型位掩码 : meta_looper + 自己写的 condition + 类 flags_operate 的 states 对象
//   2) 对齐偏移表 : 同一个 meta_looper + 一个"把每一步的迭代器状态收进列表"的 ostream
//   3) 运行期     : 编译期常量(步号/偏移/类型) × 运行期 index 的遍历 + 类型安全校验
//
// 全程只有：using 别名链、普通结构体上的成员模板、以及 meta_looper 那一层递归。
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "meta_iostream.hpp"
#include "meta_objects.hpp"

using namespace meta_objects;
using namespace exp_utilities;
using namespace meta_loop;
using namespace literal_types;
using meta_objects::meta_op_bits::u8;
using meta_objects::meta_op_bits::u64;

// ============================================================================
// 1) 位掩码：bit i 置位 ⇔ Ty[i] 就是 T
//    布局是自己定的：bit 0..54 = 掩码位，bit 55 = 总闸，bit 56..63 = 步号
// ============================================================================
inline constexpr u64 mask_continue = u64{1} << 55;
inline constexpr std::size_t mask_index_shift = 56;

template <class T>
struct same_type_flags_f {
  template <class this_flags, class gen>
  struct apply {
    static constexpr bool at_end =
        std::is_same_v<meta_produce_t<gen>, end_of_list>;
    static constexpr u64 value = [] {
      if constexpr (uninitialized_meta_object_t<gen>) {
        return this_flags::value;  // 还没初始化：本步什么都不记
      } else {
        static constexpr bool hit = std::is_same_v<T, meta_produce_t<gen>>;
        static constexpr std::size_t step = static_cast<std::size_t>(
            this_flags::template get<u8, mask_index_shift>);
        using next = typename this_flags
            ::template set_if<hit, bool, step, true>  // 命中 → 记一位
            ::inc<u8, mask_index_shift>               // 记一步
            ::template opToggle_if<mask_continue, at_end>;  // 到头 → 关总闸
        return next::value;
      }
    }();
  };
};

struct mask_stop_f {  // 条件就是"看我自己那套布局里的总闸"
  template <class this_flags>
  struct apply {
    static constexpr bool value = this_flags::template test<mask_continue>;
  };
};

using mask_condition = make_looper_condition<mask_stop_f>;

template <class T>
using same_type_flags_o =
    meta_states_object<mask_continue, same_type_flags_f<T>>;

template <class T, class... Ty>
inline constexpr u64 type_mask =
    meta_looper_t<mask_condition, same_type_flags_o<T>,
                  meta_iostream::meta_istream_list<Ty...>>::value &
    ~(u64{0xFF} << mask_index_shift);

// ============================================================================
// 2) 对齐偏移表
//    payload = exp_list<seek_to<偏移, 类型>...>，长度就正好是走过的步数
//    第一格用元对象自带的 initialize 现场造出来（不需要占位符，也不用事后再丢）
// ============================================================================
struct collect_seek_f {
  // 初始化：这一步读到的类型就是第一格 —— invoke 元对象本身，别去碰它的 function
  template <class from_ins>
    requires(!no_arguments<from_ins>)
  using initialize = exp_list<
      typename meta_invoke<meta_iostream::meta_aligned_iterator, from_ins>::type>;

  // 之后每一步：把上一格的那个迭代器"重新当成元对象"来推进
  //   meta_set<back> 造出"绑定在上一格上的迭代器"，再 invoke 它读这一步的类型
  template <class this_list, class from_ins>
  using apply = typename this_list::template push_back<
      typename meta_invoke<
          typename meta_iostream::meta_aligned_iterator::template meta_set<
              typename this_list::back>,
          from_ins>::type>;
};

using seek_history_o = meta_object<collect_seek_f>;  // 初始化器版（没有 payload）

// 条件：payload 长度到 N 就停（payload 自己带着"走了几步"）
// exp_utilities 里已经有一整套 length_* 条件函数对象，直接用 length_less
template <std::size_t N>
using stop_after_steps = make_looper_condition<length_less<N>>;

// 一份描述，两边用：编译期取 ::type，运行期 ::for_each
template <class... Ty>
using offsets_run = meta_looper<stop_after_steps<sizeof...(Ty)>, seek_history_o,
                                meta_iostream::meta_istream_list<Ty...>>;

// 每一步的状态就是其中一个 seek_to
template <class... Ty>
using offsets_t = typename meta_invoke<offsets_run<Ty...>>::type;

template <class... Ty>
constexpr auto offset_table() {
  return to_meta_array_t<offsets_t<Ty...>>::array();
}

template <class... Ty>
constexpr std::size_t total_bytes =
    offsets_t<Ty...>::back::advance_t::value;

// ============================================================================
// 3) 类型安全的对齐存储 + 运行期遍历（就是 type_safe_aligned_iterator 的骨架）
// ============================================================================
template <class... Ty>
class aligned_store {
  static_assert(sizeof...(Ty) > 0, "至少要一个类型");
  static constexpr auto offsets = offset_table<Ty...>();
  static constexpr std::size_t total = total_bytes<Ty...>;

  std::byte buffer_[total == 0 ? 1 : total]{};
  std::size_t index_ = 0;

 public:
  std::byte* base() { return buffer_; }
  std::size_t index() const { return index_; }
  void seek(std::size_t i) { index_ = i; }
  bool at_end() const { return index_ >= sizeof...(Ty); }

  // 运行期 index × 编译期掩码：T 出现在哪些位置是编译期算好的
  template <class T>
  bool is_safe() const {
    static_assert(exp_try_find<T, exp_list<Ty...>>::value,
                  "T 必须在这组类型里");
    if (at_end()) return false;
    return ((type_mask<T, Ty...> >> index_) & 1) != 0;
  }

  template <class T>
  void emplace(T&& val) {
    using value_type = std::remove_cvref_t<T>;
    if (!is_safe<value_type>()) {
      throw std::out_of_range("aligned_store: 这个位置不是这个类型");
    }
    ::new (static_cast<void*>(buffer_ + offsets[index_]))
        value_type(std::forward<T>(val));
  }

  template <class T>
  T& get() {
    if (!is_safe<T>()) throw std::out_of_range("aligned_store: 取错类型");
    return *reinterpret_cast<T*>(buffer_ + offsets[index_]);
  }

  void next() { ++index_; }
};

// 运行期遍历：一份描述，编译期常量与运行期值混在普通 if 里
template <class... Ty>
void print_at(std::ostream& os, aligned_store<Ty...>& store) {
  meta_invoke<offsets_run<Ty...>>::for_each([&](auto stage) {
    using step_seek = typename decltype(stage)::back;  // 本步的 seek_to
    using current_type = typename step_seek::type;
    // payload 长度 = 步数 → 本步步号是 length - 1
    constexpr std::size_t step = exp_size<decltype(stage)> - 1;
    if (step == store.index()) {  // 普通 if：编译期常量 × 运行期值
      os << sizeof(current_type) << "@" << step_seek::value << "="
         << store.template get<current_type>();
    }
  });
}

// ============================================================================
// 检查
// ============================================================================
using L = exp_list<char, double, int, double>;

static_assert(type_mask<double, char, double, int, double> == 0b01010ull,
              "位掩码：double 在 1、3 位");
static_assert(type_mask<char, char, double, int, double> == 0b00001ull);
static_assert(type_mask<long, char, double, int, double> == 0ull);

static_assert(offset_table<char, double, int, double>()[0] == 0);
static_assert(offset_table<char, double, int, double>()[1] == 8);   // double 对齐到 8
static_assert(offset_table<char, double, int, double>()[2] == 16);  // int 跟着 8 后面
static_assert(offset_table<char, double, int, double>()[3] == 24);  // 再对齐到 8
static_assert(total_bytes<char, double, int, double> == 32);
static_assert(offset_table<char, int>()[1] == 4);
static_assert(total_bytes<char, int> == 8);
static_assert(exp_size<offsets_t<char, int>> == 2);

int main() {
  aligned_store<char, double, int, double> store;

  store.emplace<char>('a');
  store.next();
  store.emplace<double>(3.5);
  store.next();
  store.emplace<int>(42);
  store.next();
  store.emplace<double>(7.25);

  for (std::size_t i = 0; i < 4; ++i) {
    store.seek(i);
    print_at(std::cout, store);
    std::cout << "\n";
  }

  // 类型安全：位置 0 是 char，取 double 应该被拦住
  store.seek(0);
  try {
    (void)store.get<double>();
  } catch (const std::exception& e) {
    std::cout << "caught: " << e.what() << "\n";
  }
  return 0;
}
