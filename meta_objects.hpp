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

namespace initialize_details {
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
  template <class... Arg>
  using apply =
      typename meta_timer_object_details::stop_forward_next_if_break_f_is_true<
          meta_timer_object<times, OBJ, F, break_f>, Arg...>::type;

  template <class ANOTHER_OBJ>
  using meta_set = meta_timer_object<times, ANOTHER_OBJ, F, break_f>;

  template <size_t reset_time>
  using reset = meta_timer_object<reset_time, OBJ, F, break_f>;
};
using meta_empty_o = meta_object<meta_objects_details::meta_empty,
                                 meta_objects_details::meta_empty_fn>;
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
template <class From_T, class To_T>
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

using u64 = std::uint64_t;
using u16 = std::uint16_t;
using u8  = std::uint8_t;

template<u64 flags>
using u64_int_t = std::integral_constant<u64, flags>;

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
// 默认置 G_PACK：像 meta_stream 那样产出 pack 状态作为回调 stage / 最终 type。
constexpr u64 default_ops = G_CONTINUE | G_NOT_SKIP | G_FEED_GEN | G_FORWARD | G_PACK
    | (static_cast<u64>(S_APPLY) << SLOT0_SHIFT)
    | (static_cast<u64>(S_APPLY) << SLOT1_SHIFT);

template<u64 flags>
struct flags_operator : u64_int_t<flags>
{
  // 置位：把 bit 指定的位设为 1
  template<u64 bit>
  using opSet = u64_int_t<flags | bit>;
  // 清位：把 bit 指定的位设为 0，其余位保持不变
  template<u64 bit>
  using opDeact = u64_int_t<flags & ~bit>;
  // 索引 +1（8 位回绕，不影响操作数和其余区域）
  using inc_index = u64_int_t<(flags & ~D_INDEX_MASK) | ((flags + 1) & D_INDEX_MASK)>;
  // 当前索引：数据区低 8 位
  using index = u64_int_t<flags & D_INDEX_MASK>;
  // 当前操作数：数据区高 8 位
  using operand = u64_int_t<((flags & D_OPERAND_MASK) >> D_OPERAND_SHIFT)>;
  static constexpr u64 value = flags;
};

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
// 索引 +1（8 位回绕，不影响操作数和其余区域）
struct inc_index_f{
  template<class flags_t>
  using apply = u64_int_t<(flags_t::value & ~D_INDEX_MASK)
                          | ((flags_t::value + 1) & D_INDEX_MASK)>;
};

// flags_fold：直接接收 flags 类型，内部为每个 Step 补上 ::template apply
// 并交给 meta_fold 做左折叠。Step 约定：template<class flags_t> apply。
template<class flags_t, class... Steps>
using flags_fold = flags_operator<meta_fold<flags_t, Steps::template apply...>::value>;

// 检测是否为 flags_operator 的实例
template<class T>
struct is_flags_operator : std::false_type {};
template<u64 flags>
struct is_flags_operator<flags_operator<flags>> : std::true_type {};
template<class T>
constexpr bool is_flags_operator_v = is_flags_operator<T>::value;

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
using meta_op_bits::flags_fold;
using meta_op_bits::flags_operator;
using meta_op_bits::default_ops;
using meta_op_bits::u64;
using meta_op_bits::inc_index_f;

// 默认耗尽探测：优先使用 end_of_stream 协议（MOBJ::type::end）。
// 用户可自定义 Probe 替换以适配无该协议的生成器。
namespace meta_states_details {
  template<class T, class = void>
  struct probe_end : std::false_type {};
  template<class T>
  struct probe_end<T, std::void_t<decltype(T::end)>>
      : std::bool_constant<T::end> {};
}  // namespace meta_states_details

// 默认探测元函数：step 给出一个 fold 步骤，耗尽时清 G_CONTINUE。
struct default_probe_f {
  // 满足 meta_function_t 约束（probe 不作为 fold 步骤，仅占位）
  template<class T>
  using apply = T;
  template<class MOBJ>
  using step = meta_op_bits::deact_if_f<
      meta_op_bits::G_CONTINUE,
      meta_states_details::probe_end<typename MOBJ::type>::value>;
};

//the meta_states_object is an object play as conditions in meta_looper
//it receives meta_signals and set its flags to control the looper's behavior
//Probe<MOBJ>::step 给出"根据生成器状态变换 flags"的 fold 步骤；默认检测耗尽。
//
// 两阶段探测（生成器/接收器解耦）：
//   probe_generator<Generator>  观测生成器 → 定 slot0（含未初始化时置 S_INIT），index 不增
//   probe_receiver<GenResult>   观测接收器阶段 → 定 slot1，最后递增 index
template<class Flags = flags_operator<default_ops>, class Probe = default_probe_f, meta_function_t ...Fn>
struct meta_states_object{
  using flags = Flags;
  using probe = Probe;

