#pragma once

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
// Distinct from meta_empty (which is the payload of meta_empty_o, a valid empty
// value): this tag marks a meta object that has not been constructed yet. It is
// a pre-construction state and must never have operations (alignof, nested
// member access, ...) applied to it.
struct meta_uninitialized {};
}  // namespace meta_objects_details

namespace initialize_details {
template <template <class...> class apply_shape>
struct meta_initializer_container {};

template <class F, class En = void>
struct is_initializer_type : std::false_type {};

template <class F>
struct is_initializer_type<
    F, std::void_t<meta_initializer_container<F::template initialize>>>
    : std::true_type {};

template <class F>
constexpr bool has_initializer = is_initializer_type<F>::value;

template <class F>
struct initialized {
  template <class OBJ, class... Arg>
  using apply = meta_invoke<F, OBJ, Arg...>;
};

template <class F, class... Arg>
struct initialize {
  using type = typename F::template initialize<Arg...>;
};

// with_constructor: equip a meta function F with a constructor. While the host
// object still holds the Uninit tag, its first invocation runs F::initialize
// (the constructor); every later invocation runs F::apply. The branch is lazy
// (if constexpr), so the apply body is never formed over the uninitialized tag
// and cannot trigger e.g. alignof(meta_uninitialized) or missing members.
template <class F, class Uninit = meta_objects_details::meta_uninitialized>
struct with_constructor {
 private:
  template <class Obj, class... Args>
  consteval static auto step() {
    if constexpr (std::is_same_v<Obj, Uninit>) {
      return std::type_identity<
          typename F::template initialize<Obj, Args...>>{};
    } else {
      return std::type_identity<
          typename F::template apply<Obj, Args...>>{};
    }
  }

 public:
  template <class Obj, class... Args>
  using apply = typename decltype(step<Obj, Args...>())::type;
};
}  // namespace initialize_details

using initialize_details::has_initializer;
using initialize_details::initialize;
using initialize_details::initialized;
using initialize_details::with_constructor;

/*A meta obj is a bind of a meta_function and an obj, each time it is invoked,
it update itself to a new type, use ::type to get the inner obj. The object is a
dumb carrier: constructor (initialize) support is added by wrapping F with
with_constructor, not by specializing the object.*/
template <class OBJ, class F /*Define how to Update an obj*/>
struct meta_object {
  using function = F;
  using type = OBJ;
  template <class... Arg>
  using apply = meta_object<meta_invoke<F, OBJ, Arg...>, F>;

  template <class ANOTHER_OBJ>
  using meta_set = meta_object<ANOTHER_OBJ, F>;
};

// Construct a meta_object that needs no usable seed: it starts holding the
// meta_uninitialized tag, F::initialize runs on the first invocation (the
// constructor) and F::apply on every later one.
template <class F>
using meta_object_construct =
    meta_object<meta_objects_details::meta_uninitialized, with_constructor<F>>;

// Backwards-compatible name (old meta_object_init required has_initializer<F>).
template <class F>
using meta_object_init = meta_object_construct<F>;

// A meta_ret_object is a meta_object that also exposes ::ret (a value derived
// from its state via Ret), which lets it act as a source/transform. Like
// meta_object it is a dumb, single-definition carrier. Constructor support is
// added compositionally: wrap F with with_constructor for the update, and have
// Ret map the uninitialized tag to an idle value supplied by the upper layer.
template <class OBJ, class F, class Ret>
struct meta_ret_object {
  using function = F;
  using ret = meta_invoke<Ret, OBJ>;
  using type = OBJ;
  template <class... Arg>
  using apply = meta_ret_object<meta_invoke<F, OBJ, Arg...>, F, Ret>;

  template <class ANOTHER_OBJ>
  using meta_set = meta_ret_object<ANOTHER_OBJ, F, Ret>;
};

