#pragma once

#include <bit>

#include "meta_invoke_protocols.hpp"

namespace meta_objects {

using namespace meta_invoke_protocols;

namespace meta_objects_details {
struct meta_empty {
  static constexpr int value = 0;
};
struct meta_empty_fn {
  template <class T, class...>
  using apply = T;
};
// Unlike meta_empty, which is a valid payload used by meta_empty_o, this tag
// represents an object that has not yet been initialized. Do not inspect it as
// a concrete value or apply operations that require a complete payload type.
struct meta_uninitialized {};
}  // namespace meta_objects_details
namespace meta_objects_functions_details{
  struct no_arguments_{};
  struct not_this{};
};


//suggest function use no arguments
template<class T>
concept no_arguments = std::is_same_v<T, meta_objects_functions_details::no_arguments_>;

template<class T>
concept this_ref = !std::is_same_v<T, meta_objects_functions_details::not_this>;

namespace initialize_details {


template<class F>
concept no_arguments_initializable = requires { typename F::template initialize<meta_objects_functions_details::no_arguments_>; };

template <template <class...> class initializer_shape>
struct meta_initializer_container {};

template <class F>
concept has_initializer = requires{ typename meta_initializer_container<F::template initialize>; };

template<class F>
concept has_ret = requires{ typename meta_initializer_container<F::template apply_ret>; };

template<class F>
concept object_initializer = has_initializer<F> && is_meta_function_v<F> && !has_ret<F>;

template<class F>
concept ret_object_initializer = has_initializer<F> && is_meta_function_v<F> && has_ret<F>;

template <class F>
struct initialized {
  template <class OBJ, class... Args>
  using apply = meta_invoke<F, OBJ, Args...>;
};

template <class F, class... Args>
struct initialize_t {
  using type = typename F::template initialize<Args...>;
};

//provide no arguments if the initializer can be invoked with no arguments
template<no_arguments_initializable F, class... Args>
struct initialize_t<F, Args...> {
  using type = typename F::template initialize<meta_objects_functions_details::no_arguments_>;
};
template<class F, class... Args>
using initialize = typename initialize_t<F, Args...>::type;
}  // namespace initialize_details

using initialize_details::has_initializer;
using initialize_details::initialize;
using meta_invoke_protocols::meta_function_template_container;

template<class T>
concept meta_object_t = requires{
  typename meta_function_template_container<T::template meta_set>;
  typename meta_function_template_container<T::template apply>;
  typename T::function;
};

template<class T>
constexpr bool is_uninitialized_meta_object = (meta_object_t<T> && !exp_utilities::has_type<T>);
template<class T>
concept uninitialized_meta_object_t = is_uninitialized_meta_object<T>;

using initialize_details::object_initializer;
using meta_objects_details::meta_uninitialized;


// Primary declaration for the meta-object forms defined below.
template<class ...>
struct meta_object;

// An initializer-only meta_object creates its initial payload from invocation
// arguments by calling F::initialize.
template<object_initializer F>
struct meta_object<F>{
  using function = F;
  template<class ...Args>
  using apply = meta_object<initialize<function, Args...>,  function>;
  template<class ANOTHER_OBJ>
  using meta_set = meta_object<ANOTHER_OBJ, F>;
};

// Binds a payload to a meta-function. Each invocation applies F to the current
// payload and returns a new meta_object containing the resulting payload.
template <class OBJ, class F>
struct meta_object<OBJ, F> {
  using function = F;
  using type = OBJ;
  template <class... Arg>
  using apply = meta_object<meta_invoke<F, OBJ, Arg...>, function>;

  template <class ANOTHER_OBJ>
  using meta_set = meta_object<ANOTHER_OBJ, function>;
};

using initialize_details::ret_object_initializer;
// Primary declaration for meta_ret_object's initializer and bound forms.
template<class...>
struct meta_ret_object;

template<object_initializer F, class Ret>
struct meta_ret_object<F, Ret>{
  using function = F;
  using ret = meta_uninitialized;
  template<class ...Args>
  using apply = meta_ret_object<initialize<function, Args...>, function, Ret>;

  template<class ANOTHER_OBJ>
  using meta_set = meta_ret_object<ANOTHER_OBJ, F, Ret>;
};
// A meta_ret_object carries both its current payload and a derived result.
// Ret maps the payload to ::ret, allowing the object to expose a value distinct
// from its update state. F controls state updates; Ret controls result
// projection. An initializer can use meta_uninitialized until its first
// invocation establishes a concrete payload.
template <class OBJ, class F, class Ret>
struct meta_ret_object <OBJ, F, Ret>{
  using function = F;
  using ret = meta_invoke<Ret, OBJ>;
  using type = OBJ;
  template <class... Arg>
  using apply = meta_ret_object<meta_invoke<F, OBJ, Arg...>, F, Ret>;

  template <class ANOTHER_OBJ>
  using meta_set = meta_ret_object<ANOTHER_OBJ, F, Ret>;
};

template<class T>
concept meta_ret_object_t = meta_object_t<T> &&requires{
  typename T::ret;
};


namespace meta_timer_object_details {
struct meta_break_signal : std::false_type {};

template <class Fn, class DF>
struct meta_break_if {
  template <class T>
  using apply =
      std::conditional_t<meta_invoke<Fn, T>::value, meta_break_signal, DF>;
};
struct meta_always_continue {
  template <class T>
  struct apply : std::false_type {};
};
template <class MTO, class... Arg>
struct stop_forward_next_if_break_f_is_true {
  using type = MTO;
};
template <template <std::size_t, class, class, class> class meta_timer_template,
          std::size_t times, class OBJ, class F, class break_f, class... Arg>
  requires(!std::is_same_v<
           typename meta_timer_template<times, OBJ, F, break_f>::timer,
           meta_break_signal>)
struct stop_forward_next_if_break_f_is_true<
    meta_timer_template<times, OBJ, F, break_f>, Arg...> {
  using type =
      meta_timer_template<times - 1, meta_invoke<F, OBJ, Arg...>, F, break_f>;
};
}  // namespace meta_timer_object_details

// A countdown wrapper that applies F while its break predicate allows the
// timer to advance. It intentionally has no initializer form: the payload and
// remaining count are supplied when the wrapper is created.
template <std::size_t times, class OBJ, class F,
          class break_f =
        // A true predicate stops the countdown.
          meta_timer_object_details::meta_always_continue>
struct meta_timer_object {
  using timer = meta_invoke<

      // Emit a break signal when break_f accepts the current timer value;
      // otherwise preserve the default result indicating whether time remains.
      meta_timer_object_details::meta_break_if<
          break_f, std::integral_constant<bool, (times > 0)>>,
      OBJ>;

