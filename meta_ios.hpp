#pragma once

#include "meta_objects.hpp"

namespace meta_ios {
// Forward declaration: meta_stream_skip_signal is defined later in protocols,
// but observers / io_stream_transform_details must name it in specializations.
namespace protocols {
struct meta_stream_skip_signal;
}  // namespace protocols

template <bool End, class EndType = exp_utilities::literal_types::end_of_list>
struct end_of_stream {
  static constexpr bool end = End;
  using end_type = EndType;
};

using namespace meta_invoke_protocols;
using namespace meta_objects;

//=== stream-specific observe helpers ===
namespace stream_observe_details {
template <class Stage>
consteval bool output_changed() {
  if constexpr (requires { typename Stage::type::to; }) {
    return meta_states_details::changed_value<typename Stage::type::to>();
  } else if constexpr (requires { typename Stage::to; }) {
    return meta_states_details::changed_value<typename Stage::to>();
  }
  return meta_states_details::changed_value<Stage>();
}

template <class Stage>
consteval bool input_changed() {
  if constexpr (requires { typename Stage::type::from::type; }) {
    return meta_states_details::changed_value<
        typename Stage::type::from::type>();
  } else if constexpr (requires { typename Stage::from::type; }) {
    return meta_states_details::changed_value<typename Stage::from::type>();
  }
  return meta_states_details::changed_value<Stage>();
}

// preview what next stream would be if F is applied.
// uses plain 2-arg apply; if F also accepts a bool_constant as 3rd arg,
// pass std::false_type for the preview.
template <class F, class Current, class FromIs, class = void>
struct preview_next {
  using type = meta_invoke<F, Current, FromIs>;
};

template <class F, class Current, class FromIs>
struct preview_next<
    F, Current, FromIs,
    std::void_t<typename F::template apply<Current, FromIs, std::false_type>>> {
  using type = typename F::template apply<Current, FromIs, std::false_type>;
};
}  // namespace stream_observe_details

struct observe_ostream {
  template <class Stage>
  using apply =
      std::bool_constant<stream_observe_details::output_changed<Stage>()>;
};

struct observe_istream {
  template <class Stage>
  using apply =
      std::bool_constant<stream_observe_details::input_changed<Stage>()>;
};

template <class F>
struct observe_os_change {
  template <class ThisObj, class FromIs>
  struct apply {
    using cache_t = typename ThisObj::cache;
    static constexpr bool value = [] {
      if constexpr (requires { typename ThisObj::to::changed_pred; }) {
        constexpr std::uint64_t op_skip = std::uint64_t{1} << 56;
        if constexpr ((ThisObj::to::flags & op_skip) == 0)
          return true;
        else
          return ThisObj::to::template changed<cache_t>::value;
      } else {
        using next_t =
            typename stream_observe_details::preview_next<F, ThisObj,
                                                          FromIs>::type;
        return !std::is_same_v<typename ThisObj::to::type,
                               typename next_t::to::type>;
      }
    }();
  };
};

template <class F>
struct observe_is_change {
  template <class ThisObj, class FromIs>
  struct apply {
    using next_t =
        typename stream_observe_details::preview_next<F, ThisObj, FromIs>::type;
    static constexpr bool value = !std::is_same_v<typename ThisObj::from::type,
                                                  typename next_t::from::type>;
  };
};

template <class F>
struct observe_cache_change {
  template <class ThisObj, class FromIs>
  struct apply {
    using next_t =
        typename stream_observe_details::preview_next<F, ThisObj, FromIs>::type;
    static constexpr bool value =
        !std::is_same_v<typename ThisObj::cache, typename next_t::cache>;
  };
};

template <class F>
struct observe_whole_change {
  template <class ThisObj, class FromIs>
  struct apply {
    using next_t =
        typename stream_observe_details::preview_next<F, ThisObj, FromIs>::type;
    static constexpr bool value =
        !std::is_same_v<typename ThisObj::to::type,
                        typename next_t::to::type> ||
        !std::is_same_v<typename ThisObj::from::type,
                        typename next_t::from::type> ||
        !std::is_same_v<typename ThisObj::cache, typename next_t::cache>;
  };
};

namespace io_stream_transform_details {
namespace transfer_protocols {
namespace details {
template <class meta_function_type>
struct this_policy_type {
  template <class this_type, class from_ins>
  using apply = meta_invoke<meta_function_type, this_type>;
};

template <class meta_function_type>
struct arg_policy_type {
  template <class this_type, class from_ins>
  using apply = meta_invoke<meta_function_type, from_ins>;
};

template <class meta_function_type>
struct stream_to_policy_type {
  template <class this_stream_t, class from_ins>
  using apply = meta_invoke<meta_function_type,
                            typename this_stream_t::to::type, from_ins>;
};

template <class meta_function_type>
struct stream_to_arg_type {
  template <class this_stream_t, class from_ins>
  using apply = meta_invoke<meta_function_type, from_ins>;
};

template <class meta_function_type>
struct stream_to_this_policy_type {
  template <class this_stream_t, class from_ins>
  using apply =
      meta_invoke<meta_function_type, typename this_stream_t::to::type>;
};
template <class meta_function_type>
  requires(meta_alias_argc<meta_function_type::template apply>() == 1)
struct stream_to_this_policy_type<meta_function_type> {
  template <class this_stream_t>
  using apply =
      meta_invoke<meta_function_type, typename this_stream_t::to::type>;
};

template <class meta_function_type>
struct stream_cache_policy_type {
  template <class this_stream_t, class from_ins>
  using apply =
      meta_invoke<meta_function_type, typename this_stream_t::cache, from_ins>;
};

template <class meta_function_type>
struct stream_from_only_policy_type {
  template <class this_stream_t>
  using apply =
      meta_invoke<meta_function_type, typename this_stream_t::from::type>;
};

template <class meta_function_type>
struct stream_cache_this_policy_type {
  template <class this_stream_t>
  using apply = meta_invoke<meta_function_type, typename this_stream_t::cache>;
};

}  // namespace details
}  // namespace transfer_protocols
}  // namespace io_stream_transform_details
namespace io_stream_transform_details {
using exp_utilities::exp_list;
using exp_utilities::get_type;
using exp_utilities::to_exp_list_t;
using meta_objects::meta_object;
using meta_objects::meta_ret_object;
using meta_quote::binary;
using meta_quote::unary;

namespace meta_basic_istream_detail {
template <class this_list>
using dec_f = typename this_list::pop_front;
template <class this_list>
using pop_f = typename this_list::front;

template <class TL>
using meta_basic_istream =
    meta_ret_object<to_exp_list_t<TL>, unary<dec_f>, unary<pop_f>>;
}  // namespace meta_basic_istream_detail

namespace meta_reverse_istream_detail {
template <class this_list>
using read_last = typename this_list::back;

template <class this_list>
using pop_last_f = exp_utilities::pop_last<this_list>;

template <class type_list>
using meta_reverse_istream =
    meta_ret_object<to_exp_list_t<type_list>, unary<pop_last_f>,
                    unary<read_last>>;
}  // namespace meta_reverse_istream_detail

namespace meta_basic_ostream_detail {

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
}  // namespace meta_basic_ostream_detail

namespace meta_transform_istream_detail {
using meta_basic_istream_detail::dec_f;
using meta_basic_istream_detail::pop_f;

template <class meta_function_type, class this_list>
using pop_transform_f = meta_invoke<meta_function_type, pop_f<this_list>>;

template <class TL, class meta_function_type>
using meta_basic_transform_istream = meta_ret_object<
    to_exp_list_t<TL>, unary<dec_f>,
    meta_quote::bind_binary<pop_transform_f, meta_function_type>>;
}  // namespace meta_transform_istream_detail

namespace meta_transform_ostream_detail {
using meta_basic_ostream_detail::add_f;

template <class meta_function_type>
struct add_transform_f {
  template <class this_list, class from_ins>
  using apply =
      add_f<this_list, meta_invoke<meta_function_type, this_list, from_ins>>;
};

template <class TL, class meta_function_type>
using meta_basic_transform_ostream =
    meta_object<TL, add_transform_f<meta_function_type>>;
}  // namespace meta_transform_ostream_detail

namespace meta_join_ostream_detail {
using meta_basic_ostream_detail::add_f;

template <class T>
struct add_to_this_list {
  template <class this_list>
  using apply = add_f<this_list, T>;
};

template <class this_list, class T>
struct auto_join_f {
  using type = add_f<this_list, T>;
};

template <class this_list, template <class...> class L, class... Tys>
struct auto_join_f<this_list, L<Tys...>> {
  using type = meta_fold<this_list, add_to_this_list<Tys>::template apply...>;
};

struct auto_join {
  template <class this_list, class T>
  using apply = typename auto_join_f<this_list, T>::type;
};

template <class TL>
using join_ostream = meta_object<TL, auto_join>;
}  // namespace meta_join_ostream_detail

namespace meta_reverse_ostream_detail {
// add_front: prepend T to ANY type-list template, preserving that template
// (it never forces the list to become exp_list). This is the front-inserting
// counterpart of meta_basic_ostream_detail::add_impl (which appends).
template <class TL, class T>
struct add_front_impl {
  using type = exp_utilities::literal_types::no_exist_type;
};
template <template <class...> class L, class... Tys, class T>
struct add_front_impl<L<Tys...>, T> {
  using type = L<T, Tys...>;
};
template <class TL, class T>
using add_front_f = typename add_front_impl<TL, T>::type;

// reverse_collect_f: each step prepends the incoming element, so the final
// list holds elements in reverse arrival order.
struct reverse_collect_f {
  template <class this_list, class from_ins>
  using apply = add_front_f<this_list, from_ins>;
};

// Built-in reverse-collecting ostream. TL defaults to exp_list<>; whatever
// template TL uses is preserved by add_front.
template <class TL = exp_list<>>
using reverse_ostream = meta_object<TL, reverse_collect_f>;
}  // namespace meta_reverse_ostream_detail

namespace meta_filter_ostream_detail {
using meta_basic_ostream_detail::add_f;

template <class meta_function_type>
struct add_filter_f {
  template <class this_list, class from_ins>
  using apply = std::conditional_t<
      meta_invoke<meta_function_type, this_list, from_ins>::value,
      add_f<this_list, from_ins>, this_list>;
};

template <class TL, class filter>
using filter_ostream = meta_object<TL, add_filter_f<filter>>;
}  // namespace meta_filter_ostream_detail

// A states-based ostream: wraps a type_list inside meta_states_object.
// accept_pred(current_list, from_ins) == true  -> append (changed)
// accept_pred(current_list, from_ins) == false -> keep list unchanged (not
// changed) last_changed on the resulting meta_states_object tells the looper
// whether for_each should fire for this step.
namespace meta_states_ostream_detail {
using meta_basic_ostream_detail::add_f;

template <class accept_pred>
struct states_ostream_f {
  template <class current_list, class from_ins>
  using apply = std::conditional_t<
      meta_invoke<accept_pred, current_list, from_ins>::value,
      add_f<current_list, from_ins>, current_list>;
};
}  // namespace meta_states_ostream_detail

// A states-based iterator ostream: position lives in its own OBJ state,
// just like meta_aligned_iterator stores advance_t in seek_to.
namespace meta_states_iterator_detail {
// iterator: OBJ is just the flags itself. F is identity —
// the third arg already carries the folded flags, so OBJ == Flags.
struct iterator_passthrough {
  template <class Obj, class FromIs, class Flags>
  using apply = Flags;
};

struct always_changed {
  template <class...>
  struct apply : std::true_type {};
};
}  // namespace meta_states_iterator_detail

namespace meta_index_istream_detail {
using exp_utilities::Idx;
using exp_utilities::inc_idx_t;

// to prevent the istream from ending because of exp_size<obj> == 0;
// exp_size<exp_list<Idx<start>>> == 1, so the istream will not end
template <std::size_t start>
using my_counter = exp_list<Idx<start>>;

template <class this_list>
using make_inc_type = exp_list<inc_idx_t<typename this_list::template at<0>>>;

template <class this_list>
using ret_index = typename this_list::template at<0>;

template <std::size_t start>
using index_istream =
    meta_ret_object<my_counter<start>, meta_quote::unary<make_inc_type>,
                    meta_quote::unary<ret_index>>;

}  // namespace meta_index_istream_detail

namespace ranged_based_index_sequence_istream_detail {
template <std::size_t dec_index>
struct index_counter : std::integral_constant<std::size_t, dec_index>,
                       end_of_stream<(dec_index == 0)> {
  static constexpr std::size_t length = dec_index;
  using dec_t = index_counter<dec_index - 1>;
};

template <std::size_t Len>
struct dec_index_f {
  template <class this_index, class...>
  using apply = typename this_index::dec_t;
};

template <std::size_t Len>
struct ret_index {
  // The produced position is exposed as a plain std::integral_constant (not as
  // the state type index_counter itself), so exact-type conditions such as
  // std::is_same<cache, std::integral_constant<std::size_t, K>> match. The
  // loop state remains index_counter; only the returned cache is normalized.
  template <class this_index, class...>
  using apply =
      std::integral_constant<std::size_t, Len - this_index::value>;
};

template <std::size_t Len>
using ranged_index_istream =
    meta_ret_object<index_counter<Len>, dec_index_f<Len>, ret_index<Len>>;
}  // namespace ranged_based_index_sequence_istream_detail

namespace meta_replace_able_ostream_detail {
using exp_utilities::literal_types::no_exist_type;
template <class this_obj, class T>
using replace_this = T;

using replace_able_ostream =
    meta_object<no_exist_type, meta_quote::binary<replace_this>>;
}  // namespace meta_replace_able_ostream_detail
namespace meta_transform_iterator_detail {

using exp_utilities::literal_types::no_exist_type;
template <class F>
struct transform_iterator_f {
  