  // 阶段一：观测生成器，生成本轮 flags 的生成器部分。
  // 初始化修正【先】跑：未初始化时 slot0=S_INIT、关 slot0 S_APPLY、关 G_NOT_SKIP
  // （初始化轮只初始化生成器，不调回调）；已初始化时清 S_INIT、恢复 S_APPLY。
  // Fn 与 Probe 后跑：可在其上覆盖（含非初始化轮重新打开 G_NOT_SKIP）。
  template<meta_object_t Generator>
  using probe_generator = meta_states_object<
      flags_fold<flags,
          meta_op_bits::set_if_f<
              (static_cast<meta_op_bits::u64>(meta_op_bits::S_INIT) << meta_op_bits::SLOT0_SHIFT),
              is_uninitialized_meta_object<Generator>>,
          meta_op_bits::deact_if_f<
              (static_cast<meta_op_bits::u64>(meta_op_bits::S_INIT) << meta_op_bits::SLOT0_SHIFT),
              !is_uninitialized_meta_object<Generator>>,
          meta_op_bits::deact_if_f<
              (static_cast<meta_op_bits::u64>(meta_op_bits::S_APPLY) << meta_op_bits::SLOT0_SHIFT),
              is_uninitialized_meta_object<Generator>>,
          meta_op_bits::set_if_f<
              (static_cast<meta_op_bits::u64>(meta_op_bits::S_APPLY) << meta_op_bits::SLOT0_SHIFT),
              !is_uninitialized_meta_object<Generator>>,
          meta_op_bits::deact_if_f<
              meta_op_bits::G_NOT_SKIP,
              is_uninitialized_meta_object<Generator>>,
          // 已初始化：恢复 G_NOT_SKIP 默认开（Fn 后跑仍可覆盖关闭）
          meta_op_bits::set_if_f<
              meta_op_bits::G_NOT_SKIP,
              !is_uninitialized_meta_object<Generator>>,
          Fn...,
          typename Probe::template step<Generator>>,
      Probe, Fn...>;

  // 阶段二：观测接收器阶段的结果（生成器操作后的对象 GenResult），定 slot1，递增 index。
  // Probe::step<GenResult> 在【生成器推进后】检测耗尽：
  //   若本轮产出已是 end_of_list / 列表已空 → 清 G_CONTINUE 并关 slot1，
  //   阻止把 end_of_list 喂给接收器（避免多跑一次）。
  template<class GenResult>
  using probe_receiver = meta_states_object<
      flags_fold<flags, typename Probe::template step<GenResult>, inc_index_f>,
      Probe, Fn...>;

  // 兼容旧式整体 apply：等价于一次整体探测。
  template<meta_object_t MOBJ>
  using apply = meta_states_object<
      flags_fold<flags, Fn..., typename Probe::template step<MOBJ>, inc_index_f>,
      Probe, Fn...>;

  // meta_object_t 要求：重置内部 flags
  template<class ANOTHER_FLAGS>
  struct meta_set_helper {
    static_assert(meta_op_bits::is_flags_operator_v<ANOTHER_FLAGS>,
                  "meta_states_object::meta_set requires a flags_operator type");
    using type = meta_states_object<ANOTHER_FLAGS, Probe, Fn...>;
  };
  template<class ANOTHER_FLAGS>
  using meta_set = typename meta_set_helper<ANOTHER_FLAGS>::type;

  using type = flags;
  static constexpr u64 value = flags::value;
};

// 检测是否为 meta_states_object 的实例
template<class T>
struct is_meta_states_object : std::false_type {};
template<class Flags, class Probe, class ...Fn>
struct is_meta_states_object<meta_states_object<Flags, Probe, Fn...>> : std::true_type {};
template<class T>
constexpr bool is_meta_states_object_v = is_meta_states_object<T>::value;