  using type = OBJ;
  using function = F;
  template <class... Arg>
  using apply =
      typename meta_timer_object_details::stop_forward_next_if_break_f_is_true<
          meta_timer_object<times, OBJ, F, break_f>, Arg...>::type;

  template <class ANOTHER_OBJ>
  using meta_set = meta_timer_object<times, ANOTHER_OBJ, F, break_f>;

  template <size_t reset_time>
  using reset = meta_timer_object<reset_time, OBJ, F, break_f>;
};
template<class T>
concept meta_timer_object_t = meta_object_t<T> && requires { typename T::timer; };
using meta_empty_o = meta_object<meta_objects_details::meta_empty,
                                 meta_objects_details::meta_empty_fn>;

// meta_op_bits 是 meta_looper 的布局，定义在后面的 meta_loop 区域。
// MSO 的 meta_set 需要 is_flags_operator 检查载体，这里前向声明（延迟实例化时解析）。
namespace meta_op_bits {

using u64 = std::uint64_t;
using u16 = std::uint16_t;
using u8  = std::uint8_t;

template<u64 flags>
using u64_int_t = std::integral_constant<u64, flags>;

// ===== flags 变换步骤：一元元函数，输入/输出都是带 ::value 的 u64 类型 =====
// 无条件置位/清位
template<u64 new_bits>
struct set_f{
  template<class flags_t>
  using apply = u64_int_t<flags_t::value | new_bits>;
};
template<u64 new_bits>
struct deact_f{
  template<class flags_t>
  using apply = u64_int_t<flags_t::value & ~new_bits>;
};
// 条件置位/清位：active 为 false 时原样返回 flags_t
template<u64 new_bits, bool active>
struct set_if_f{
  template<class flags_t>
  using apply = std::conditional_t<active,
      u64_int_t<flags_t::value | new_bits>, flags_t>;
};
template<u64 new_bits, bool active>
struct deact_if_f{
  template<class flags_t>
  using apply = std::conditional_t<active,
      u64_int_t<flags_t::value & ~new_bits>, flags_t>;
};

// ===== 字段宽度/掩码的命名空间级 trait（支持 bool 作为 1bit）=====
namespace flags_field_details {
  // 字段位宽：bool 为 1bit，其余为 sizeof(T)*8
  template <class T>
  struct field_bits : std::integral_constant<std::size_t, sizeof(T) * 8> {};
  template <>
  struct field_bits<bool> : std::integral_constant<std::size_t, 1> {};

  template <class T>
  constexpr std::size_t field_bits_v = field_bits<T>::value;

  // 字段的全 1 掩码值（未移位）：bool -> 1，其余 -> T(-1)
  template <class T>
  constexpr u64 field_ones =
      field_bits_v<T> == 64 ? ~u64{0} : ((u64{1} << field_bits_v<T>) - 1);

  // 字段掩码（移位到 shift）
  template <class T, std::size_t shift>
  constexpr u64 field_mask = field_ones<T> << shift;
}  // namespace flags_field_details

template<u64 flags>
struct flags_operator : u64_int_t<flags>
{
  // 将 flags_operator 隐式转换为 u64 类型，方便按位操作和掩码计算。
  constexpr operator u64() const noexcept { return flags; }
  // 单 bit 测试
  template<u64 bit>
  static constexpr bool has      = (flags & bit) == bit;   // 全部置位
  template<u64 bit>
  static constexpr bool has_any  = (flags & bit) != 0;     // 任一置位
  template<u64 bit>
  static constexpr bool test     = has_any<bit>;                // 常用别名

  // 整体测试
  static constexpr bool is_empty = (flags == 0);
  static constexpr bool any      = (flags != 0);
  static constexpr int  count    = std::popcount(flags);   // C++20

  
  // ===== 原始按位操作（u64 掩码）=====
  template<u64 bit>
  using opSet = flags_operator<flags | bit>;
  template<u64 bit>
  using opDeact = flags_operator<flags & ~bit>;
  template<u64 bit>
  using opToggle = flags_operator<flags ^ bit>;

  template<u64 bit, bool con = true>
  using opToggle_if = std::conditional_t<con, opToggle<bit>, flags_operator<flags>>;

  // ===== 字段访问：指定类型 + 偏移 =====
  // 生成指定位置的字段掩码（bool 为 1bit）
  template<class uintegral_type, std::size_t shift>
  static constexpr u64 mask = flags_field_details::field_mask<uintegral_type, shift>;

  // 获取指定位置的字段值，右移到最低位后按字段类型返回
  template<class uintegral_type, std::size_t shift>
  static constexpr uintegral_type get =
      static_cast<uintegral_type>((flags & mask<uintegral_type, shift>) >> shift);
  // 字段级测试
  template<class T, std::size_t shift>
  static constexpr bool field_any = (get<T, shift> != 0);
  template<class T, std::size_t shift, T v>
  static constexpr bool field_eq  = (get<T, shift> == v);
  template<class T, std::size_t shift>
  static constexpr bool field_none = (get<T, shift> == 0);

  // ===== 字段写入：set<T, shift, value> =====
  // 把字段类型的值 value 写入从 shift 开始的字段（掩码内），其他位清零后写入。
  template<class uintegral_type, std::size_t shift, uintegral_type value>
  using set = flags_operator<
      (flags & ~mask<uintegral_type, shift>) |
      ((static_cast<u64>(value) << shift) & mask<uintegral_type, shift>)>;

  // ===== 字段清空：deact<T, shift> =====
  // 把从 shift 开始的字段清零，其余位不变。
  template<class uintegral_type, std::size_t shift>
  using deact = flags_operator<flags & ~mask<uintegral_type, shift>>;

  // ===== 条件字段写入/清空 =====
  template<bool con, class uintegral_type, std::size_t shift, uintegral_type value>
  using set_if = std::conditional_t<con,
      set<uintegral_type, shift, value>, 
      flags_operator<flags>
    >;
  template<bool con, class T, std::size_t shift, T v_true, T v_false>
  using set_if_else = set<T, shift, con ? v_true : v_false>;

  // 条件位选择器：根据布尔条件选择性地设置或清除特定位
  template<bool con, u64 bit_true, u64 bit_false>
  using select = flags_operator<con ? (flags | bit_true) & ~bit_false
                                    : (flags | bit_false) & ~bit_true>;
  template<bool con, class uintegral_type, std::size_t shift>
  using deact_if = std::conditional_t<con,
      deact<uintegral_type, shift>, flags_operator<flags>>;
  // 字段按位
  template<class T, std::size_t shift, T v>
  using field_or  = set<T, shift, static_cast<T>(get<T, shift> | v)>;
  template<class T, std::size_t shift, T v>
  using field_and = set<T, shift, static_cast<T>(get<T, shift> & v)>;
  template<class T, std::size_t shift, T v>
  using field_xor = set<T, shift, static_cast<T>(get<T, shift> ^ v)>;