  template <class this_obj, class from_ins>
  using apply = meta_invoke<F, this_obj, from_ins>;

  template<class from_ins>
  using initialize = initialize<F, from_ins>;
};

template<bool con, meta_object_t mo, class T>
using set_if = std::conditional_t<con, typename mo::meta_set<T>, mo>;


template <class F, typename T>
using transform_iterator = set_if<
                            !std::is_same_v<no_exist_type, T>, 
                            meta_object<transform_iterator_f<F>>, T>;
}  // namespace meta_transform_iterator_detail

namespace meta_self_repeat_ostream_detail {
template <class this_obj>
using ret_self = typename this_obj::template at<0>;

template <class this_obj>
using do_nothing = this_obj;

template <class T>
using self_ostream = meta_ret_object<exp_list<T>, meta_quote::unary<do_nothing>,
                                     meta_quote::unary<ret_self>>;
}  // namespace meta_self_repeat_ostream_detail
namespace meta_forward_ostream_details {
using exp_utilities::exp_select;
using exp_utilities::exp_size;
using meta_basic_ostream_detail::meta_basic_ostream;
template <std::size_t L, class TL, class F>
struct forward_ostream_f_impl {
  // get current index according to the length of this_list, and apply F to the
  // current index and the corresponding type in TL
  template <class this_list, class Arg>
  struct apply {
    static constexpr std::size_t current_index =
        to_exp_list_t<this_list>::length;
    using result = meta_invoke<F, exp_select<current_index, TL>, Arg>;
    using type =
        typename meta_invoke<meta_basic_ostream<this_list>, result>::type;
  };
  // some istream don't empty itself, it cause the stream to invoke the length 0
  // istream to cause en error, so when the length of this_list is greater than
  // or equal to L, it means the stream is empty, just return this_list without
  // applying F
  template <class this_list, class Arg>
    requires(to_exp_list_t<this_list>::length >= L)
  struct apply<this_list, Arg> {
    using type = this_list;
  };
};
template <class TL, class F>
struct forward_ostream_f {
  template <class this_list, class Arg>
  using apply =
      typename forward_ostream_f_impl<exp_size<TL>, TL,
                                      F>::template apply<this_list, Arg>::type;
};
template <class TL, class F>
using meta_forward_ostream = meta_object<exp_list<>, forward_ostream_f<TL, F>>;
}  // namespace meta_forward_ostream_details
namespace timer_condition_details {
struct timer_receiver {
  template <class timer, class...>
  struct apply : timer {};
};
};

namespace states_condition_details {
struct states_break_cond {
  template <class flags_t>
  struct apply : std::bool_constant<(flags_t::value & (1ULL << 63)) != 0> {};
};
}  // namespace states_condition_details

using meta_timer_cond_o =
    meta_object<void, timer_condition_details::timer_receiver>;
using meta_states_cond_o =
    meta_object<void, states_condition_details::states_break_cond>;
}  // namespace io_stream_transform_details
using meta_loop::meta_looper;
using meta_loop::meta_looper_t;
using meta_objects::invoke_object_if;
using meta_objects::meta_empty_o;
using meta_objects::meta_object;
using meta_objects::meta_ret_object;
using meta_objects::meta_timer_object;
using meta_objects::meta_timer_object_details::meta_always_continue;
using namespace exp_utilities;

namespace io_stream_transform_details {
namespace io_stream_traits {
template <class T>
struct is_meta_object : std::false_type {};

template <class OBJ, class F>
struct is_meta_object<meta_object<OBJ, F>> : std::true_type {};
template <class OBJ, class F, class Ret>
struct is_meta_object<meta_ret_object<OBJ, F, Ret>> : std::true_type {};
template <class OBJ, class F, class Changed_Pred, class break_f,
          std::uint64_t byte_flag, std::size_t flag_index>
struct is_meta_object<
    meta_states_object<OBJ, F, Changed_Pred, break_f, byte_flag, flag_index>>
    : std::true_type {};

template <class T>
constexpr bool is_meta_object_v = is_meta_object<T>::value;

// Detect a meta_states_object specifically (as opposed to a plain
// meta_object / meta_ret_object), so meta_stream can expose the states
// introspection members only when its ostream carries states.
template <class T>
struct is_meta_states_object : std::false_type {};
template <class OBJ, class F, class Changed_Pred, class break_f,
          std::uint64_t byte_flag, std::size_t flag_index>
struct is_meta_states_object<
    meta_states_object<OBJ, F, Changed_Pred, break_f, byte_flag, flag_index>>
    : std::true_type {};
template <class T>
constexpr bool is_meta_states_object_v = is_meta_states_object<T>::value;


// the meta_istream_type must be a meta_ret_object
template <class T>
concept meta_istream_t = meta_ret_object_t<T>;

// the meta_ostream_type can be any type of meta_object
template <class T>
concept meta_ostream_t = meta_object_t<T>;

}  // namespace io_stream_traits

template <class T>
concept has_end_state = requires {
  { T::end } -> std::convertible_to<bool>;
  typename T::end_type;
};

template <class T>
concept is_end_stream = has_end_state<T> && T::end;

template <class From, bool = is_end_stream<typename From::type>>
struct stream_cache;

template <class From>
struct stream_cache<From, false> {
  using type = typename From::ret;
};

template <class From>
struct stream_cache<From, true> {
  using type = typename From::type::end_type;
};

#include <typeinfo>

template <io_stream_traits::meta_ostream_t To,
          io_stream_traits::meta_istream_t From>
struct meta_stream {
  using from = From;
  using to = To;
  using from_t = typename from::type;
  using to_t = typename to::type;
  using cache = typename stream_cache<From>::type;