namespace meta_objects_invoke_details {
// Treat the first meta-object as a meta-function and invoke it with the
// payload held by the second object.
template <class OBJ1, class OBJ2, class ...Args>
struct Meta_Object_Invoke {
  using type = meta_invoke<OBJ1, typename OBJ2::type, Args...>;
};

// For a meta_ret_object source, pass its projected result rather than its
// internal payload to the target meta-function.
// 禁用：当目标是 meta_states_object 时由目标特化接管（需要观察源对象本身）。
template <class OBJ1, class Obj2, class F, class Ret, class ...Args>
  requires(!is_meta_states_object_v<OBJ1>)
struct Meta_Object_Invoke<OBJ1, meta_ret_object<Obj2, F, Ret>, Args...> {
  using type = meta_invoke<OBJ1, typename meta_ret_object<Obj2, F, Ret>::ret, Args...>;
};

// meta_states_object as source: directly invoke OBJ1 on the states object
// itself (it is already a meta-object carrying flags), not on its ::type.
// 禁用：当目标也是 meta_states_object 时由目标特化接管。
template <class OBJ1, class Flags, class Probe, class ...Fn, class ...Args>
  requires(!is_meta_states_object_v<OBJ1>)
struct Meta_Object_Invoke<OBJ1, meta_states_object<Flags, Probe, Fn...>, Args...> {
  using type = meta_invoke<OBJ1, meta_states_object<Flags, Probe, Fn...>, Args...>;
};

// meta_states_object as destination: it must observe the *source object
// itself* (e.g. a generator's meta-object) to inspect its state, so pass the
// source meta-object through unmodified instead of unwrapping its ::type/ret.
template <class Flags, class Probe, class ...Fn, class OBJ2, class ...Args>
  requires meta_object_t<OBJ2>
struct Meta_Object_Invoke<meta_states_object<Flags, Probe, Fn...>, OBJ2, Args...> {
  using type = meta_invoke<meta_states_object<Flags, Probe, Fn...>, OBJ2, Args...>;
};

}  // namespace meta_objects_invoke_details
template <class OBJ1, class OBJ2>
using meta_object_invoke =
    typename meta_objects_invoke_details::Meta_Object_Invoke<OBJ1, OBJ2>::type;

namespace meta_objects_invoke_details {
// 按协议解包元对象的【产出】（不调用）：
//   meta_object      -> ::type
//   meta_ret_object  -> ::ret
// 这是 cache 的来源：生成器产出但未被接收器消费的值。
template <class MO, class = void>
struct meta_produce {
  using type = typename MO::type;  // 默认：meta_object 的产出是 ::type
};
template <class MO>
struct meta_produce<MO, std::void_t<typename MO::ret>> {
  using type = typename MO::ret;   // meta_ret_object 的产出是 ::ret
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

template <class Cond, class MO, class Generator>
struct meta_looper_impl<false, Cond, MO, Generator> {
  static constexpr bool _continue_ = false;
  using type = typename MO::type;
};

// ===== meta_states_object 作为条件的特化：flags 驱动 =====
// 当 Condition 是 meta_states_object 时，looper 行为由 flags 位指导：
//   - 用 states_object 探测 Generator 生成/更新 flags
//   - 按 flags 调用 loop_op<flags> 原子化推进（gen apply、结果喂 state）
//   - bit62 G_NOT_SKIP 只控制 for_each 是否调用用户回调
//   - “跳过”= 推进生成器但不喂接收器，由 slot1 的 S_APPLY 位指定
namespace states_looper_detail {
  using namespace meta_op_bits;

  // ===== gen_op：对生成器的原子操作（由 slot0 驱动）=====
  // 只负责推进/初始化生成器，返回新生成器。不触碰接收器。
  template<u64 flags, class Generator, class... Args>
  struct gen_op;

  // slot0 S_INIT：初始化生成器（未初始化形态，调 apply<Args...> 触发 initialize）。
  // 初始化接收 looper 的外部参数 Args。
  template<u64 flags, class Generator, class... Args>
    requires((flags & (static_cast<u64>(S_INIT) << SLOT0_SHIFT)) != 0)
  struct gen_op<flags, Generator, Args...> {
    using type = meta_invoke<Generator, Args...>;  // 初始化（带外部参数）
  };

  // slot0 S_APPLY（且无 S_INIT）：推进生成器。
  template<u64 flags, class Generator, class... Args>
    requires((flags & (static_cast<u64>(S_APPLY) << SLOT0_SHIFT)) != 0 &&
             (flags & (static_cast<u64>(S_INIT) << SLOT0_SHIFT)) == 0)
  struct gen_op<flags, Generator, Args...> {
    using type = meta_invoke<Generator, Args...>;
  };

  // slot0 无操作：生成器保持。
  template<u64 flags, class Generator, class... Args>
    requires((flags & (static_cast<u64>(S_APPLY) << SLOT0_SHIFT)) == 0 &&
             (flags & (static_cast<u64>(S_INIT) << SLOT0_SHIFT)) == 0)
  struct gen_op<flags, Generator, Args...> {
    using type = Generator;
  };