  // 字段自增/自减（比如计数器字段）
  template<class T, std::size_t shift, T delta = 1>
  using inc = set<T, shift, static_cast<T>(get<T, shift> + delta)>;
  template<class T, std::size_t shift, T delta = 1>
  using dec = set<T, shift, static_cast<T>(get<T, shift> - delta)>;
  // F 是一个 constexpr 函数 T(T)
  template<class T, std::size_t shift, auto F>
  using update = set<T, shift,
      static_cast<T>(F(get<T, shift>))>;
  // 用法：update<u8, 4, [](u8 v){ return v + 1; }>
  constexpr bool operator==(const flags_operator&) const noexcept = default;
  constexpr bool operator!=(const flags_operator&) const noexcept = default;
  // 或
  template<u64 other>
  static constexpr bool eq = (flags == other);
  template<u64 other>
  static constexpr bool ne = (flags != other);
  template<u64 other> using opOr  = flags_operator<flags | other>;
  template<u64 other> using opAnd = flags_operator<flags & other>;
  template<u64 other> using opXor = flags_operator<flags ^ other>;
  using opNot = flags_operator<~flags>;

  // 与另一个 flags_operator 实例合并
  template<class Other>
  using merge     = flags_operator<flags | Other::value>;
  template<class Other>
  using intersect = flags_operator<flags & Other::value>;
  template<class Other>
  using diff      = flags_operator<flags & ~Other::value>;   // A 去掉 B

  using reset = flags_operator<0>;
  template<u64 keep>
  using keep_only = flags_operator<flags & keep>;
};
template<class T>
struct is_flags_operator : std::false_type {};
template<u64 flags>
struct is_flags_operator<flags_operator<flags>> : std::true_type {};
template <class T>
constexpr bool is_flags_operator_v = is_flags_operator<T>::value;
}  // namespace meta_op_bits

// ===== meta_states_object（MSO）：持有 flags 的纯状态元对象 =====
// 它是 MO 的普通一员，符合 MO 调用规范：
//   - meta_invoke<MSO, objects...> 返回 MSO 的新状态
//   - MO 之间可相互调用（见 Meta_Object_Invoke 的 MSO 协议特化）
// MSO 自身不知道"生成器/接收器/looper"，那些是 meta_looper 用 MSO 实现条件判断的行为。
//
// 用户的 Fn 接收 flags_operator（this_flags），用 set/deact/set_if/deact_if 链式变换，
// Fn::apply<this_flags, objects...> 的结果成为新 MSO 的 flags。
//
// 签名：template<class Flags, class Fn>
// meta_states_object返回指定类型生成的特定bits
// meta_states_object 的产出是 ::flags
// meta_states_object 不规定bits的具体定义，请使用者自己实现自定义的bits布局
// meta_states_object 可以携带特定的执行器（executor）以实现自定义的行为
// 执行器仅供提取，不会直接影响 meta_states_object 的 flags。

using meta_op_bits::flags_operator;
using meta_op_bits::u64;
template<class T>
concept flags_operator_type = meta_op_bits::is_flags_operator_v<T>;

template<template<u64> class Fn>
struct states_executor{
  template<class flags_t, class ...Args>
  using apply = meta_invoke<Fn<flags_t::value>, Args...>;
};

template <u64 Flags, class Fn, class Executor = void>
struct meta_states_object {
  using executor = Executor;
  static constexpr bool has_executor = !std::is_same_v<Executor, void>;
  using flags = flags_operator<Flags>;
  using function = Fn;

  // 变参 apply：把 flags 和传入的 objects 一起交给用户的 Fn。
  // Fn::apply<flags, objects...>,正如其他MSO，以及meta_invoke的定义，apply直接就是指定类型，不需要::type解压
  // 注意：::value 是静态数据成员而非类型，requires 中不能加 typename。
  template <class... objects> requires requires { meta_invoke<Fn, flags, objects...>::value; }
  using apply = meta_states_object<
      meta_invoke<Fn, flags, objects...>::value, Fn, Executor>;

  // 直接使用 flags_operator 类型进行重置
  template <flags_operator_type ANOTHER_FLAGS>
  using meta_set = meta_states_object<ANOTHER_FLAGS::value, Fn, Executor>;