  // Whether this stream position has no more input to process.
  // True for end-marker states (e.g. index_counter<0>) or when the
  // cache resolved to end_of_list (exp_list exhausted).
  static constexpr bool end =
      is_end_stream<from_t> ||
      std::is_same_v<cache, literal_types::end_of_list>;
  using end_type = literal_types::end_of_list;

  template <class... Arg>
  using invoke_to = meta_stream<meta_invoke<To, Arg...>, from>;

  constexpr std::type_info const& target_type() const { return typeid(to_t); }
  constexpr to_t object() const { return to_t{}; }
  consteval auto value() const {
    if constexpr (has_value<to_t>) {
      return to_t::value;

    } else {
      return literal_types::no_exist_type{};
    }
  }
  consteval std::size_t left() const { return exp_size<from_t>; }

  // States introspection: available only when the ostream (To) is a
  // meta_states_object. Expose the states flag index (To::size is the 1-based
  // count of recorded steps, so the current 0-based index is size - 1), the
  // raw flags word, and the last-step change bit. For a non-states To these
  // members are not declared at all.
  consteval std::size_t index() const
    requires io_stream_traits::is_meta_states_object_v<To> {
    return To::size - 1;
  }
  consteval std::uint64_t flags() const
    requires io_stream_traits::is_meta_states_object_v<To> {
    return To::flags;
  }
  consteval bool last_changed() const
    requires io_stream_traits::is_meta_states_object_v<To> {
    return To::last_changed;
  }
};
template <class meta_stream_t>
struct meta_stream_update {
  using type = meta_stream_t;
};

template <class To, class From>
  requires(!length_equal<typename From::type, 0>)
struct meta_stream_update<meta_stream<To, From>> {
  using type = meta_stream<meta_object_invoke<To, From>, meta_invoke<From>>;
};

struct meta_stream_f {
  template <class mo_stream, class...>
  using apply = typename meta_stream_update<mo_stream>::type;
};

//=== operation-code bits in the high byte of to::type::opr_code ===
// opr_code defaults to all-ones (0xFF00000000000000): every bit "on" = normal.
// bit=0 triggers the special behaviour.
namespace stream_op_bits {
constexpr std::uint64_t OP_SKIP = 1ULL << 56;      // on: skip element when pred is false
constexpr std::uint64_t OP_OS_CLEAR = 1ULL << 57;  // on: clear ostream to empty when pred is false
constexpr std::uint64_t OP_IS_IDLE = 1ULL << 58;   // on: pop istream but discard when pred is false
constexpr std::uint64_t OP_OS_IDLE = 1ULL << 59;   // on: force-call ostream function even when pred is false
constexpr std::uint64_t OP_IS_SKIP = 1ULL << 60;   // on: skip when istream ret meta_stream_skip_signal
constexpr std::uint64_t OP_TIMER_DEC = 1ULL << 62;
constexpr std::uint64_t OP_BREAK = 1ULL << 63;     // on: break stream when pred is false
constexpr std::uint64_t OP_DEFAULT = OP_SKIP | OP_IS_IDLE | OP_IS_SKIP;  // default: skip + call_is + consume-skip on

// short names
constexpr std::uint64_t opSkip = OP_SKIP;
constexpr std::uint64_t opReset = OP_OS_CLEAR;
constexpr std::uint64_t opCallIs = OP_IS_IDLE;
constexpr std::uint64_t opCallOs = OP_OS_IDLE;
constexpr std::uint64_t opBreak = OP_BREAK;
constexpr std::uint64_t opIsSkip = OP_IS_SKIP;

// --- mask composition used by stream_op ---
// Spec encodes two sets in one uint64:
//   band 56..63 : bits forced ON  (enable)
//   band 48..55 : bits forced OFF (deactivate, mirrored from 56..63)
// enable: force bits on; stays in the 56..63 band.
consteval std::uint64_t enable(std::uint64_t bits) { return bits; }
// deactivate: force bits off; mirrors 56+k -> 48+k (internal encoding).
consteval std::uint64_t deactivate(std::uint64_t bits) { return bits >> 8; }
// compose: keep OP_DEFAULT, force enabled bits on, deactivated bits off.
consteval std::uint64_t compose(std::uint64_t spec) {
  constexpr std::uint64_t OP_BAND = 0xFFULL << 56;
  const std::uint64_t set = spec & OP_BAND;
  const std::uint64_t clear = (spec & (0xFFULL << 48)) << 8;  // mirror back
  return (OP_DEFAULT | set) & ~clear;
}
}  // namespace stream_op_bits


// stream_op<Spec>: base class for user-defined states. Compose Spec with
// stream_op_bits::enable() / deactivate(); unspecified bits keep OP_DEFAULT.
// Spec = 0 (the default) gives the pure OP_DEFAULT behaviour.
template <std::uint64_t Spec = 0>
struct stream_op {
  static constexpr std::uint64_t opr_code = stream_op_bits::compose(Spec);
};
// operator_code<bits...>: those bits are turned OFF (triggered)
template <std::uint64_t... Bits>
struct operator_code {
  static constexpr std::uint64_t value = ~(Bits | ...);
};

// default_t: placeholder object / result for the states members a user states
// class leaves unset (used by states_base below).
struct default_t {};

// states_base: supplies no-op defaults for every member a states class needs
// (type / apply / on_changed / pred), so a user states class only overrides the
// members it actually uses instead of writing all four every time. Combine it
// with stream_op<Spec> through multiple inheritance when opr_code flags are
// also needed:
//   struct my_states : stream_op<opSkip>, states_base { ... };
struct states_base {
  using type = default_t;