  // ===== recv_op：对接收器的原子操作（由 slot1 驱动）=====
  // 输入是生成器操作后的对象；拆包由 meta_object_invoke 决定。
  template<u64 flags, class OBJ, class GenResult>
  struct recv_op;

  // slot1 S_APPLY：用生成器结果 invoke 接收器。
  template<u64 flags, class OBJ, class GenResult>
    requires((flags & (static_cast<u64>(S_APPLY) << SLOT1_SHIFT)) != 0)
  struct recv_op<flags, OBJ, GenResult> {
    using type = meta_object_invoke<OBJ, GenResult>;
  };

  // slot1 无操作：接收器保持（跳过）。
  template<u64 flags, class OBJ, class GenResult>
    requires((flags & (static_cast<u64>(S_APPLY) << SLOT1_SHIFT)) == 0)
  struct recv_op<flags, OBJ, GenResult> {
    using type = OBJ;
  };

  // ===== loop_pack：打包本轮状态供用户回调 / 下一阶段（meta_pipe）解包 =====
  // 参考 meta_stream：打包 Generator/Receiver/State/Index，并暴露常用探测成员。
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
  // Gen=生成器操作后对象，RecvObj=接收器操作后对象（接收器结果），OldRecv=接收器操作前。
  template<u64 flags, class Gen, class RecvObj, class States>
  struct make_stage {
    static constexpr u64 idx = States::flags::index::value;
    using type = std::conditional_t<(flags & G_PACK) != 0,
        loop_pack<Gen, RecvObj, RecvObj, idx>,
        std::conditional_t<(flags & G_FEED_GEN) != 0,
            Gen, RecvObj>>;
  };
}  // namespace states_looper_detail

namespace states_looper_detail {
  // 递归下一迭代的分派：continue_ 为 false 时不实例化下一迭代（避免对空生成器再探测）。
  // 终止时产出最终 type：G_PACK 置位则为 pack，否则为接收器结果。
  template<u64 flags, bool continue_, class cur_states_t, class new_state, class new_gen, class... Args>
  struct next_step {
    using type = typename make_stage<flags, new_gen, new_state, cur_states_t>::type;
    template <class... arg_types>
    static constexpr decltype(auto) for_each(auto&&, arg_types&&...) {
      return;
    }
    template <class first_arg_type, class... arg_types>
    static constexpr decltype(auto) for_each_forward(auto&&, first_arg_type&&, arg_types&&...) {
      return;
    }
  };
  template<u64 flags, class cur_states_t, class new_state, class new_gen, class... Args>
  struct next_step<flags, true, cur_states_t, new_state, new_gen, Args...> {
    using recurse_apply_t = typename meta_looper_impl<true, cur_states_t, new_state, new_gen>
        ::template apply<Args...>;
    using type = typename recurse_apply_t::type;
    template <class... arg_types>
    static constexpr decltype(auto) for_each(auto&& f, arg_types&&... args) {
      return recurse_apply_t::for_each(f, std::forward<arg_types>(args)...);
    }
    template <class first_arg_type, class... arg_types>
    static constexpr decltype(auto) for_each_forward(auto&& f, first_arg_type&& first, arg_types&&... args) {
      return recurse_apply_t::for_each_forward(f, std::forward<first_arg_type>(first), std::forward<arg_types>(args)...);
    }
  };
}  // namespace states_looper_detail

// 特化：Condition 为 meta_states_object，分阶段流水线（生成器/接收器解耦）。
//   阶段A：states_object.probe_generator<Generator> → 定 slot0 → gen_op 推进/初始化生成器
//   阶段B：states_object.probe_receiver<GenResult>   → 定 slot1 → recv_op 喂接收器 → 递增 index
//   bit62 G_NOT_SKIP 只控制 for_each 是否调用用户回调。
template <class Flags, class Probe, class... Fn, class OBJ, class Generator>
struct meta_looper_impl<true, meta_states_object<Flags, Probe, Fn...>, OBJ, Generator> {
  using states_t = meta_states_object<Flags, Probe, Fn...>;

  template <class... Args>
  struct apply {
    // ===== 阶段 A：探测生成器，按 slot0 操作生成器 =====
    using gen_states_t = typename states_t::template probe_generator<Generator>;
    static constexpr meta_op_bits::u64 gen_flags_v = gen_states_t::flags::value;
    static constexpr bool continue_ = (gen_flags_v & meta_op_bits::G_CONTINUE) != 0;

    // 对生成器执行 slot0 操作（continue_ 为 false 时静止，避免推空生成器）
    using new_gen = typename std::conditional_t<continue_,
        states_looper_detail::gen_op<gen_flags_v, Generator, Args...>,
        states_looper_detail::gen_op<0, Generator, Args...>
        >::type;