namespace meta_states_details {
// low 56 bits: change history
// high 8 bits: opr_code (never touched by next_flags)
constexpr std::uint64_t FLAGS_LOW_MASK = (1ULL << 56) - 1;

template <bool Changed, std::uint64_t Flags, std::size_t Index>
consteval std::uint64_t next_flags() {
  // extract high 8 bits (opr_code)
  constexpr std::uint64_t opr_code = Flags & ~FLAGS_LOW_MASK;
  // low 56 bits: change history
  constexpr std::uint64_t low_flags = Flags & FLAGS_LOW_MASK;

  if constexpr (Index >= 56) {
    // overflow: reset low 56 bits to 0, keep high bits
    if constexpr (Changed) {
      return opr_code | 1ULL;  // reset and set first bit
    } else {
      return opr_code;  // just reset
    }
  } else {
    if constexpr (Changed) {
      return opr_code | (low_flags | (1ULL << Index));
    } else {
      return Flags;
    }
  }
}

// detect whether F::apply accepts an extra integral_constant<uint64_t, flags>
template <class F, class Obj, class... Args>
concept apply_takes_flags = requires {
  typename F::template apply<Obj, Args...,
                             std::integral_constant<std::uint64_t, 0>>;
};

// detect on_changed<Obj, Args...>
template <class F, class Obj, class... Args>
concept has_on_changed =
    requires { typename F::template on_changed<Obj, Args...>; };

template <class F, class Obj, bool Changed, std::uint64_t Flags, class... Args>
consteval auto compute_next_type() {
  if constexpr (Changed && has_on_changed<F, Obj, Args...>) {
    return std::type_identity<typename F::template on_changed<Obj, Args...>>{};
  } else if constexpr (apply_takes_flags<F, Obj, Args...>) {
    return std::type_identity<typename F::template apply<
        Obj, Args..., std::integral_constant<std::uint64_t, Flags>>>{};
  } else {
    return std::type_identity<typename F::template apply<Obj, Args...>>{};
  }
}
}  // namespace meta_states_details

template <class OBJ, class F, class Changed_Pred, std::uint64_t byte_flag = 0,
          std::size_t flag_index = 0>
  requires meta_function_t<F> && meta_function_t<Changed_Pred>
struct meta_states_object {
  using type = OBJ;
  using function = F;
  using changed_pred = Changed_Pred;
  static constexpr std::uint64_t flags = byte_flag;
  static constexpr std::size_t size = flag_index;
  static constexpr bool last_changed =
      flag_index != 0 &&
      ((byte_flag >> (flag_index - 1)) & std::uint64_t{1}) != 0;

  template <class ANOTHER_OBJ>
  using meta_set =
      meta_states_object<ANOTHER_OBJ, F, Changed_Pred, byte_flag, flag_index>;

  template <std::size_t I>
    requires(I < flag_index && I < std::numeric_limits<std::uint64_t>::digits)
  static constexpr bool at() noexcept {
    return ((byte_flag >> I) & std::uint64_t{1}) != 0;
  }

  template <class... Args>
  using changed = meta_invoke<Changed_Pred, OBJ, Args...>;

  template <class... Args>
  static consteval std::uint64_t new_flags() {
    return meta_states_details::next_flags<changed<Args...>::value, byte_flag,
                                           flag_index>();
  }

  template <class... Args>
  using next_type = typename decltype(meta_states_details::compute_next_type<
                                      F, OBJ, changed<Args...>::value,
                                      new_flags<Args...>(), Args...>())::type;

  // reset_flag_index: reset flag_index to 0, keep everything else
  using reset_flag_index = meta_states_object<OBJ, F, Changed_Pred,
                                             byte_flag, 0>;

  // apply_impl: helper to avoid instantiating both branches of conditional
  template <bool Overflow, class... Args>
  struct apply_impl;

  template <class... Args>
  struct apply_impl<false, Args...> {
    using type = meta_states_object<next_type<Args...>, F, Changed_Pred,
                                   new_flags<Args...>(), flag_index + 1>;
  };

  template <class... Args>
  struct apply_impl<true, Args...> {
    using type = meta_states_object<next_type<Args...>, F, Changed_Pred,
                                   new_flags<Args...>(), 0>;
  };

  template <class... Args>
  using apply = typename apply_impl<(flag_index >= 56), Args...>::type;
};

namespace meta_states_details {
template <class T>
consteval bool changed_value() {
  if constexpr (requires { T::last_changed; }) {
    return T::last_changed;
  }
  return true;
}
}  // namespace meta_states_details

// observe_default: the default, generic observer for the looper. It is a
// unary meta-function that reports whether a stage changed by reading the
// states object's own change bit (changed_value). It is the looper's runtime
// callback gate; it is distinct from the states change predicate a user picks
// (ostream/istream/cache/stream_observer).
struct observe_default {
  template <class Stage>
  using apply = std::bool_constant<meta_states_details::changed_value<Stage>()>;
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

// since modifying to timer is forbidden, there is no initializer for
// meta_timer_object
template <std::size_t times, class OBJ, class F,
          class break_f =
              // when true, looper breaks
          meta_timer_object_details::meta_always_continue>
struct meta_timer_object {
  using timer = meta_invoke<