  template <class T, class U>
  using apply = default_t;

  template <class T, class U>
  using on_changed = default_t;

  template <class T, class U>
  using pred = std::false_type;
};

// make_base<List, OpCode>: wraps a type_list with a static opr_code
template <class List, std::uint64_t OpCode = stream_op_bits::OP_DEFAULT>
struct make_base {
  using type = List;
  static constexpr std::uint64_t opr_code = OpCode;
};


// meta_states: a states type declares its starting object via ::type and provides
// the member alias templates apply / on_changed / pred. Their mere existence is
// probed by passing them as template-template arguments (no concrete
// instantiation), mirroring meta_function_t, so the second parameter kind is not
// hardcoded to std::integral_constant.
template <class States>
concept meta_states = requires {
  typename States::type;
  typename meta_function_template_container<States::template apply>;
  typename meta_function_template_container<States::template on_changed>;
  typename meta_function_template_container<States::template pred>;
};

// meta_states_with_opcode concept: also has opr_code
template <class States>
concept meta_states_with_opcode = meta_states<States> && requires {
  States::opr_code;
};


// states_wrapper: wraps a user-defined states struct into a meta-function
// that has both apply and on_changed
template <class States>
struct states_wrapper {
  template <class Obj, class From>
  using apply = typename States::template apply<Obj, From>;

  template <class Obj, class From>
  using on_changed = typename States::template on_changed<Obj, From>;
};
// make_states_type: wrap a user-defined states struct into meta_states_object.
// The opcode branch is selected lazily (if constexpr) so a states without an
// opr_code never forces a substitution of States::opr_code.
template <meta_states States>
struct make_states_type {
  static consteval auto get() {
    if constexpr (requires { States::opr_code; }) {
      return std::type_identity<meta_states_object<
          get_type<States>, states_wrapper<States>,
          meta_quote::binary<States::template pred>,
          meta_states_details::meta_states_always_continue, States::opr_code>>{};
    } else {
      return std::type_identity<meta_states_object<
          get_type<States>, states_wrapper<States>,
          meta_quote::binary<States::template pred>,
          meta_states_details::meta_states_always_continue,
          stream_op<>::opr_code>>{};
    }
  }
  using type = typename decltype(get())::type;
};

// convenience alias
template <class T>
using meta_make_states = typename make_states_type<T>::type;
// states-style update: specialises on meta_states_object.
// Reads opr_code from to::type, applies skip/is_idle/break bits.
struct meta_stream_s_f {
 private:
  // Whether ostream To consumes a meta_stream_skip_signal (the opIsSkip bit).
  // Default ON for every ostream; it is OFF only when To explicitly supplies an
  // opr_code with OP_IS_SKIP cleared, meaning it wants to receive skip_signal.
  template <class To>
  static consteval bool consumes_skip_v() {
    if constexpr (requires { typename To::changed_pred; }) {
      // states ostream: opr_code lives in the high bits of flags
      constexpr std::uint64_t code =
          To::flags & ~meta_states_details::FLAGS_LOW_MASK;
      return (code & stream_op_bits::OP_IS_SKIP) != 0;
    } else if constexpr (requires { To::opr_code; }) {
      return (To::opr_code & stream_op_bits::OP_IS_SKIP) != 0;
    } else {
      return true;  // plain ostream with no opr_code: consume by default
    }
  }

  // plain (non-states) ostream update
  template <class To, class From, class = void>
  struct plain_update {
    using type = meta_stream<
        meta_object_invoke<To, From>, meta_invoke<From>>;
  };
  // cache is skip_signal and the ostream consumes it: ostream stays unchanged
  // and the input advances (the skip is consumed so the loop progresses)
  template <class To, class From>
  struct plain_update<To, From, std::enable_if_t<
      std::is_same_v<typename meta_stream<To, From>::cache,
                     protocols::meta_stream_skip_signal> &&
          consumes_skip_v<To>()>> {
    using type = meta_stream<To, meta_invoke<From>>;
  };

  template <class Stream, class = void>
  struct update_impl {
    using type = typename plain_update<typename Stream::to,
                                      typename Stream::from>::type;
  };

  // To is a meta_states_object
  template <class To, class From>
  struct update_impl<meta_stream<To, From>,
                     std::void_t<typename To::changed_pred>> {
   private:
    using cache_t = typename meta_stream<To, From>::cache;
    // opr_code is stored in high 8 bits of flags
    static constexpr std::uint64_t code =
        To::flags & ~meta_states_details::FLAGS_LOW_MASK;

    // atomic op flags read up front
    static constexpr bool op_is_skip_sig =
        (code & stream_op_bits::OP_IS_SKIP);
    static constexpr bool op_is_idle =
        (code & stream_op_bits::OP_IS_IDLE);

    // record one step. Use the same overflow protocol as meta_states_object:
    // next_flags() resets the low 56 bits once the index reaches 56, and the
    // index wraps to 0, so a long stream never forms (1ULL << index) for an
    // index >= 64 (which would be ill-formed).
    template <bool Changed>
    static consteval std::uint64_t recorded_flags() {
      return meta_states_details::next_flags<Changed, To::flags, To::size>();
    }
    template <bool Changed>
    using record = meta_states_object<
        typename To::type, typename To::function,
        typename To::break_condition, typename To::changed_pred,
        recorded_flags<Changed>(),
        (To::size >= 56 ? 0 : To::size + 1)>;

    // choose_from<Advance>: advance the input or keep it
    template <bool Advance, class FromT>
    struct choose_from {
      using type = FromT;
    };
    template <class FromT>
    struct choose_from<true, FromT> {
      using type = meta_invoke<FromT>;
    };

    // choose_ostream<CallOs, Skip>: select ostream type
    template <bool CallOs, bool Skip, class ToT, class FromT,
              class RecordedT, class CacheT>
    struct choose_ostream {
      // normal: ostream receives cache
      using type = meta_object_invoke<ToT, FromT>;
    };
    template <class ToT, class FromT, class RecordedT, class CacheT>
    struct choose_ostream<true, true, ToT, FromT, RecordedT, CacheT> {
      // call_os takes priority
      using type = typename RecordedT::template meta_set<
          meta_invoke<typename ToT::function, typename ToT::type, CacheT>>;
    };
    template <class ToT, class FromT, class RecordedT, class CacheT>
    struct choose_ostream<true, false, ToT, FromT, RecordedT, CacheT> {
      using type = typename RecordedT::template meta_set<
          meta_invoke<typename ToT::function, typename ToT::type, CacheT>>;
    };
    template <class ToT, class FromT, class RecordedT, class CacheT>
    struct choose_ostream<false, true, ToT, FromT, RecordedT, CacheT> {
      // skip: ostream unchanged
      using type = RecordedT;
    };

    // body<Cache, OpIsSkipSig>: dispatch on the cache value
    template <class Cache, bool OpIsSkipSig, class = void>
    struct body;

    // (A) input yielded meta_stream_skip_signal and opIsSkip is on:
    //     skip this step -- ostream unchanged (record no-change), input
    //     advances only if opCallIs is on.
    template <class Dummy>
    struct body<protocols::meta_stream_skip_signal, true, Dummy> {
      using recorded = record<false>;
      using next_from = typename choose_from<op_is_idle, From>::type;
      using type = meta_stream<recorded, next_from>;
    };

    // (B) normal step: run the user pred, then skip/call_os dispatch
    template <class Cache, bool OpIsSkipSig>
    struct body<Cache, OpIsSkipSig, std::enable_if_t<!(
        std::is_same_v<Cache, protocols::meta_stream_skip_signal> &&
        OpIsSkipSig)>> {
      static constexpr bool pred = To::template changed<Cache>::value;
      static constexpr bool skip =
          (code & stream_op_bits::OP_SKIP) && !pred;
      static constexpr bool is_idle =
          (code & stream_op_bits::OP_IS_IDLE) && !pred;
      static constexpr bool call_os =
          (code & stream_op_bits::OP_OS_IDLE) && !pred;

      using recorded = record<pred>;
      static constexpr bool advance_from = pred || is_idle;
      using next_from = typename choose_from<advance_from, From>::type;
      using next_ostream = typename choose_ostream<
          call_os, skip, To, From, recorded, Cache>::type;
      using type = meta_stream<next_ostream, next_from>;
    };

   public:
    using type = typename body<cache_t, op_is_skip_sig>::type;
  };