  // MSO 的产物是它的 flags。
  using type = flags;
  static constexpr auto value = flags::value;
};
// 检测是否为 meta_states_object 的实例（定义见 meta_timer_object 之后）。
// MSO 的 Flags 载体约定为 meta_op_bits::flags_operator。
template<class T>
concept meta_states_object_t = meta_object_t<T> && requires { typename T::flags; };
template<class T>
constexpr bool is_meta_states_object_v = meta_states_object_t<T>;


namespace meta_timer_object_details {
template <class OBJ, size_t N, class break_f>
struct To_Timer {};

template <class obj, class F, size_t N, class break_f>
struct To_Timer<meta_object<obj, F>, N, break_f> {
  using type = meta_timer_object<N, obj, F, break_f>;
};

template <class mo, class F>
struct Break_If {};
template <size_t N, class OBJ, class MO_F, class F>
struct Break_If<meta_timer_object<N, OBJ, MO_F>, F> {
  using type = meta_timer_object<N, OBJ, MO_F, F>;
};
}  // namespace meta_timer_object_details

template <class OBJ, size_t N,
          class break_f = meta_timer_object_details::meta_always_continue>
using to_timer =
    typename meta_timer_object_details::To_Timer<OBJ, N, break_f>::type;

// Returns the same timer wrapper with BF installed as its break predicate.
template <class MTO, class BF>
using break_if =
    exp_utilities::get_type<meta_timer_object_details::Break_If<MTO, BF>>;

namespace meta_objects_invoke_details {
template <class From_T, class To_T> requires(!is_meta_states_object_v<From_T>)
struct meta_transfer_object_impl {
  using type = typename To_T::template meta_set<typename From_T::type>;
};

// Transfer the timer's computed status to the destination object's payload.
template <size_t times, class obj, class F, class To_T, class B>
struct meta_transfer_object_impl<meta_timer_object<times, obj, F, B>, To_T> {
  using type = typename To_T::template meta_set<
      typename meta_timer_object<times, obj, F, B>::timer>;
};

// Transfer the projected result when the source is a meta_ret_object.
template <class Ret, class obj, class F, class To_T>
struct meta_transfer_object_impl<meta_ret_object<obj, F, Ret>, To_T> {
  using type = typename To_T::template meta_set<
      typename meta_ret_object<obj, F, Ret>::ret>;
};
}  // namespace meta_objects_invoke_details

template <class From_T, class To_T>
using meta_transfer_object =
    typename meta_objects_invoke_details::meta_transfer_object_impl<From_T,
                                                                    To_T>::type;

namespace meta_op_bits {



// ===== 全局控制：bit 56 ~ 63 =====
constexpr u64 G_CONTINUE   = 1ull << 63; // 1=继续，0=终止。对应你说的终止位
constexpr u64 G_NOT_SKIP   = 1ull << 62; // 1=调用回调，0=编译期跳过回调分支
// 以下两位决定【本轮循环结束后】投递给用户回调（或后续 meta_pipe）的解包协议，
// 不影响 looper 如何推进生成器/接收器。参考 meta_ios::protocols::stream_to/cache/from_t。
constexpr u64 G_FEED_GEN   = 1ull << 61; // 1=回调投递生成器结果，0=投递接收器结果
constexpr u64 G_PACK       = 1ull << 60; // 1=回调投递 pack<Generator,Receiver,State,Index>
constexpr u64 G_FORWARD    = 1ull << 59; // 1=正向 gen->recv->cond，0=反向 recv->gen->cond
constexpr u64 G_HOOK_MASK  = 0x7ull << 56; // bit56~58：中断/钩子 id，0 表示无
constexpr int G_HOOK_SHIFT = 56;

// ===== 数据区：bit 0 ~ 15 =====
constexpr u64 D_COUNT_MASK    = 0xFFFFull; // 整个数据区
constexpr u64 D_INDEX_MASK    = 0x00FFull; // 低 8 位：当前索引
constexpr u64 D_OPERAND_MASK  = 0xFF00ull; // 高 8 位：操作数
constexpr int D_OPERAND_SHIFT = 8;

// ===== slot 区 =====
constexpr int SLOT0_SHIFT = 16; // 生成器指令，16 位
constexpr int SLOT1_SHIFT = 32; // 接收器指令，16 位
constexpr int SLOT2_SHIFT = 48; // 条件/自定义指令，只留 8 位
constexpr u64 SLOT0_MASK  = 0xFFFFull << SLOT0_SHIFT;
constexpr u64 SLOT1_MASK  = 0xFFFFull << SLOT1_SHIFT;
constexpr u64 SLOT2_MASK  = 0x00FFull << SLOT2_SHIFT;

// ===== slot 内部布局 =====
// slot 低 8 位：修饰/基础动作
constexpr u16 S_INIT       = 1u << 0; // 调用 initialize
constexpr u16 S_APPLY      = 1u << 1; // 调用 apply
constexpr u16 S_TYPE_LIST  = 1u << 2; // 内部持有类型列表
constexpr u16 S_CLEAR      = 1u << 3; // 调用后清空/重置
constexpr u16 S_APPEND     = 1u << 4; // 追加到目标列表
constexpr u16 S_PREPEND    = 1u << 5; // 前插到目标列表
constexpr u16 S_FROM_TAIL  = 1u << 6; // 从源列表尾部取
constexpr u16 S_CONSUME    = 1u << 7; // 复制后消耗源

// slot 高 8 位：opcode
enum SlotOp : u8 {
    OP_NONE = 0,
    OP_INIT,
    OP_APPLY,
    OP_COPY_N,
    OP_CONCAT,
    OP_TAKE,
    OP_DROP,
    OP_REVERSE,
    OP_HOOK,