    // ===== 阶段 B：探测接收器阶段，按 slot1 操作接收器，递增 index =====
    using final_states_t = typename gen_states_t::template probe_receiver<new_gen>;
    static constexpr meta_op_bits::u64 flags_v = final_states_t::flags::value;

    // 对接收器执行 slot1 操作（输入是生成器操作后的 new_gen）
    using new_state = typename std::conditional_t<continue_,
        states_looper_detail::recv_op<flags_v, OBJ, new_gen>,
        states_looper_detail::recv_op<0, OBJ, new_gen>
        >::type;

    // 递归下一迭代（continue_ 为 false 时不实例化）。
    // 最终产出的 type 由 next_step 一路传到终止轮产出（G_PACK 则为 pack）。
    using next_t = states_looper_detail::next_step<flags_v, continue_, final_states_t, new_state, new_gen, Args...>;
    using type = typename next_t::type;

    template <class... arg_types>
    static constexpr decltype(auto) for_each(auto&& f, arg_types&&... args) {
      // 投递给回调的 stage：G_PACK 置位时为 pack，否则按 G_FEED_GEN 选生成器/接收器结果
      using stage_t = typename states_looper_detail::make_stage<flags_v, new_gen, new_state, final_states_t>::type;
      // bit62 G_NOT_SKIP 即 observe：0 时编译期跳过回调分支——
      // 不推导 return_type、不实例化 f，只负责递归。
      constexpr bool observe = (flags_v & meta_op_bits::G_NOT_SKIP) != 0;

      if constexpr (!observe) {
        // 当前阶段不需要观察：不实例化 f，只递归。
        if constexpr (continue_) {
          return next_t::for_each(f, std::forward<arg_types>(args)...);
        } else {
          // 没有更多阶段，返回 void。
          return;
        }
      } else {
        using return_type =
            std::invoke_result_t<decltype(f), stage_t, arg_types...>;

        if constexpr (std::is_void_v<return_type>) {
          // 返回 void：检查下一层是否继续
          if constexpr (continue_) {
            std::invoke(f, stage_t{}, std::forward<arg_types>(args)...);
            return next_t::for_each(f, std::forward<arg_types>(args)...);
          } else {
            return std::invoke(f, stage_t{}, std::forward<arg_types>(args)...);
          }
        } else {
          // 返回非 void：不提前构造返回值。
          // 后续还要递归时丢弃当前返回值，否则直接返回当前调用结果。
          if constexpr (continue_) {
            (void)std::invoke(f, stage_t{}, std::forward<arg_types>(args)...);
            return next_t::for_each(f, std::forward<arg_types>(args)...);
          } else {
            return std::invoke(f, stage_t{}, std::forward<arg_types>(args)...);
          }
        }
      }
    }

    // 每轮消费一个参数 first 喂给 f(stage_t, first)，剩余 args 传给下一轮。
    // observe=false 时不实例化 f、不推导返回类型，仅按剩余参数继续递归。
    template <class first_arg_type, class... arg_types>
    static constexpr decltype(auto) for_each_forward(auto&& f, first_arg_type&& first, arg_types&&... args) {
      using stage_t = typename states_looper_detail::make_stage<flags_v, new_gen, new_state, final_states_t>::type;
      constexpr bool observe = (flags_v & meta_op_bits::G_NOT_SKIP) != 0;
      // 是否还有下一轮参数可供 forward
      constexpr bool has_next = continue_ && (sizeof...(arg_types) > 0);

      if constexpr (!observe) {
        // 不实例化 f：仅消费 first，按剩余参数递归。
        if constexpr (has_next) {
          return next_t::for_each_forward(f, std::forward<arg_types>(args)...);
        } else {
          return;
        }
      } else {
        using return_type =
            std::invoke_result_t<decltype(f), stage_t, first_arg_type>;

        if constexpr (std::is_void_v<return_type>) {
          if constexpr (has_next) {
            std::invoke(f, stage_t{}, std::forward<first_arg_type>(first));
            return next_t::for_each_forward(f, std::forward<arg_types>(args)...);
          } else {
            return std::invoke(f, stage_t{}, std::forward<first_arg_type>(first));
          }
        } else {
          if constexpr (has_next) {
            (void)std::invoke(f, stage_t{}, std::forward<first_arg_type>(first));
            return next_t::for_each_forward(f, std::forward<arg_types>(args)...);
          } else {
            return std::invoke(f, stage_t{}, std::forward<first_arg_type>(first));
          }
        }
      }
    }
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