 public:
  // End guard: an ended stream stays unchanged (idempotent), so callers
  // can apply freely without instantiating the op logic on end_of_list.
  // Non-meta_stream types (no ::end) fall through to update_impl.
  template <class Stream, class = void>
  struct apply_impl {
    using type = typename update_impl<Stream>::type;
  };
  template <class Stream>
  struct apply_impl<Stream, std::enable_if_t<Stream::end>> {
    using type = Stream;
  };

  template <class mo_stream, class...>
  using apply = typename apply_impl<mo_stream>::type;
};

struct meta_always_false_c_o {
  template <class>
  struct apply : std::false_type {};
};

// Condition implementation, dispatched by whether stream has ended
// Uses template specialization instead of lambda if constexpr to ensure
// invalid branches are never substituted/instantiated
template <class Stream, class BF, bool Ended>
struct transfer_condition_impl {
  static constexpr bool value = false;
};

template <class Stream, class BF>
struct transfer_condition_impl<Stream, BF, false> {
 private:
  using to_t = typename Stream::to;
  using cache_t = typename Stream::cache;

  // Check inner MSO break bit
  template <class T, class = void>
  struct inner_break {
    static constexpr bool value = false;
  };

  template <class T>
  struct inner_break<T, std::void_t<typename T::changed_pred>> {
    static constexpr bool pred = T::template changed<cache_t>::value;
    static constexpr std::uint64_t code =
        T::flags & ~meta_states_details::FLAGS_LOW_MASK;
    static constexpr bool value =
        (code & stream_op_bits::OP_BREAK) && !pred;
  };