    // 高频专用组合，直接表达“从哪复制到哪”
    OP_COPY_RECV_N_TO_RECV_LIST,
    OP_COPY_RECV_N_TO_GEN_LIST,
    OP_APPEND_GEN_TO_RECV_LIST,
    OP_PREPEND_GEN_TO_RECV_LIST,
    OP_CONCAT_RECV_TO_RECV_LIST,
};

constexpr u16 slot_code(SlotOp op, u16 mods = 0) {
    return static_cast<u16>((static_cast<u16>(op) << 8) | mods);
}

//default ops：全局控制位之外，slot0（生成器）和 slot1（接收器）默认执行 apply。
// slot 指令用 opcode（slot_code(OP_APPLY)）写入字段高 8 位。
// 默认置 G_PACK：像 meta_stream 那样产出 pack 状态作为回调 stage / 最终 type。
constexpr u64 default_ops = G_CONTINUE | (G_NOT_SKIP & ~G_FEED_GEN) | G_FORWARD | G_PACK
    | (static_cast<u64>(slot_code(OP_APPLY)) << SLOT0_SHIFT)
    | (static_cast<u64>(slot_code(OP_APPLY)) << SLOT1_SHIFT);

// 索引 +1（8 位回绕，不影响操作数和其余区域）
struct inc_index_f{
  template<class flags_t>
  using apply = u64_int_t<(flags_t::value & ~D_INDEX_MASK)
                          | ((flags_t::value + 1) & D_INDEX_MASK)>;
};

// ===== loop_op：按 flags 特化的原子推进操作 =====
// 职责：只负责按 flags 推进 generator/state（gen apply、结果喂 state），
// 返回新的 (state, gen) 对。用户回调由 looper 按 G_NOT_SKIP 单独控制。
//
// 关键位：
//   bit63 G_CONTINUE       是否继续（0 时 looper 不应再调用 loop_op）
//   bit17 slot0 S_APPLY    是否推进生成器（generator apply）
//   bit33 slot1 S_APPLY    是否把生成器结果喂给接收器 state
//
// 主模板：未识别的 flags 组合。
template<u64 flags>
struct loop_op;

namespace loop_op_detail {
  // 从 flags 提取关键布尔量
  template<u64 flags>
  constexpr bool slot0_apply_v = (flags & (static_cast<u64>(S_APPLY) << SLOT0_SHIFT)) != 0;
  template<u64 flags>
  constexpr bool slot1_apply_v = (flags & (static_cast<u64>(S_APPLY) << SLOT1_SHIFT)) != 0;
}  // namespace loop_op_detail


}  // namespace meta_op_bits
using meta_op_bits::flags_operator;
using meta_op_bits::default_ops;
using meta_op_bits::u64;
using meta_op_bits::inc_index_f;

template<class T>
concept pure_meta_object_t = meta_object_t<T> 
  && !meta_states_object_t<T> 
  && !meta_ret_object_t<T> 
  && !meta_timer_object_t<T>;

/*meta object invocation protocols*/
namespace meta_objects_invoke_details {
// Treat the first meta-object as a meta-function and invoke it with the
// payload held by the second object.
using initialize_details::no_arguments_initializable;
using exp_utilities::has_type;

template<class ...Args>
struct meta_object_invoke_impl;

//当第二个元对象未初始化且可以无参初始化时，尝试无参初始化后再调用
template<meta_object_t OBJ1, uninitialized_meta_object_t OBJ2, class ...Args>
  requires(no_arguments_initializable<typename OBJ2::function>)
struct meta_object_invoke_impl<OBJ1, OBJ2, Args...> {
  using type = meta_object_invoke_impl<OBJ1, meta_invoke<OBJ2>, Args...>::type;
};
//当第二个元对象未初始化且无法无参初始化时，提供一个带初始参数的初始化结构
template<meta_object_t OBJ1, uninitialized_meta_object_t OBJ2, class ...Args>
  requires(!no_arguments_initializable<typename OBJ2::function>)
struct meta_object_invoke_impl<OBJ1, OBJ2, Args...> {
  struct type
  {
    template<class ...initial_args>
    using apply = meta_object_invoke_impl<OBJ1, meta_invoke<OBJ2, initial_args...>, Args...>::type;
  };
};
//使用has_type检查元对象是否具有type成员，确保元对象已经初始化
//如果调用方也未初始化，必须具有合法的初始化函数

//pure meta object 返回::type
template<meta_object_t OBJ1, pure_meta_object_t OBJ2, class ...Args> 
  requires(has_type<OBJ2> && ! meta_states_object_t<OBJ1>)
struct meta_object_invoke_impl<OBJ1, OBJ2, Args...> {
  using type = meta_invoke<OBJ1, typename OBJ2::type, Args...>;
};

//meta_ret_object 返回::ret
template<meta_object_t OBJ1, meta_ret_object_t OBJ2, class ...Args>
  requires(has_type<OBJ2> && ! meta_states_object_t<OBJ1>)
struct meta_object_invoke_impl<OBJ1, OBJ2, Args...> {
  using type = meta_invoke<OBJ1, typename OBJ2::ret, Args...>;
};

//meta_timer_object 返回::type
template<meta_object_t OBJ1, meta_timer_object_t OBJ2, class ...Args>
  requires(has_type<OBJ2> && ! meta_states_object_t<OBJ1>)
struct meta_object_invoke_impl<OBJ1, OBJ2, Args...> {
  using type = meta_invoke<OBJ1, typename OBJ2::type, Args...>;
};
//meta_states_object 返回::flags
template<meta_object_t OBJ1, meta_states_object_t OBJ2, class ...Args>
  requires(has_type<OBJ2> && ! meta_states_object_t<OBJ1>)
struct meta_object_invoke_impl<OBJ1, OBJ2, Args...> {
  using type = meta_invoke<OBJ1, typename OBJ2::flags, Args...>;
};
//meta_states_object 获得整个元对象本身
template<meta_states_object_t OBJ1, meta_object_t OBJ2, class ...Args>
  requires(has_type<OBJ1> && ! meta_states_object_t<OBJ2> && has_type<OBJ2>)
struct meta_object_invoke_impl<OBJ1, OBJ2, Args...> {
  using type = meta_invoke<OBJ1, OBJ2, Args...>;
};
//meta_states_objects相互调用，传递 flags
template<meta_states_object_t OBJ1, meta_states_object_t OBJ2, class ...Args>
  requires(has_type<OBJ1> && has_type<OBJ2>)
struct meta_object_invoke_impl<OBJ1, OBJ2, Args...> {
  using type = meta_invoke<OBJ1, typename OBJ2::flags, Args...>;
};

}  // namespace meta_objects_invoke_details
//如果OBJ2 未初始化，且无法无参数初始化，将返回一个元函数，等待初始化参数再调用。
//可以使用meta_invoke_protocols::is_meta_function_v检测返回的是否为元函数
template <meta_object_t OBJ1, meta_object_t OBJ2, class ...Args> 
using meta_object_invoke =
    typename meta_objects_invoke_details::meta_object_invoke_impl<OBJ1, OBJ2, Args...>::type;

namespace meta_objects_invoke_details {
// 按协议解包元对象的【产出】（不调用）：
//   meta_object      -> ::type
//   meta_ret_object  -> ::ret
//   meta_states_object -> ::flags
// 这是 cache 的来源：生成器产出但未被接收器消费的值。
template <class MO, class = void>
struct meta_produce {
  using type = typename MO::type;  // 默认：meta_object 的产出是 ::type
};
template <class MO>
struct meta_produce<MO, std::void_t<typename MO::ret>> {
  using type = typename MO::ret;   // meta_ret_object 的产出是 ::ret
};
template <class MO>
struct meta_produce<MO, std::void_t<typename MO::flags>> {
  using type = typename MO::flags;   // meta_states_object 的产出是 ::flags
};
}  // namespace meta_objects_invoke_details

template <class MO>
using meta_produce_t = typename meta_objects_invoke_details::meta_produce<MO>::type;

namespace invoke_object_if_details {
template <bool, class MO1, class MO2>
struct meta_o_branch {
  using type = MO1;
};
template <class MO1, class MO2>
struct meta_o_branch<true, MO1, MO2> {
  using type = meta_object_invoke<MO1, MO2>;
};
}  // namespace invoke_object_if_details

template <bool con>
struct invoke_object_if {
  template <class MO1, class MO2>
  using apply =
      typename invoke_object_if_details::meta_o_branch<con, MO1, MO2>::type;
};

}  // namespace meta_objects

namespace meta_loop {
using namespace meta_objects;

// The looper condition, state, and generator are meta objects.
namespace meta_looper_detail {
template <bool, class Condition, class OBJ, class Generator = meta_empty_o>
struct meta_looper_impl {
  template <class... Args>
  struct apply {
    // Adapt the current state to the condition's expected input, then evaluate
    // whether another iteration should run.
    using _continue_t =
        typename meta_invoke<meta_transfer_object<OBJ, Condition>>::type;
    static const bool _continue_ = _continue_t::value;

    // Advance the generator only when the condition permits another iteration.
    using generator_stage_o =
        meta_invoke<invoke_if<_continue_>, Generator, Args...>;

    // Apply the generated input to the current state when continuing; otherwise
    // retain the current state as the final result.
    using result_stage_o =
        meta_invoke<invoke_object_if<_continue_>, OBJ, generator_stage_o>;

    // Instantiate the next iteration only when the condition remains true.
    using track_apply_t =
        meta_invoke<invoke_if<_continue_>,
                    meta_looper_impl<_continue_, Condition, result_stage_o,
                     generator_stage_o>,
                    Args...>;
    using type = typename track_apply_t::type;

    template <class... arg_types>
    static constexpr decltype(auto) for_each(auto&& f, arg_types&&... args) {
      using stage_t = typename result_stage_o::type;

      using return_type =
          std::invoke_result_t<decltype(f), stage_t, arg_types...>;

      if constexpr (std::is_void_v<return_type>) {
        if constexpr (track_apply_t::_continue_) {
          std::invoke(f, stage_t{}, std::forward<arg_types>(args)...);
          return track_apply_t::for_each(f, std::forward<arg_types>(args)...);
        } else {
          return std::invoke(f, stage_t{}, std::forward<arg_types>(args)...);
        }
      } else {
        if constexpr (track_apply_t::_continue_) {
          (void)std::invoke(f, stage_t{}, std::forward<arg_types>(args)...);
          return track_apply_t::for_each(f, std::forward<arg_types>(args)...);
        } else {
          return std::invoke(f, stage_t{},
                             std::forward<arg_types>(args)...);
        }
      }
    }