      // meta_break_if<Pred, Default_if_false_t>, Arg>
      // if(Pred<Arg> == true) return meta_break_signal
      // else return Default_if_false_t
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

// set a break condition for meta_timer_object
template <class MTO, class BF>
using break_if =
    exp_utilities::get_type<meta_timer_object_details::Break_If<MTO, BF>>;

namespace meta_objects_invoke_details {
template <class From_T, class To_T>
struct meta_transfer_object_impl {
  using type = typename To_T::template meta_set<typename From_T::type>;
};

// transfer timer if invoke to a meta_timer_oect
template <size_t times, class obj, class F, class To_T, class B>
struct meta_transfer_object_impl<meta_timer_object<times, obj, F, B>, To_T> {
  using type = typename To_T::template meta_set<
      typename meta_timer_object<times, obj, F, B>::timer>;
};

// transfer returns if invoke to a meta_ret_object
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

namespace meta_objects_invoke_details {
// the meta_object is itself a meta_function
// if two meta_objects invoked, invoke the first object with type in second
// object
template <class OBJ1, class OBJ2>
struct Meta_Object_Invoke {
  using type = meta_invoke<OBJ1, typename OBJ2::type>;
};

// if invoke with meta_ret_object, invoke the first object with returns
template <class OBJ1, class Obj2, class F, class Ret>
struct Meta_Object_Invoke<OBJ1, meta_ret_object<Obj2, F, Ret>> {
  using type = meta_invoke<OBJ1, typename meta_ret_object<Obj2, F, Ret>::ret>;
};

}  // namespace meta_objects_invoke_details
template <class OBJ1, class OBJ2>
using meta_object_invoke =
    typename meta_objects_invoke_details::Meta_Object_Invoke<OBJ1, OBJ2>::type;

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

// Note: All template parameters are meta objects
namespace meta_looper_detail {
template <bool, class Condition, class OBJ, class Generator = meta_empty_o,
          class Observer = observe_default>
struct meta_looper_impl {
  template <class... Args>
  struct apply {
    // transfer current obj to  condition_obj to judge
    // transfer different context based on types of meta_object
    using _continue_t =
        typename meta_invoke<meta_transfer_object<OBJ, Condition>>::type;
    static const bool _continue_ = _continue_t::value;

    // invoke generator object if condition is true
    using generator_stage_o =
        meta_invoke<invoke_if<_continue_>, Generator, Args...>;

    // invoke Obj object if condition is true
    using result_stage_o =
        meta_invoke<invoke_object_if<_continue_>, OBJ, generator_stage_o>;
    using observe_result = meta_invoke<Observer, result_stage_o>;

    // recursively loop for result
    using track_apply_t =
        meta_invoke<invoke_if<_continue_>,
                    meta_looper_impl<_continue_, Condition, result_stage_o,
                                     generator_stage_o, Observer>,
                    Args...>;
    using type = typename track_apply_t::type;

    template <class... arg_types>
    static constexpr decltype(auto) for_each(auto&& f, arg_types&&... args) {
      // 把当前阶段类型抽出来，后面推导返回类型时更干净。
      using stage_t = typename result_stage_o::type;

      if constexpr (!observe_result::value) {
        // 当前阶段不需要观察：不实例化 f，只负责继续递归。
        if constexpr (_continue_) {
          return track_apply_t::for_each(f, std::forward<arg_types>(args)...);
        } else {
          // No more stages ahead. Return void.
          return;
        }
      } else {
        // 直接用 std::invoke_result_t 推导 f(stage_t{}, args...) 的返回类型，
        // 不再手写 decltype(std::invoke(...))。
        using return_type =
            std::invoke_result_t<decltype(f), stage_t, arg_types...>;

        if constexpr (std::is_void_v<return_type>) {
          // 返回 void：和非 void 分支一样，检查下一层是否继续
          // 下一层继续：调用后递归；下一层停止：调用后直接结束
          if constexpr (track_apply_t::_continue_) {
            std::invoke(f, stage_t{}, std::forward<arg_types>(args)...);
            return track_apply_t::for_each(f, std::forward<arg_types>(args)...);
          } else {
            return std::invoke(f, stage_t{}, std::forward<arg_types>(args)...);
          }
        } else {
          // 返回非 void：不提前构造 ret_val。
          // 后续还要递归时丢弃当前返回值，否则直接返回当前调用结果。
          if constexpr (track_apply_t::_continue_) {
            (void)std::invoke(f, stage_t{}, std::forward<arg_types>(args)...);
            return track_apply_t::for_each(f,
                                           std::forward<arg_types>(args)...);
          } else {
            return std::invoke(f, stage_t{},
                               std::forward<arg_types>(args)...);
          }
        }
      }
    }