 public:
  static constexpr bool value =
      !(inner_break<to_t>::value ||
        meta_invoke<BF, Stream>::value);
};

template <class BF>
struct meta_transfer_until_condition {
  template <class this_stream, class...>
  struct apply {
    static constexpr bool stream_ended =
        is_end_stream<typename this_stream::from_t> ||
        std::is_same_v<typename this_stream::cache,
                       literal_types::end_of_list>;
    static constexpr bool value =
        transfer_condition_impl<this_stream, BF, stream_ended>::value;
  };
};

template <class BF>
using meta_transfer_until_condition_o =
    meta_object<void, meta_transfer_until_condition<BF>>;

template <class To, class From, class ChangedPred>
using meta_stream_s_o = meta_states_object<meta_stream<To, From>,
                                          meta_stream_s_f, ChangedPred,
                                          meta_states_details::meta_states_always_continue>;

struct meta_stream_always_continue {
  template <class in_stream_t>
  struct apply {
    static constexpr bool value =
        std::is_same_v<typename in_stream_t::from::ret,
                       literal_types::end_of_list>;
  };
};

// default ChangedPred: mark change only when ostream (to) actually differs
// after update
using default_stream_change_pred = observe_os_change<meta_stream_s_f>;
}  // namespace io_stream_transform_details

template <class T>
concept meta_istream_t =
    io_stream_transform_details::io_stream_traits::meta_istream_t<T>;

template <class T>
concept meta_ostream_t =
    meta_object_t<T>;

using meta_range_continue = io_stream_transform_details::meta_always_false_c_o;

//=== user-selectable states observers (change predicates) ===
// Each observer is a binary meta-function (observe_*_change) bound to the
// stream update function meta_stream_s_f. It decides what counts as a "change"
// at each step, which drives the states flags / last_changed and, through the
// looper's default observer (observe_default), whether the runtime callback
// fires. ostream_observer is the default.
using ostream_observer =
    observe_os_change<io_stream_transform_details::meta_stream_s_f>;
using istream_observer =
    observe_is_change<io_stream_transform_details::meta_stream_s_f>;
using cache_observer =
    observe_cache_change<io_stream_transform_details::meta_stream_s_f>;
using stream_observer =
    observe_whole_change<io_stream_transform_details::meta_stream_s_f>;

template <meta_ostream_t To, meta_istream_t From, class BF, class Observer>
using meta_transfer_until_impl = meta_invoke<meta_looper<
    io_stream_transform_details::meta_transfer_until_condition_o<BF>,
    io_stream_transform_details::meta_stream_s_o<To, From, Observer>,
    meta_empty_o, observe_default>>;

template <meta_ostream_t To, meta_istream_t From,
          class BF = meta_range_continue,
          class Observer = ostream_observer>
using meta_transfer_until =
    meta_transfer_until_impl<To, From, BF, Observer>;

template <meta_ostream_t To, meta_istream_t From,
      class break_f = meta_range_continue>
using transfer_until =
    typename meta_transfer_until<To, From, break_f>::type;

// convert meta_stream into a timed meta_object
template <std::size_t Transfer_Length, meta_ostream_t To, meta_istream_t From,
          class break_f = meta_always_continue>
using meta_stream_t_o =
    meta_timer_object<Transfer_Length,
                      io_stream_transform_details::meta_stream<To, From>,
                      io_stream_transform_details::meta_stream_f, break_f>;

// create a meta_timer_object that transfers all elements from From to To
template <meta_ostream_t To, meta_istream_t From,
          class break_f = meta_always_continue>
using meta_all_transfer_o =
    meta_stream_t_o<exp_size<typename From::type>, To, From, break_f>;

template <meta_ostream_t To, meta_istream_t From,
          class break_f = meta_always_continue>
using meta_all_transfer =
    meta_looper_t<io_stream_transform_details::meta_timer_cond_o,
                  meta_all_transfer_o<To, From, break_f>, meta_empty_o>;

template <std::size_t N, meta_ostream_t To, meta_istream_t From,
          class break_f = meta_always_continue>
using transfer =
    meta_looper_t<io_stream_transform_details::meta_timer_cond_o,
                  meta_stream_t_o<N, To, From, break_f>, meta_empty_o>;

// create a meta_looper
// in meta_stream, Generator is not needed, so we use meta_empty_o here
template <class stream_t>
using transfer_stream =
    meta_looper<io_stream_transform_details::meta_timer_cond_o, stream_t,
                meta_empty_o>;

template <meta_ostream_t To, meta_istream_t From,
          class break_f = meta_always_continue>
using meta_for =
    meta_invoke<transfer_stream<meta_all_transfer_o<To, From, break_f>>>;

template <std::size_t times, meta_ostream_t To, meta_istream_t From,
          class break_f = meta_always_continue>
using meta_while =
    meta_invoke<transfer_stream<meta_stream_t_o<times, To, From, break_f>>>;

// common basic io streams
template <class type_list>
using meta_istream =
    io_stream_transform_details::meta_basic_istream_detail::meta_basic_istream<
        type_list>;

template <class type_list>
using meta_ristream = io_stream_transform_details::meta_reverse_istream_detail::
    meta_reverse_istream<type_list>;

template <class... Tys>
using meta_istream_list = meta_istream<exp_list<Tys...>>;

template <class type_list = exp_list<>>
using meta_ostream =
    io_stream_transform_details::meta_basic_ostream_detail::meta_basic_ostream<
        type_list>;

// operation-code bits for meta_states_object opr_code
namespace stream_op_bits = io_stream_transform_details::stream_op_bits;

// short names for op bits, directly in meta_ios
using io_stream_transform_details::stream_op_bits::OP_SKIP;
using io_stream_transform_details::stream_op_bits::OP_OS_CLEAR;
using io_stream_transform_details::stream_op_bits::OP_IS_IDLE;
using io_stream_transform_details::stream_op_bits::OP_OS_IDLE;
using io_stream_transform_details::stream_op_bits::OP_IS_SKIP;
using io_stream_transform_details::stream_op_bits::OP_TIMER_DEC;
using io_stream_transform_details::stream_op_bits::OP_BREAK;
using io_stream_transform_details::stream_op_bits::OP_DEFAULT;
using io_stream_transform_details::stream_op_bits::opSkip;
using io_stream_transform_details::stream_op_bits::opReset;
using io_stream_transform_details::stream_op_bits::opCallIs;
using io_stream_transform_details::stream_op_bits::opCallOs;
using io_stream_transform_details::stream_op_bits::opBreak;
using io_stream_transform_details::stream_op_bits::opIsSkip;
using io_stream_transform_details::stream_op_bits::enable;
using io_stream_transform_details::stream_op_bits::deactivate;
using io_stream_transform_details::stream_op_bits::compose;

using io_stream_transform_details::make_base;
using io_stream_transform_details::operator_code;
using io_stream_transform_details::stream_op;
using io_stream_transform_details::default_t;
using io_stream_transform_details::states_base;
using io_stream_transform_details::meta_states;
using io_stream_transform_details::meta_states_with_opcode;
using io_stream_transform_details::states_wrapper;
using io_stream_transform_details::make_states_type;
using io_stream_transform_details::meta_make_states;

template <class type_list, class meta_function_type>
using meta_transform_istream =
    io_stream_transform_details::meta_transform_istream_detail::
        meta_basic_transform_istream<type_list, meta_function_type>;

template <class type_list, class meta_function_type>
using meta_transform_ostream =
    io_stream_transform_details::meta_transform_ostream_detail::
        meta_basic_transform_ostream<type_list, meta_function_type>;

template <class type_list, class meta_function_type>
using meta_filter_ostream =
    io_stream_transform_details::meta_filter_ostream_detail::filter_ostream<
        type_list, meta_function_type>;

// A states-based ostream: meta_states_object wrapping a type_list.
// accept_pred == true  -> element is appended (last_changed = true)
// accept_pred == false -> element is rejected (list unchanged, last_changed =
// false)
template <class TL, class accept_pred>
using meta_states_ostream =
    meta_states_object<TL,
                       io_stream_transform_details::meta_states_ostream_detail::
                           states_ostream_f<accept_pred>,
                       accept_pred,
                       meta_states_details::meta_states_always_continue>;

// A states-based iterator ostream: OBJ IS the flags, pred defaults to
// always-changed.
using meta_states_iterator = meta_states_object<
    std::integral_constant<std::uint64_t, 0>,
    io_stream_transform_details::meta_states_iterator_detail::
        iterator_passthrough,
    io_stream_transform_details::meta_states_iterator_detail::always_changed,
    meta_states_details::meta_states_always_continue>;
// generate an index type for each element in the istream, starting from 'start'
// note: this istream never ends
template <std::size_t start>
using meta_index_istream =
    io_stream_transform_details::meta_index_istream_detail::index_istream<
        start>;

// warning: ranged based, a timer based transfer cannot detect length
template <std::size_t Len>
using index_sequence_istream = io_stream_transform_details::
    ranged_based_index_sequence_istream_detail::ranged_index_istream<Len>;

template <std::size_t start, std::size_t count>
using meta_count = typename transfer<count, meta_ostream<exp_list<>>,
                                     meta_index_istream<start>>::to::type;

template <std::size_t start, std::size_t count>
using meta_count_istream = meta_istream<meta_count<start, count>>;

template <class F, typename init = literal_types::no_exist_type>
using meta_transform_iterator = io_stream_transform_details::
    meta_transform_iterator_detail::transform_iterator<F, init>;

using meta_iterator = io_stream_transform_details::
    meta_replace_able_ostream_detail::replace_able_ostream;

// generate an specific type T for each element in the istream
// note: this istream never ends
template <class T>
using meta_repeat_istream =
    io_stream_transform_details::meta_self_repeat_ostream_detail::self_ostream<
        T>;

// if from the istream reads a typelist, it joins the typelist into the typelist
// in ostream
template <class TL>
using meta_jostream =
    io_stream_transform_details::meta_join_ostream_detail::join_ostream<TL>;

// Collect by prepending: final list is in reverse arrival order (suitable for
// reverse destruction). Original order is read via protocols::forward_reverse.
template <class TL = exp_list<>>
using meta_rostream = io_stream_transform_details::
    meta_reverse_ostream_detail::reverse_ostream<TL>;

template <class T1, class T2>
using default_combine = exp_list<T1, T2>;

// in each iteration, F<this_list::at<I>, from_is>
template <class TL, class F = meta_quote::binary<default_combine>>
using meta_forward_ostream = io_stream_transform_details::
    meta_forward_ostream_details::meta_forward_ostream<TL, F>;

// meta character istream, '0' terminated
template <static_str str>
using meta_char_istream = meta_istream<
    typename transfer<exp_size<str_to_list<str>>, meta_ostream<exp_list<>>,
                      meta_istream<str_to_list<str>>>::to::type>;

// transfer protocols
namespace protocols {
// quote meta unary function to handle this_type prefix only
template <class meta_function_type>
using only_this =
    io_stream_transform_details::transfer_protocols::details::this_policy_type<
        meta_function_type>;
// quote meta unary function to handle ret from istream only
template <class meta_function_type>
using only_arg =
    io_stream_transform_details::transfer_protocols::details::arg_policy_type<
        meta_function_type>;
// quote meta binary function to handle to::type from ostream and ret from
// istream
template <class meta_function_type>
using stream_to_unref = io_stream_transform_details::transfer_protocols::
    details::stream_to_policy_type<meta_function_type>;
// quote meta unary function to handle ret from istream only
template <class meta_function_type>
using only_stream_to_unref = io_stream_transform_details::transfer_protocols::
    details::stream_to_this_policy_type<meta_function_type>;
// quote meta unary function to handle ret from istream only
template <class meta_function_type>
using only_stream_from_unref = io_stream_transform_details::transfer_protocols::
    details::stream_from_only_policy_type<meta_function_type>;
// quote meta binary function to handle cache from istream and ret from istream
template <class meta_function_type>
using stream_cache_unref = io_stream_transform_details::transfer_protocols::
    details::stream_cache_policy_type<meta_function_type>;
// quote meta unary function to handle cache from istream only
template <class meta_function_type>
using only_stream_cache_unref =
    io_stream_transform_details::transfer_protocols::details::
        stream_cache_this_policy_type<meta_function_type>;

template <class meta_stream_type>
using stream_to_t = typename meta_stream_type::to::type;

template <class meta_stream_type>
using stream_from_t = typename meta_stream_type::from::type;

template <class meta_stream_type>
using stream_cache_t = typename meta_stream_type::cache;

template <class type_list>
using forward_last = exp_select<max_index<type_list>, type_list>;

// Extract the first element of a produced type list
template <class type_list>
using forward_front = exp_select<0, type_list>;

// stream_no_unref: marker + identity protocol.
// Its presence tells the driver NOT to auto-prepend stream_to_t, with the same
// precedence as stream_to_t / stream_from_t / stream_cache_t. Use it when a
// later protocol consumes the WHOLE meta_stream directly. As a fold step it is
// identity -- it passes the meta_stream through untouched.
template <class Stream>
using stream_no_unref = Stream;

struct meta_stream_skip_signal {};

template <class T>
constexpr bool is_skip_signal = std::is_same_v<T, meta_stream_skip_signal>;

// wait_for_end: optional gate for a reducing/collecting stage.
// While input still has elements, yields meta_stream_skip_signal so the
// driving loop advances the node without observing it; once the input is
// exhausted, passes the meta_stream through so its final to::type is read.
// This is NOT a default -- a node waits only if it uses this protocol.
// Exhaustion test for an input state, compatible with both:
//  - meta_stream-like states (has_end_state): use T::end
//  - exp_list-based states: exhausted when exp_size == 0
template <class T, class = void>
struct exhausted_impl {
  static constexpr bool value = (exp_size<T> == 0);
};
template <class T>
struct exhausted_impl<T, std::enable_if_t<
      io_stream_transform_details::has_end_state<T>>> {
  static constexpr bool value = T::end;
};
template <class T>
constexpr bool exhausted_v = exhausted_impl<T>::value;

// wait_for_end: optional gate for a reducing/collecting stage.
// While the input state is not exhausted, yields meta_stream_skip_signal so the
// driving loop advances the node without observing it; once the input is
// exhausted, passes the meta_stream through so its final to::type is read.
// This is NOT a default -- a node waits only if it uses this protocol.
template <class M, class = void>
struct wait_for_end_impl {
  using type = meta_stream_skip_signal;
};
template <class M>
struct wait_for_end_impl<
    M, std::enable_if_t<exhausted_v<typename M::from::type>>> {
  using type = M;
};
template <class M>
using wait_for_end = typename wait_for_end_impl<M>::type;

// wait_for_end_to_t: while the input is not exhausted -> meta_stream_skip_signal;
// once exhausted -> the stream's to_t (to::type). This fuses wait_for_end with
// stream_to_t. No from/cache variants are provided: after exhaustion both are
// meaningless.
template <class M, class = void>
struct wait_for_end_to_t_impl {
  using type = meta_stream_skip_signal;
};
template <class M>
struct wait_for_end_to_t_impl<
    M, std::enable_if_t<exhausted_v<typename M::from::type>>> {
  using type = typename M::to::type;
};
template <class M>
using wait_for_end_to_t = typename wait_for_end_to_t_impl<M>::type;
}  // namespace protocols

// helper to call a function with a protocol folded stream
namespace protocol_auto_unref_details {
// if has no protocol stream_to_t/stream_from_t/stream_cache_t, default adding
// protocol stream_to_t. detect protocols
template <template <class> class P>
struct protocol_container {
  template <class T>
  using transform = P<T>;
};

template <template <class> class P1, template <class> class P2>
struct template_equal {
  static constexpr bool value =
      std::is_same_v<protocol_container<P1>, protocol_container<P2>>;
};

template <template <class> class P>
struct template_equal_with {
  template <class T>
  struct apply_impl : std::false_type {};
  template <template <class> class anotherP>
  struct apply_impl<protocol_container<anotherP>>
      : template_equal<P, anotherP> {};