    // Applies each unary metafunction in Ps to the stage type before invoking
    // f. The folded type is used only for the callback; loop state is unchanged.
    template <template <class> class... Ps, class... arg_types>
      requires(sizeof...(Ps) > 0)
    static constexpr decltype(auto) for_each(auto&& f, arg_types&&... args) {
      using raw_stage = typename result_stage_o::type;
      using stage_t = meta_fold<raw_stage, Ps...>;

      using return_type =
          std::invoke_result_t<decltype(f), stage_t, arg_types...>;
      if constexpr (std::is_void_v<return_type>) {
        if constexpr (track_apply_t::_continue_) {
          std::invoke(f, stage_t{}, std::forward<arg_types>(args)...);
          return track_apply_t::template for_each<Ps...>(
              f, std::forward<arg_types>(args)...);
        } else {
          return std::invoke(f, stage_t{},
                             std::forward<arg_types>(args)...);
        }
      } else {
        if constexpr (track_apply_t::_continue_) {
          (void)std::invoke(f, stage_t{}, std::forward<arg_types>(args)...);
          return track_apply_t::template for_each<Ps...>(
              f, std::forward<arg_types>(args)...);
        } else {
          return std::invoke(f, stage_t{},
                             std::forward<arg_types>(args)...);
        }
      }
    }

    template <class first_arg_type, class... arg_types>
    static constexpr decltype(auto) for_each_forward(auto&& f,
                                                     first_arg_type&& first,
                                                     arg_types&&... args) {
      using stage_t = typename result_stage_o::type;

      using return_type =
          std::invoke_result_t<decltype(f), stage_t, first_arg_type>;

      if constexpr (std::is_void_v<return_type>) {
        if constexpr (track_apply_t::_continue_ && sizeof...(arg_types)) {
          std::invoke(f, stage_t{}, std::forward<first_arg_type>(first));
          return track_apply_t::for_each_forward(
              f, std::forward<arg_types>(args)...);
        } else {
          return std::invoke(f, stage_t{},
                             std::forward<first_arg_type>(first));
        }
      } else {
        if constexpr (track_apply_t::_continue_ && sizeof...(arg_types)) {
          (void)std::invoke(f, stage_t{},
                            std::forward<first_arg_type>(first));
          return track_apply_t::for_each_forward(
              f, std::forward<arg_types>(args)...);
        } else {
          return std::invoke(f, stage_t{},
                             std::forward<first_arg_type>(first));
        }
      }
    }

    // Forward one input argument to each callback, applying Ps to the stage
    // type first. Additional arguments are forwarded to subsequent iterations.
    template <template <class> class... Ps, class first_arg_type,
              class... arg_types>
      requires(sizeof...(Ps) > 0)
    static constexpr decltype(auto) for_each_forward(
        auto&& f, first_arg_type&& first, arg_types&&... args) {
      using raw_stage = typename result_stage_o::type;
      using stage_t = meta_fold<raw_stage, Ps...>;

      using return_type =
          std::invoke_result_t<decltype(f), stage_t, first_arg_type>;
      if constexpr (std::is_void_v<return_type>) {
        if constexpr (track_apply_t::_continue_ && sizeof...(arg_types)) {
          std::invoke(f, stage_t{}, std::forward<first_arg_type>(first));
          return track_apply_t::template for_each_forward<Ps...>(
              f, std::forward<arg_types>(args)...);
        } else {
          return std::invoke(f, stage_t{},
                             std::forward<first_arg_type>(first));
        }
      } else {
        if constexpr (track_apply_t::_continue_ && sizeof...(arg_types)) {
          (void)std::invoke(f, stage_t{},
                            std::forward<first_arg_type>(first));
          return track_apply_t::template for_each_forward<Ps...>(
              f, std::forward<arg_types>(args)...);
        } else {
          return std::invoke(f, stage_t{},
                             std::forward<first_arg_type>(first));
        }
      }
    }
  };
};

//此处提供Main_states 以及两个子states的元函数
namespace states_based_condition_details{
  using namespace meta_op_bits;

  // 默认 Fn：什么都不做，flags 原样返回。
  struct identity_f{
    template<class this_flags, class...>
    using apply = this_flags;
  };

  // 主 states 的 Fn：吸收子 states 算出的 flags，再过用户 Fn 得主 states 的新状态。
  // Fn 什么都不做时，最终 flags = 子 states 算出的值。
  template<class Fn>
  struct main_states_f{
    template<class this_flags, class sub_states_flags>
    struct apply_impl{
      using type = meta_invoke<Fn, sub_states_flags>;
    };
    template<class this_flags, class sub_states_flags>
    using apply = typename apply_impl<this_flags, sub_states_flags>::type;
  };

  // 检测元对象产出是否为 end_of_list（无产出形态如 initializer 返回 false）
  template<class MO, class = void>
  struct produce_is_end : std::false_type {};
  template<class MO>
  struct produce_is_end<MO, std::void_t<meta_produce_t<MO>>>
      : std::bool_constant<
            std::is_same_v<meta_produce_t<MO>, exp_utilities::literal_types::end_of_list>> {};
  template<class MO>
  constexpr bool produce_is_end_v = produce_is_end<MO>::value;

  // slot0 子 states 的 Fn：观测生成器，按初始化状态写 slot0 指令字段；
  // 若生成器产出已是 end_of_list（耗尽），自动清 G_CONTINUE 停止循环。
  //   未初始化 -> slot0 = slot_code(OP_INIT)（本轮只初始化生成器）
  //   已初始化 -> slot0 = slot_code(OP_APPLY)（本轮推进生成器）
  struct slot0_states_f{
    template<class this_flags, class slot0_o>
    struct apply_impl{
      static constexpr bool is_uninitialized = is_uninitialized_meta_object<slot0_o>;
      static constexpr u16 code =
          slot_code(is_uninitialized ? OP_INIT : OP_APPLY);
      // 生成器产出为 end_of_list（耗尽）时停止
      static constexpr bool exhausted = produce_is_end_v<slot0_o>;
      // 链式：写 slot0 指令字段，再按耗尽条件清 G_CONTINUE
      using type = typename this_flags
          ::template set<u16, SLOT0_SHIFT, code>
          ::template deact_if<exhausted, bool, 63>;  // G_CONTINUE = bit63
    };
    template<class this_flags, class slot0_o>
    using apply = typename apply_impl<this_flags, slot0_o>::type;
  };