    // Overload: fold a pack of unary metafunctions into each stage BEFORE the
    // value is handed to the callable f. Only the value passed to f is folded;
    // the loop machinery is unchanged.
    template <template <class> class... Ps, class... arg_types>
      requires(sizeof...(Ps) > 0)
    static constexpr decltype(auto) for_each(auto&& f, arg_types&&... args) {
      using raw_stage = typename result_stage_o::type;
      using stage_t = meta_fold<raw_stage, Ps...>;

      if constexpr (!observe_result::value) {
        if constexpr (_continue_) {
          return track_apply_t::template for_each<Ps...>(
              f, std::forward<arg_types>(args)...);
        } else {
          return;
        }
      } else {
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
    }

    template <class first_arg_type, class... arg_types>
    static constexpr decltype(auto) for_each_forward(auto&& f,
                                                     first_arg_type&& first,
                                                     arg_types&&... args) {
      // 同样抽出当前阶段类型。
      using stage_t = typename result_stage_o::type;

      if constexpr (!observe_result::value) {
        if constexpr (_continue_ && sizeof...(arg_types)) {
          return track_apply_t::for_each_forward(
              f, std::forward<arg_types>(args)...);
        } else {
          // No more observing stages ahead. Return void.
          return;
        }
      } else {
        // 只推导对第一个参数调用时的返回类型。
        using return_type =
            std::invoke_result_t<decltype(f), stage_t, first_arg_type>;

        if constexpr (std::is_void_v<return_type>) {
          // 和非 void 分支一样，检查下一层是否继续且还有参数
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
    }

    // Overload: unary metafunctions folded into each stage before calling f.
    template <template <class> class... Ps, class first_arg_type,
              class... arg_types>
      requires(sizeof...(Ps) > 0)
    static constexpr decltype(auto) for_each_forward(
        auto&& f, first_arg_type&& first, arg_types&&... args) {
      using raw_stage = typename result_stage_o::type;
      using stage_t = meta_fold<raw_stage, Ps...>;

      if constexpr (!observe_result::value) {
        if constexpr (_continue_ && sizeof...(arg_types)) {
          return track_apply_t::template for_each_forward<Ps...>(
              f, std::forward<arg_types>(args)...);
        } else {
          return;
        }
      } else {
        using return_type =
            std::invoke_result_t<decltype(f), stage_t, first_arg_type>;
        if constexpr (std::is_void_v<return_type>) {
          if constexpr (track_apply_t::_continue_ &&
                        sizeof...(arg_types)) {
            std::invoke(f, stage_t{}, std::forward<first_arg_type>(first));
            return track_apply_t::template for_each_forward<Ps...>(
                f, std::forward<arg_types>(args)...);
          } else {
            return std::invoke(f, stage_t{},
                               std::forward<first_arg_type>(first));
          }
        } else {
          if constexpr (track_apply_t::_continue_ &&
                        sizeof...(arg_types)) {
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
    }
  };
};

template <class Cond, class MO, class Generator, class Observer>
struct meta_looper_impl<false, Cond, MO, Generator, Observer> {
  static constexpr bool _continue_ = false;
  using type = typename MO::type;
};
}  // namespace meta_looper_detail

template <class C, class O, class G, class Observer = observe_default,
          class... ARG_Tys>
using meta_looper_t = typename meta_invoke<
    meta_looper_detail::meta_looper_impl<true, C, O, G, Observer>,
    ARG_Tys...>::type;

template <class C, class O, class G = meta_empty_o,
          class Observer = observe_default>
using meta_looper =
    meta_looper_detail::meta_looper_impl<true, C, O, G, Observer>;
}  // namespace meta_loop