  template <class T>
  using apply = apply_impl<T>;
};

template <template <class> class P, template <class> class... PS>
using has_protocol = meta_all_transfer<
    meta_filter_ostream<exp_list<>,
                        protocols::only_arg<template_equal_with<P>>>,
    meta_istream_list<protocol_container<PS>...>,
    // Early exit: as soon as one matching protocol is collected (result list
    // length 1), stop -- no need to walk the rest of the protocol pack.
    protocols::only_stream_to_unref<length_is<1>>>::to_t;

template <template <class> class P, template <class> class... PS>
constexpr bool has_no_protocol_v = length_equal<has_protocol<P, PS...>, 0>;

using protocols::meta_stream_skip_signal;

struct fold_transition {
  template <class this_obj, class from_ins>
  struct impl {
    using type = typename from_ins::template transform<this_obj>;
  };
  template <class this_obj, class from_ins>
  using apply = typename impl<this_obj, from_ins>::type;
};

using break_if_result_is_skip_signal = protocols::only_stream_to_unref<
    meta_quote::bind_binary<std::is_same, meta_stream_skip_signal>>;

template <class in_stream_t, template <class> class... PS>
using fold_result =
    meta_all_transfer<meta_transform_iterator<fold_transition, in_stream_t>,
                      meta_istream_list<protocol_container<PS>...>,
                      break_if_result_is_skip_signal>::to_t;

}  // namespace protocol_auto_unref_details
template <template <class> class... PS>
constexpr auto protocol_call(auto&& f) {
  using protocol_auto_unref_details::fold_result;
  using protocol_auto_unref_details::has_no_protocol_v;
  if constexpr (has_no_protocol_v<protocols::stream_to_t, PS...> &&
                has_no_protocol_v<protocols::stream_from_t, PS...> &&
                has_no_protocol_v<protocols::stream_cache_t, PS...> &&
                has_no_protocol_v<protocols::stream_no_unref, PS...> &&
                has_no_protocol_v<protocols::wait_for_end, PS...> &&
                has_no_protocol_v<protocols::wait_for_end_to_t, PS...>) {
    return [&f]<class in_stream_t, class... Args>(in_stream_t, Args&&... args) {
      using fold_result_t =
          fold_result<in_stream_t, protocols::stream_to_t, PS...>;
      if constexpr (!std::is_same_v<fold_result_t,
                                    protocols::meta_stream_skip_signal>) {
        std::invoke(f, fold_result_t{}, std::forward<Args>(args)...);
      }
    };
  } else {
    return [&f]<class in_stream_t, class... Args>(in_stream_t, Args&&... args) {
      using fold_result_t = fold_result<in_stream_t, PS...>;
      if constexpr (!std::is_same_v<fold_result_t,
                                    protocols::meta_stream_skip_signal>) {
        std::invoke(f, fold_result_t{}, std::forward<Args>(args)...);
      }
    };
  }
}

template <class R, template <class> class... PS>
constexpr auto protocol_call(auto&& f) {
  using protocol_auto_unref_details::fold_result;
  using protocol_auto_unref_details::has_no_protocol_v;
  if constexpr (has_no_protocol_v<protocols::stream_to_t, PS...> &&
                has_no_protocol_v<protocols::stream_from_t, PS...> &&
                has_no_protocol_v<protocols::stream_cache_t, PS...> &&
                has_no_protocol_v<protocols::stream_no_unref, PS...> &&
                has_no_protocol_v<protocols::wait_for_end, PS...> &&
                has_no_protocol_v<protocols::wait_for_end_to_t, PS...>) {
    return [&f]<class in_stream_t, class... Args>(
               in_stream_t, Args&&... args) -> any_caster<R> {
      using fold_result_t =
          fold_result<in_stream_t, protocols::stream_to_t, PS...>;
      if constexpr (!std::is_same_v<fold_result_t,
                                    protocols::meta_stream_skip_signal>) {
        return {std::invoke(f, fold_result_t{}, std::forward<Args>(args)...)};
      } else {
        return any_caster<R>{};
      }
    };
  } else {
    return [&f]<class in_stream_t, class... Args>(
               in_stream_t, Args&&... args) -> any_caster<R> {
      using fold_result_t = fold_result<in_stream_t, PS...>;
      if constexpr (!std::is_same_v<fold_result_t,
                                    protocols::meta_stream_skip_signal>) {
        return {std::invoke(f, fold_result_t{}, std::forward<Args>(args)...)};
      } else {
        return any_caster<R>{};
      }
    };
  }
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
/// Reflection adapter: reflect a struct's non-static data members into an
/// exp_list of their types, ready to feed meta_istream.
/// Requires C++26 static reflection (GCC 16+, -std=c++26 -freflection).
#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
// GCC 16.2 workaround: <meta> must be included (by the user, or here) after
// basic headers.
#include <meta>
namespace reflection_detail {
template <::std::meta::info M>
using member_type = [: ::std::meta::type_of(M):];
}
template <class T, auto Ctx = ::std::meta::access_context::unprivileged()>
struct reflected_member_types {
  static constexpr auto members = ::std::define_static_array(
      ::std::meta::nonstatic_data_members_of(^^T, Ctx));
  template <std::size_t... Is>
  static constexpr auto make(std::index_sequence<Is...>) {
    return exp_list<reflection_detail::member_type<members[Is]>...>{};
  }
  using type = decltype(make(std::make_index_sequence<members.size()>{}));
};
#endif

namespace meta_pipe_node_details {
// ret_from_node: extracts the value handed to the next stage.
// The node state holds an UNPROCESSED input, so extraction first advances
// a copy (without mutating the stored state), then reads the resulting to.
//
// Two modes:
//  - value protocols (default): advance a copy, stream_to_t, then ps
//  - gate protocol wait_for_end: advance a copy, wait_for_end decides whether
//    the node is still waiting (skip_signal) or input is exhausted (read final
//    to::type). This keeps collection lazy -- nothing runs until extraction.
template <template <class> class... ps>
struct ret_from_node {
 private:
  // Reuse the protocol_call rule: auto-prepend stream_to_t only when the
  // protocol pack already contains none of stream_to_t / stream_from_t /
  // stream_cache_t. This makes stream_istream fold exactly like protocol_call.
  static constexpr bool auto_stream_to =
      protocol_auto_unref_details::has_no_protocol_v<protocols::stream_to_t,
                                                     ps...> &&
      protocol_auto_unref_details::has_no_protocol_v<protocols::stream_from_t,
                                                     ps...> &&
      protocol_auto_unref_details::has_no_protocol_v<protocols::stream_cache_t,
                                                     ps...> &&
      protocol_auto_unref_details::has_no_protocol_v<protocols::stream_no_unref,
                                                     ps...> &&
      // wait_for_end consumes the WHOLE meta_stream (it reads ::from to detect
      // exhaustion) and chains its own extraction, so it suppresses the auto
      // stream_to_t just like stream_no_unref; the user orders any following
      // stream_to_t explicitly after it.
      protocol_auto_unref_details::has_no_protocol_v<protocols::wait_for_end,
                                                     ps...> &&
      // wait_for_end_to_t likewise consumes the whole meta_stream and itself
      // yields to_t after exhaustion.
      protocol_auto_unref_details::has_no_protocol_v<
          protocols::wait_for_end_to_t, ps...>;

  // Fold the stepped copy with fold_result, which stops (and stays) on
  // meta_stream_skip_signal, so the signal propagates out untouched.
  // Dispatched by template specialization (no std::conditional_t).
  template <bool AutoTo, class Stepped>
  struct do_fold;
  template <class Stepped>
  struct do_fold<true, Stepped> {
    using type = protocol_auto_unref_details::fold_result<
        Stepped, protocols::stream_to_t, ps...>;
  };
  template <class Stepped>
  struct do_fold<false, Stepped> {
    using type = protocol_auto_unref_details::fold_result<Stepped, ps...>;
  };

  // state not ended: advance a copy (stepped), then fold protocols
  template <class Pipe, class = void>
  struct impl {
    using stepped = meta_invoke<
        io_stream_transform_details::meta_stream_s_f, Pipe>;
    using type = typename do_fold<auto_stream_to, stepped>::type;
  };

  // state ended: emit end_of_list directly, never through the protocols --
  // the fold stops right here, like fold_result stops on a skip_signal.
  template <class Pipe>
  struct impl<Pipe, std::enable_if_t<Pipe::end>> {
    using type = literal_types::end_of_list;
  };

 public:
  template <class this_pipe>
  using apply = typename impl<this_pipe>::type;
};

// stream_istream: wraps (is, os) into a self-terminating istream.
//  - state   = meta_stream<os, is> before the first step (unprocessed)
//  - advance = meta_stream_s_f (plain/states ops, idempotent on end)
//  - extract = ret_from_node (advances a copy then reads to; end -> end_of_list)
template <meta_istream_t is, meta_ostream_t os, template <class> class... ps>
using stream_istream = meta_ret_object<
    io_stream_transform_details::meta_stream<os, is>,
    io_stream_transform_details::meta_stream_s_f,
    ret_from_node<ps...>>;

// meta_pipe_builder: holds the node istream produced so far.
// Construction is fully lazy -- *_to only composes types, nothing runs.
// Termination needs no count: it propagates via the end_of_stream protocol,
// so filters that change element counts work as well.
template <meta_istream_t CurNode>
struct meta_pipe_builder {
  // Append a stage: next_os processes the current node stream; Ps are output
  // protocols applied when extracting that stage value.
  template <meta_ostream_t next_os, template <class> class... Ps>
  using all_to = meta_pipe_builder<stream_istream<CurNode, next_os, Ps...>>;

  // Append a stage forwarding only the last element of a produced list
  // (output protocol = forward_last)
  template <meta_ostream_t next_os, template <class> class... Ps>
  using each_to = meta_pipe_builder<
      stream_istream<CurNode, next_os, Ps..., protocols::forward_last>>;

  // The node istream produced so far -- usable directly by any driver.
  // It stays UNFLOWED: composing all_to/each_to only builds types.
  using from = CurNode;

  // run_with: attach FinalOs as a stage and flow. The protocols Ps are folded on
  // the IS-side node (stream_istream), never on the output: wait_for_end makes
  // every intermediate collection emit skip_signal, so it is neither observed
  // nor forwarded and FinalOs is not asked to process a partial result; once the
  // container is full (input exhausted), stream_to_t reads it and the remaining
  // Ps transform it (e.g. to_meta_array_t). A forwarding meta_iterator drives
  // that node with the built-in for_each, which then fires only for the final
  // value:
  //   ...::run_with<meta_rostream<>, wait_for_end, stream_to_t,
  //                to_meta_array_t>::for_each(f);
  // run: flow the composed pipe -- Os accepts CurNode. It is exactly
  // meta_transfer_until and keeps its built-in for_each. Pass unary
  // metafunctions to for_each to fold the stage before the callable:
  //   ...::all_to<...>::run<final_os>::for_each<P...>(f);
  template <meta_ostream_t Os>
  using run = meta_transfer_until<Os, CurNode>;

  // Kept for the transfer::from spelling
  struct transfer {
    using from = CurNode;
  };
};

}  // namespace meta_pipe_node_details

// Public entry point (in meta_ios):
//   meta_pipe<is>::all_to<os, Ps...>...::from
template <meta_istream_t Is>
struct meta_pipe {
  template <meta_ostream_t os, template <class> class... Ps>
  using all_to = meta_pipe_node_details::meta_pipe_builder<
      meta_pipe_node_details::stream_istream<Is, os, Ps...>>;

  // Flow: Os accepts the composed pipe (built-in for_each).
  template <meta_ostream_t Os>
  using run = meta_pipe_node_details::meta_pipe_builder<Is>
      ::template run<Os>;
};

}  // namespace meta_ios

namespace exp_utilities {
namespace args_at_details {
using meta_ios::opCallIs;
using meta_ios::opSkip;
using meta_ios::states_base;
using meta_ios::stream_op;

// States that change only when the incoming index equals I. Non-matching
// indices are skipped (opSkip), and the matching step advances (opCallIs).
template <std::size_t I>
struct select_states : stream_op<opSkip | opCallIs>, states_base {
  template <class this_, class from_is>
  using pred = std::bool_constant<(I == from_is::value)>;
};
}  // namespace args_at_details

// args_at<I>(args...): select the I-th argument. A compile-time index stream
// drives the selection; the flow stops as soon as the stream cache reaches the
// slot following I, so the callable is instantiated/invoked only for the one
// argument at position I (and, unlike a change-history test, this works for an
// arbitrary pack size). The callable forwards with decltype(auto), preserving
// the value category: an lvalue yields a writable reference, an rvalue yields
// an rvalue reference.
template <std::size_t I, class... Args>
  requires(I < sizeof...(Args))
decltype(auto) args_at(Args&&... args) {
  return meta_ios::meta_transfer_until<
      meta_ios::meta_make_states<args_at_details::select_states<I>>,
      meta_ios::index_sequence_istream<sizeof...(Args)>,
      meta_ios::protocols::only_stream_cache_unref<
          meta_quote::bind_binary<
              std::is_same, std::integral_constant<std::size_t, I + 1>>>>::
      for_each_forward(
          [](auto /*stream*/, auto&& val) -> decltype(auto) {
            return std::forward<decltype(val)>(val);
          },
          std::forward<Args>(args)...);
}
}  // namespace exp_utilities