  // slot1 子 states 的 Fn：观测接收器，按初始化状态写 slot1 指令字段。
  //   未初始化 -> slot1 = slot_code(OP_INIT)
  //   已初始化 -> slot1 = slot_code(OP_APPLY)
  struct slot1_states_f{
    template<class this_flags, class slot1_o>
    struct apply_impl{
      static constexpr bool is_uninitialized = is_uninitialized_meta_object<slot1_o>;
      static constexpr u16 code =
          slot_code(is_uninitialized ? OP_INIT : OP_APPLY);
      using type = typename this_flags
          ::template set<u16, SLOT1_SHIFT, code>;
    };
    template<class this_flags, class slot1_o>
    using apply = typename apply_impl<this_flags, slot1_o>::type;
  };
}

// ===== meta_looper 用 meta_states_object 实现条件判断的布局部件 =====
// 这些原子操作由 slot flags 驱动，与 meta_states_object 解耦——MSO 只提供 flags。
namespace states_looper_detail {
  using namespace meta_op_bits;

  // 从 flags 提取 slot 的 opcode（高 8 位）
  template<u64 flags, u64 shift>
  constexpr SlotOp slot_opcode_v =
      static_cast<SlotOp>((flags & (0xFFull << (shift + 8))) >> (shift + 8));

  // 从 flags 读取 slot 的 opcode 是否要调用（INIT / APPLY 都调用）
  template<u64 flags>
  constexpr bool slot0_call_v =
      (slot_opcode_v<flags, SLOT0_SHIFT> == OP_INIT) ||
      (slot_opcode_v<flags, SLOT0_SHIFT> == OP_APPLY);
  template<u64 flags>
  constexpr bool slot1_call_v =
      (slot_opcode_v<flags, SLOT1_SHIFT> == OP_INIT) ||
      (slot_opcode_v<flags, SLOT1_SHIFT> == OP_APPLY);

  // ===== gen_op：对生成器的原子操作（由 slot0 驱动）=====
  // 接收 flags_operator 类型，用其 get API 读取 slot0 的 opcode 分派。
  // INIT / APPLY 是不同操作；修饰位（如 S_TYPE_LIST）保留在 flags 供后续优化读取。
  template<class Flags, class Generator, class... Args>
  struct gen_op {
    static constexpr SlotOp op = static_cast<SlotOp>(Flags::template get<u8, SLOT0_SHIFT + 8>);
    static constexpr bool call = (op == OP_INIT) || (op == OP_APPLY);
    using type = meta_invoke<invoke_if<call>, Generator, Args...>;
  };

  // ===== recv_op：对接收器的原子操作（由 slot1 驱动）=====
  // 接收 flags_operator 类型，用其 get API 读 slot1 修饰位。
  // 两个元对象的调用走 meta_object_invoke 协议（拆包由它决定）：
  // slot1 INIT 时用生成器产出初始化 receiver；APPLY 时用产出推进 receiver。
  // slot1 关闭 = 跳过，接收器保持。
  template<class Flags, class OBJ, class GenResult>
  struct recv_op {
    // slot1 的 opcode（高 8 位）：INIT / APPLY 时调用 receiver（INIT 用产出初始化，APPLY 用产出推进）
    static constexpr SlotOp op = static_cast<SlotOp>(Flags::template get<u8, SLOT1_SHIFT + 8>);
    static constexpr bool call = (op == OP_INIT) || (op == OP_APPLY);
    // 生成器推进后的产出已是 end_of_list 时不喂（终止信号，不进入接收器）
    static constexpr bool gen_produce_end =
        std::is_same_v<meta_produce_t<GenResult>, exp_utilities::literal_types::end_of_list>;
    using type = meta_invoke<invoke_object_if<call && !gen_produce_end>, OBJ, GenResult>;
  };

  // ===== loop_pack：打包本轮状态供用户回调 / 下一阶段（meta_pipe）解包 =====
  // cache = 生成器产出（按协议解包）但未被接收器消费的部分。
  template <class Gen, class Recv, class State, u64 Index>
  struct loop_pack {
    using generator = Gen;
    using receiver = Recv;
    using state = State;
    static constexpr u64 index = Index;

    // 生成器产出但未被接收器消费的值（按 meta_produce 协议解包）
    using cache = meta_produce_t<Gen>;

    // 常用解包视图
    using gen_t = typename Gen::type;
    using state_t = typename State::type;
    static constexpr bool gen_end = requires { Gen::type::end; } && Gen::type::end;
    consteval u64 index_v() const { return Index; }
  };

  // 产出 stage：G_PACK 置位时打包 loop_pack，否则按 G_FEED_GEN 选生成器/接收器结果。
  // index 读 flags 数据区低 8 位（get<u8, 0>）。
  template<u64 flags, class Gen, class RecvObj, class States>
  struct make_stage {
    static constexpr u64 idx = flags_operator<flags>::template get<u8, 0>;
    using type = std::conditional_t<(flags & G_PACK) != 0,
        loop_pack<Gen, RecvObj, RecvObj, idx>,
        std::conditional_t<(flags & G_FEED_GEN) != 0,
            Gen, RecvObj>>;
  };
}  // namespace states_looper_detail

// looper 特化：Condition 为 meta_states_object。
// 用一个主 MSO 统筹，两个独立子 MSO 分别观测生成器/接收器算出对应 slot 的 flags，
// 主 MSO 吸收子结果作为新条件状态。
// 元对象之间的调用走 meta_object_invoke 协议（MSO 作目标时收完整对象，作源时投递 flags）。
template<meta_states_object_t Condition, class Receiver, class Generator> 
struct meta_looper_impl<true, Condition, Receiver, Generator> {
  using states_t = Condition;

  // 子 states 元函数：slot0 观测生成器，slot1 观测接收器
  using slot0_o = meta_states_object<states_t::value, states_based_condition_details::slot0_states_f>;

  template <class... Args>
  struct apply {
    // slot0：子 MSO 观测生成器（走 meta_object_invoke），写 slot0 指令
    using slot0_states_t = meta_object_invoke<slot0_o, Generator>;

    // slot1 是否被禁用（读 slot0 算出的 flags 的 slot1 修饰位）
    static constexpr bool slot1_not_disable =
        states_looper_detail::slot1_call_v<slot0_states_t::flags::value>;

    // slot1：invoke_object_if 包装——slot1 未被禁用时子 MSO 才去观测 Receiver 写 slot1 指令；
    // 被禁用（false）时 invoke_object_if 返回子 MSO 本身，flags 仍是 slot0 的结果（跳过接收器观测）。
    using slot1_states_t = meta_invoke<
        invoke_object_if<slot1_not_disable>,
        meta_states_object<slot0_states_t::flags::value, states_based_condition_details::slot1_states_f>,
        Receiver>;
    // 主 MSO 吸收子结果（meta_object_invoke<MSO, MSO> 投递 slot1 的 flags）
    using absorbed_states = meta_object_invoke<states_t, slot1_states_t>;

    // index++：只有 slot1 未被禁用（本轮 receiver 被调用）才给 index 自增，写进下一轮条件状态。
    // 若跳过（slot1 禁用），index 不变。用链式条件写入数据区低 8 位（u8 自增）。
    using absorbed_flags = typename absorbed_states::flags;
    using next_flags_t = typename absorbed_flags
        ::template set_if<slot1_not_disable, meta_op_bits::u8, 0,
                          static_cast<meta_op_bits::u8>(absorbed_flags::template get<meta_op_bits::u8, 0> + 1)>;
    // 下一轮的主 MSO 条件状态
    using new_main_states = typename absorbed_states::template meta_set<next_flags_t>;

    // 本轮 flags（主 MSO 统筹后的结果）
    using flags_t = typename new_main_states::flags;

    // 是否继续：读 G_CONTINUE 位（slot0_states_f 在生成器产出 end_of_list 时清零）
    static constexpr bool continue_ = flags_t::template get<bool, 63>;

    // 用 flags 驱动原子操作（continue_=false 时本轮静止，不再推进/喂入）。
    // gen_op 收 flags_operator 用 get 读 opcode；recv_op 收 flags_operator 走 meta_object_invoke 协议。
    using new_gen = typename std::conditional_t<continue_,
        states_looper_detail::gen_op<flags_t, Generator, Args...>,
        states_looper_detail::gen_op<flags_operator<0>, Generator, Args...>  // 静止：opcode=OP_NONE
        >::type;
    using new_receiver = typename std::conditional_t<continue_,
        states_looper_detail::recv_op<flags_t, Receiver, new_gen>,
        states_looper_detail::recv_op<flags_operator<0>, Receiver, new_gen>  // 静止
        >::type;

    // 递归下一迭代：continue_ 决定进入 meta_looper_impl<true>（推进）或 <false>（终止），
    // 终止分支不再实例化下一迭代（避免对空生成器再探测）。
    using track_apply_t = meta_invoke<
        meta_looper_impl<continue_, new_main_states, new_receiver, new_gen>,
        Args...>;

    // 最终产出 type：
    //   continue_ = true  -> 递归取下一轮的 type（一路传到终止轮）
    //   continue_ = false -> 本轮产出 make_stage（G_PACK 置位则为 pack，否则接收器结果）
    using type = std::conditional_t<continue_,
        typename track_apply_t::type,
        typename states_looper_detail::make_stage<
            flags_t::value, new_gen, new_receiver, new_main_states>::type>;

    template <class... arg_types>
    static constexpr decltype(auto) for_each(auto&& f, arg_types&&... args) {
      // bit62 G_NOT_SKIP 即 observe：0 时不实例化 f、不推导返回类型，只递归
      constexpr bool observe = flags_t::template get<bool, 62>;

      // 终止轮（continue_=false）不推进也不调回调，直接返回
      if constexpr (!continue_) {
        return;
      } else if constexpr (!observe) {
        // 当前阶段不需要观察：不实例化 f，只递归
        return track_apply_t::for_each(f, std::forward<arg_types>(args)...);
      } else {
        using stage_t = typename states_looper_detail::make_stage<
            flags_t::value, new_gen, new_receiver, new_main_states>::type;
        using return_type =
            std::invoke_result_t<decltype(f), stage_t, arg_types...>;
        if constexpr (std::is_void_v<return_type>) {
          std::invoke(f, stage_t{}, std::forward<arg_types>(args)...);
          return track_apply_t::for_each(f, std::forward<arg_types>(args)...);
        } else {
          (void)std::invoke(f, stage_t{}, std::forward<arg_types>(args)...);
          return track_apply_t::for_each(f, std::forward<arg_types>(args)...);
        }
      }
    }

    // 每轮消费一个参数 first 喂给 f(stage_t, first)，剩余 args 传给下一轮。
    // observe=false 时不实例化 f、不推导返回类型，仅按剩余参数递归。
    template <class first_arg_type, class... arg_types>
    static constexpr decltype(auto) for_each_forward(auto&& f, first_arg_type&& first, arg_types&&... args) {
      constexpr bool observe = flags_t::template get<bool, 62>;
      // 是否还有下一轮参数可供 forward
      constexpr bool has_next = continue_ && (sizeof...(arg_types) > 0);

      if constexpr (!continue_ || !observe) {
        // 终止或不观察：不实例化 f。若还有参数则继续递归传递。
        if constexpr (has_next) {
          return track_apply_t::for_each_forward(f, std::forward<arg_types>(args)...);
        } else {
          return;
        }
      } else {
        using stage_t = typename states_looper_detail::make_stage<
            flags_t::value, new_gen, new_receiver, new_main_states>::type;
        using return_type =
            std::invoke_result_t<decltype(f), stage_t, first_arg_type>;
        if constexpr (std::is_void_v<return_type>) {
          if constexpr (has_next) {
            std::invoke(f, stage_t{}, std::forward<first_arg_type>(first));
            return track_apply_t::for_each_forward(f, std::forward<arg_types>(args)...);
          } else {
            return std::invoke(f, stage_t{}, std::forward<first_arg_type>(first));
          }
        } else {
          if constexpr (has_next) {
            (void)std::invoke(f, stage_t{}, std::forward<first_arg_type>(first));
            return track_apply_t::for_each_forward(f, std::forward<arg_types>(args)...);
          } else {
            return std::invoke(f, stage_t{}, std::forward<first_arg_type>(first));
          }
        }
      }
    }
  };
};

template <class Cond, class Receiver, class Generator>
struct meta_looper_impl<false, Cond, Receiver, Generator> {
  static constexpr bool _continue_ = false;
  using type = typename Receiver::type;
  template <class... Args>
  struct apply {
    using type = typename Receiver::type;
    template <class... arg_types>
    static constexpr decltype(auto) for_each(auto&&, arg_types&&...) { return; }
    template <class first_arg_type, class... arg_types>
    static constexpr decltype(auto) for_each_forward(auto&&, first_arg_type&&, arg_types&&...) { return; }
  };
};

}  // namespace meta_looper_detail

template <class C, class O, class G, class... ARG_Tys>
using meta_looper_t = typename meta_invoke<
  meta_looper_detail::meta_looper_impl<true, C, O, G>,
    ARG_Tys...>::type;

template <class C, class O, class G = meta_empty_o>
using meta_looper =
  meta_looper_detail::meta_looper_impl<true, C, O, G>;
}  // namespace meta_loop
