#include <algorithm>
#include <array>
#include <concepts>
#include <cstdint>
#include <functional>
#include <limits>
#include <type_traits>
#include <utility>
namespace exp_utilities {
namespace literal_types {
struct no_exist_type : std::false_type {};

struct end_of_list {
  using front = no_exist_type;
  using back = no_exist_type;
};

template <class T>
struct error {
  template <auto str>
  struct message {
    static constexpr bool value = false;
  };
};
}  // namespace literal_types
namespace exp_select_detail {
// Helper to find the largest power of 2 <= N (N > 0)
template <int N>
struct largest_pow2_le {
  static constexpr int value = N >= 32   ? 32
                               : N >= 16 ? 16
                               : N >= 8  ? 8
                               : N >= 4  ? 4
                               : N >= 2  ? 2
                                         : 1;
};

// Safely drop a power-of-2 number of elements from a type list.
// Returns literal_types::no_exist_type if the list is too short.
template <int N, class T>
struct safe_drop_pow2 {
  using type = literal_types::no_exist_type;
};

// N=0: drop nothing
template <class T>
struct safe_drop_pow2<0, T> {
  using type = T;
};

// Empty list with N>0: failure
template <int N, template <typename...> typename TL>
  requires(N > 0)
struct safe_drop_pow2<N, TL<>> {
  using type = literal_types::no_exist_type;
};

// N=1: drop exactly one element
template <template <typename...> typename TL, class T0, class... Ts>
struct safe_drop_pow2<1, TL<T0, Ts...>> {
  using type = TL<Ts...>;
};

// N>1 with at least two elements: recurse by halving
template <int N, template <typename...> typename TL, class T0, class T1,
          class... Ts>
  requires(N > 1)
struct safe_drop_pow2<N, TL<T0, T1, Ts...>> {
  using half = typename safe_drop_pow2<N / 2, TL<T0, T1, Ts...>>::type;
  using type = typename safe_drop_pow2<N / 2, half>::type;
};

// Forward declaration
template <int N, class T>
struct drop_impl;

// Helper to avoid instantiating drop_impl when the previous drop failed
template <bool Failed, int N, class T>
struct drop_impl_helper;

template <int N, class T>
struct drop_impl_helper<true, N, T> {
  using type = literal_types::no_exist_type;
};

template <int N, class T>
struct drop_impl_helper<false, N, T> {
  using type = typename drop_impl<N, T>::type;
};

// Drop N elements using binary decomposition
template <int N, class T>
struct drop_impl {
  static constexpr int P = largest_pow2_le<N>::value;
  using intermediate = typename safe_drop_pow2<P, T>::type;
  using type = typename drop_impl_helper<
      std::is_same<intermediate, literal_types::no_exist_type>::value, N - P,
      intermediate>::type;
};

template <class T>
struct drop_impl<0, T> {
  using type = T;
};

// Extract the first element of a type list, or no_exist_type if empty
template <class T>
struct first_type {
  using type = literal_types::no_exist_type;
};

template <template <typename...> typename TL, class First, class... Rest>
struct first_type<TL<First, Rest...>> {
  using type = First;
};

// The optimized select_type
template <int I, class T>
struct select_type {
  using dropped = typename drop_impl<I, T>::type;
  using type = typename first_type<dropped>::type;
};
}  // namespace exp_select_detail

template <std::size_t I, class TL>
using exp_select = typename exp_select_detail::select_type<I, TL>::type;

namespace exp_list_select_detail {
template <std::size_t... I>
struct list_select_impl {
  template <template <typename...> typename TL, class... Tys>
  static constexpr auto apply_impl(TL<Tys...>) {
    return TL<exp_select<I, TL<Tys...>>...>{};
  }

  template <typename TL>
  using apply = decltype(apply_impl(std::declval<TL>()));
};
}  // namespace exp_list_select_detail

template <std::size_t... I>
struct exp_list_select {
  template <typename TL>
  using apply = typename exp_list_select_detail::list_select_impl<
      I...>::template apply<TL>;
};

template <class index_sequence_t>
struct to_exp_list_select_t_impl;

template <std::size_t... I>
struct to_exp_list_select_t_impl<std::index_sequence<I...>> {
  using type = exp_list_select<I...>;
};

template <class index_sequence_t>
using to_exp_list_select_t =
    typename to_exp_list_select_t_impl<index_sequence_t>::type;

template <class... Tys>
struct exp_list;

namespace exp_list_details {
template <class TL>
struct to_exp_list {};
template <template <class...> class TL, class... Typs>
struct to_exp_list<TL<Typs...>> {
  using type = exp_list<Typs...>;
};

template <typename... Tys>
struct my_list {
  static constexpr std::size_t length = sizeof...(Tys);
};

template <class TL>
struct is_exp_list_based : std::false_type {};
template <template <class...> class TL, class... TS>
struct is_exp_list_based<TL<TS...>> {
  static constexpr bool value = std::is_base_of_v<exp_list<TS...>, TL<TS...>>;
};

template <class List, class Acc = exp_list<>>
struct pop_back_accumulate;

template <class T, class... AccTys>
struct pop_back_accumulate<exp_list<T>, exp_list<AccTys...>> {
  using type = exp_list<AccTys...>;
};

template <class First, class Second, class... Rest, class... AccTys>
struct pop_back_accumulate<exp_list<First, Second, Rest...>,
                           exp_list<AccTys...>> {
  using type = typename pop_back_accumulate<exp_list<Second, Rest...>,
                                            exp_list<AccTys..., First>>::type;
};

template <class List>
struct pop_back_impl;

template <class First, class... Tys>
struct pop_back_impl<exp_list<First, Tys...>> {
  using type = typename pop_back_accumulate<exp_list<First, Tys...>>::type;
};

}  // namespace exp_list_details

template <class First, class... Tys>
struct exp_list<First, Tys...> {
  static constexpr std::size_t length = sizeof...(Tys) + 1;
  template <template <class...> class TL>
  using to = TL<First, Tys...>;
  template <template <class> class F>
  using for_each = exp_list<F<First>, F<Tys>...>;
  template <class T>
  using push_back = exp_list<First, Tys..., T>;
  template <class T>
  using push_front = exp_list<T, First, Tys...>;
  using pop_front = exp_list<Tys...>;
  using pop_back = typename exp_list_details::pop_back_impl<exp_list>::type;
  using front = First;
  using back =
      exp_select<sizeof...(Tys), exp_list_details::my_list<First, Tys...>>;
  template <std::size_t I>
    requires(I < length)
  using at = exp_select<I, exp_list<First, Tys...>>;
};

template <>
struct exp_list<> {
  static constexpr std::size_t length = 0;
  template <template <class...> class TL>
  using to = TL<>;
  template <template <class> class F>
  using for_each = exp_list<>;
  template <class T>
  using push_back = exp_list<T>;
  template <class T>
  using push_front = exp_list<T>;
  using pop_front = literal_types::end_of_list;
  using pop_back = literal_types::end_of_list;
  using front = literal_types::end_of_list;
  using back = literal_types::end_of_list;
};

template <class TL, class = void>
struct pop_last_impl {
  using type = TL;
};

template <class TL>
struct pop_last_impl<TL, std::void_t<decltype(TL::length)>> {
  using type =
      std::conditional_t<(TL::length >= 1),
                         typename to_exp_list_select_t<std::make_index_sequence<
                             TL::length - 1>>::template apply<TL>,
                         literal_types::end_of_list>;
};

template <class TL>
using pop_last = typename pop_last_impl<TL>::type;

template <class TL>
using to_exp_list_t = typename exp_list_details::to_exp_list<TL>::type;
namespace exp_function_info_details {
template <class R, class... Args>
using function_t = R(Args...);
template <class R, class C, class... Args>
using member_function_t = R (C::*)(Args...);

template <class F>
concept Functor = requires(F f) { &F::operator(); };

template <class F>
struct function_info;

template <class R, class... Args>
struct function_info<R (*)(Args...)> {
  using return_type = R;
  using argument_types = exp_list<Args...>;
};
template <class R, class C, class... Args>
struct function_info<R (C::*)(Args...)> {
  using return_type = R;
  using argument_types = exp_list<Args...>;
};

template <class R, class C, class... Args>
struct function_info<R (C::*)(Args...) const> {
  using return_type = R;
  using argument_types = exp_list<Args...>;
};

template <Functor F>
struct function_info<F> : function_info<decltype(&F::operator())> {};
}  // namespace exp_function_info_details

template <class F>
using exp_function_info = exp_function_info_details::function_info<F>;
template <class T>
struct any_caster {
  constexpr any_caster() noexcept : _dummy(), _has_value(false) {}
  constexpr any_caster(const T& val) : _has_value(false) {
    std::construct_at(&_value, val);
    _has_value = true;
  }
  constexpr any_caster(T&& val) : _has_value(false) {
    std::construct_at(&_value, std::move(val));
    _has_value = true;
  }
  constexpr any_caster(const any_caster& another) : _has_value(false) {
    if (another._has_value) {
      std::construct_at(&_value, another._value);
      _has_value = true;
    }
  }
  constexpr any_caster(any_caster&& another) noexcept : _has_value(false) {
    if (another._has_value) {
      std::construct_at(&_value, std::move(another._value));
      _has_value = true;
    }
  }
  // if T is constexpr - able, then this function will return a constexpr value
  // of T
  constexpr T constexpr_value() { return T{}; }

  // if T is constexpr - able, then this function will return a constexpr value
  // of T constructed with val
  template <auto val>
  constexpr T constexpr_value() {
    return T{val};
  }
  constexpr T value() { return _value; }
  constexpr any_caster& operator=(const any_caster& another) {
    if (this == &another) return *this;
    if (_has_value && another._has_value) {
      _value = another._value;
    } else if (_has_value && !another._has_value) {
      std::destroy_at(&_value);
      _has_value = false;
    } else if (!_has_value && another._has_value) {
      std::construct_at(&_value, another._value);
      _has_value = true;
    }
    return *this;
  }
  constexpr any_caster& operator=(any_caster&& another) {
    if (this == &another) return *this;
    if (_has_value && another._has_value) {
      _value = std::move(another._value);
    } else if (_has_value && !another._has_value) {
      std::destroy_at(&_value);
      _has_value = false;
    } else if (!_has_value && another._has_value) {
      std::construct_at(&_value, std::move(another._value));
      _has_value = true;
    }
    return *this;
  }
  union {
    char _dummy;
    T _value;
  };
  bool _has_value;
  constexpr ~any_caster() {
    if (_has_value) {
      std::destroy_at(&_value);
    }
  }
  operator T() { return _value; }
  constexpr explicit operator bool() const noexcept { return _has_value; }
  constexpr bool has_value() { return _has_value; }
  constexpr T& operator*() { return _value; }
  constexpr const T& operator*() const { return _value; }
};

template <class TL>
concept exp_list_based = exp_list_details::is_exp_list_based<TL>::value;
namespace exp_size_details {
template <class TL>
concept has_length = requires { TL::length; };
template <class TL>
struct type_list_size {
  static constexpr std::size_t value = 0;
};

template <class TL>
  requires has_length<TL>
struct type_list_size<TL> {
  static constexpr std::size_t value = TL::length;
};

template <template <class...> class TL, class... Tys>
  requires(!has_length<TL<Tys...>>)
struct type_list_size<TL<Tys...>> {
  static constexpr std::size_t value = sizeof...(Tys);
};
}  // namespace exp_size_details
template <class L>
constexpr std::size_t exp_size = exp_size_details::type_list_size<L>::value;

namespace max_index_details {
template <class TL>
struct max_index_type {
  static_assert((exp_size<TL> > 0), "Error: empty list");
  static constexpr std::size_t value = exp_size<TL> - 1;
};
}  // namespace max_index_details

template <class L>
constexpr std::size_t max_index = max_index_details::max_index_type<L>::value;

template <class L, std::size_t N>
constexpr bool length_equal = (exp_size<L> == N);

template <std::size_t N>
struct length_is {
  template <class L>
  struct apply : std::bool_constant<length_equal<L, N>> {};
};

template <std::size_t N>
struct length_is_not {
  template <class L>
  struct apply : std::bool_constant<!length_equal<L, N>> {};
};

template <std::size_t N>
struct length_less {
  template <class L>
      struct apply : std::bool_constant < exp_size<L><N> {};
};

template <std::size_t N>
struct length_greater {
  template <class L>
  struct apply : std::bool_constant<(exp_size<L> > N)> {};
};
namespace exp_find_detail {
template <class IDX, class T, class TL>
struct find_impl;

template <class IDX, class T, template <class...> class TL, class First,
          class... Ts>
struct find_impl<IDX, T, TL<First, Ts...>> {
  using type = typename std::conditional_t<
      std::is_same<T, First>::value,
      std::integral_constant<std::size_t, IDX::value>,
      typename find_impl<std::integral_constant<std::size_t, IDX::value + 1>, T,
                         TL<Ts...>>::type>;
};

template <class IDX, class T, template <class...> class TL>
struct find_impl<IDX, T, TL<>> {
  using type = IDX;  // idx overflow
};
}  // namespace exp_find_detail

template <class T, class TL>
using exp_find =
    typename exp_find_detail::find_impl<std::integral_constant<std::size_t, 0>,
                                        T, TL>::type;

template <class T, class TL>
using exp_try_find =
    std::integral_constant<bool, (max_index<TL> >= exp_find<T, TL>::value)>;

namespace get_type_detail {
template <class T, class U = std::void_t<>>
struct get_type_impl : std::false_type {
  using type = literal_types::no_exist_type;
};

template <class T>
struct get_type_impl<T, std::void_t<typename T::type>> : std::true_type {
  using type = typename T::type;
};
}  // namespace get_type_detail
template <class T>
using get_type = typename get_type_detail::get_type_impl<T>::type;

template <class T>
constexpr bool has_type = get_type_detail::get_type_impl<T>::value;

template <class T>
concept has_value = requires { T::value; };
template <class T, auto cmp>
constexpr bool value_equal = false;

template <class T, auto cmp>
  requires has_value<T>
constexpr bool value_equal<T, cmp> = T::value == cmp;

template <auto val>
struct value_is {
  template <class T>
  struct apply : std::bool_constant<value_equal<T, val>> {};
  template <class F2>
  struct OR {
    template <class T>
    struct apply : std::bool_constant<value_equal<T, val> ||
                                      F2::template apply<T>::value> {};
  };
  template <class F2>
  struct AND {
    template <class T>
    struct apply : std::bool_constant<value_equal<T, val> &&
                                      F2::template apply<T>::value> {};
  };
};

template <auto val>
struct value_is_not {
  template <class T>
  struct apply : std::bool_constant<!value_equal<T, val>> {};
  template <class F2>
  struct OR {
    template <class T>
    struct apply : std::bool_constant<!(value_equal<T, val> ||
                                        F2::template apply<T>::value)> {};
  };
  template <class F2>
  struct AND {
    template <class T>
    struct apply : std::bool_constant<!value_equal<T, val> &&
                                      F2::template apply<T>::value> {};
  };
};

template <class T>
struct type_is {
  template <class U>
  struct apply : std::bool_constant<std::is_same_v<get_type<U>, T>> {};
  template <class F2>
  struct OR {
    template <class U>
    struct apply : std::bool_constant<std::is_same_v<get_type<U>, T> ||
                                      F2::template apply<U>::value> {};
  };
  template <class F2>
  struct AND {
    template <class U>
    struct apply : std::bool_constant<std::is_same_v<get_type<U>, T> &&
                                      F2::template apply<U>::value> {};
  };
};

template <class T>
struct type_is_not {
  template <class U>
  struct apply : std::bool_constant<!std::is_same_v<get_type<U>, T>> {};
  template <class F2>
  struct OR {
    template <class U>
    struct apply : std::bool_constant<!(std::is_same_v<get_type<U>, T> ||
                                        F2::template apply<U>::value)> {};
  };
  template <class F2>
  struct AND {
    template <class U>
    struct apply : std::bool_constant<!std::is_same_v<get_type<U>, T> &&
                                      F2::template apply<U>::value> {};
  };
};

struct below_zero {
  static constexpr size_t value = 0;
};

template <std::size_t I>
using Idx = std::integral_constant<std::size_t, I>;

namespace exp_indices_details {
template <class _Idx, size_t _I>
struct Add_Idx {};

template <std::size_t I>
struct Add_Idx<below_zero, I> {
  using type = Idx<0>;
};
template <template <size_t> class idx, std::size_t I_in_Idx, std::size_t I>
struct Add_Idx<idx<I_in_Idx>, I> {
  using type = idx<I_in_Idx + I>;
};
template <std::size_t I, size_t ADD>
struct Add_Idx<Idx<I>, ADD> {
  using type = Idx<I + ADD>;
};
}  // namespace exp_indices_details

template <class idx, size_t I>
using add_idx_t = typename exp_indices_details::Add_Idx<idx, I>::type;

template <class _Idx>
using inc_idx_t = add_idx_t<_Idx, 1>;

template <size_t... _elements>
struct meta_array : exp_list<Idx<_elements>...> {
  using cv_typelist = exp_list<Idx<_elements>...>;
  template <template <size_t... I> class integer_array_type>
  using to = integer_array_type<_elements...>;
  template <size_t I>
  using at = exp_select<I, cv_typelist>;
  template <size_t I>
  static constexpr size_t get() {
    return at<I>::value;
  }

  static constexpr size_t length = cv_typelist::length;
  static constexpr size_t sum = (0 + ... + _elements);
  static consteval std::array<std::size_t, length> array() {
    return {_elements...};
  }
};

template <class TL>
struct to_meta_array {
  static_assert(sizeof(TL) == 0, "not all elements has value");
};

template <template <class...> class integer_wrapper, std::size_t... elements>
struct to_meta_array<integer_wrapper<Idx<elements>...>> {
  using type = meta_array<elements...>;
};

template <class TL>
using to_meta_array_t = get_type<to_meta_array<TL>>;

template <char c>
struct exp_char {
  static constexpr char value = c;
  constexpr operator char() const { return value; }
};

namespace exp_str_detail {
template <char... str>
struct exp_str_impl : exp_list<exp_char<str>...> {
  constexpr exp_str_impl() {}
  const char m_str[sizeof...(str)]{str...};
  operator const char*() { return m_str; }
  static constexpr std::size_t length = sizeof...(str);
};
template <class TL>
struct to_exp_char {};
template <char... str>
struct to_exp_char<exp_list<exp_char<str>...>> {
  using type = exp_str_impl<str...>;
};

template <const char* p, class idx_ts>
struct my_sptr_impl_conv;

template <const char* p, std::size_t... idx>
struct my_sptr_impl_conv<p, std::index_sequence<idx...>> {
  using type = exp_str_detail::exp_str_impl<p[idx]...>;
};

template <std::size_t N, const char* p>
struct my_sptr {
  static constexpr auto to_my_ch() {
    using cnt_arr = std::make_index_sequence<N>;
    using ret_t = typename my_sptr_impl_conv<p, cnt_arr>::type;
    return ret_t{};
  }
};
template <std::size_t N, const char* p>
using meta_str = decltype(my_sptr<N, p>::to_my_ch());
}  // namespace exp_str_detail

template <class TL>
using to_exp_char_t = get_type<exp_str_detail::to_exp_char<to_exp_list_t<TL>>>;

template <std::size_t N>
struct static_str {
  constexpr static_str(char const (&s)[N]) { std::ranges::copy(s, str); }
  char str[N];
};

template <static_str ss>
constexpr auto operator""_exp_str() {
  constexpr auto size = sizeof(ss.str);
  return exp_str_detail::meta_str<size, ss.str>{};
}

template <static_str ss>
using str_to_list =
    typename exp_str_detail::meta_str<sizeof(ss.str),
                                      ss.str>::template to<exp_list>;
}  // namespace exp_utilities

namespace meta_invoke_protocols {
template <std::size_t>
struct dummy {};

template <template <class...> class apply_shape>
struct meta_function_template_container {};

template <class F>
concept meta_function_t =
    requires { typename meta_function_template_container<F::template apply>; };

template <template <class...> class F, std::size_t... Is>
consteval bool can_instantiate(std::index_sequence<Is...>) {
  return requires { typename F<dummy<Is>...>; };
}

template <template <class...> class F, std::size_t N = 0,
          std::size_t Limit = 64>
consteval std::size_t meta_alias_argc() {
  static_assert(N <= Limit, "template parameter count exceeds limit");

  if constexpr (can_instantiate<F>(std::make_index_sequence<N>{})) {
    if constexpr (!can_instantiate<F>(std::make_index_sequence<N + 1>{})) {
      return N;
    } else {
      return meta_alias_argc<F, N + 1, Limit>();
    }
  } else {
    return meta_alias_argc<F, N + 1, Limit>();
  }
}

}  // namespace meta_invoke_protocols

using meta_invoke_protocols::meta_function_t;

template <class F>
constexpr bool is_meta_function_v = meta_function_t<F>;

namespace meta_invoke_detail {
template <class F, class... L>
struct impl {
  static_assert(meta_function_t<F>,
                "First parameter of meta_invoke must be a meta function");
};
template <class F, class... L>
  requires meta_function_t<F>
struct impl<F, L...> {
  using type = typename F::template apply<L...>;
};

template <class F, class TL>
struct type_list_invoke;

template <class F, template <class...> class L, class... Ts>
struct type_list_invoke<F, L<Ts...>> {
  using type = typename impl<F, Ts...>::type;
};
}  // namespace meta_invoke_detail

template <class F, class... L>
using meta_invoke = typename meta_invoke_detail::impl<F, L...>::type;

template <class F, class TL>
using meta_list_invoke =
    typename meta_invoke_detail::type_list_invoke<F, TL>::type;

namespace meta_invoke_if_detail {
template <bool con, class F, class... Args>
struct meta_function_branch {
  using type = F;
};
template <class F, class... Args>
struct meta_function_branch<true, F, Args...> {
  using type = meta_invoke<F, Args...>;
};
}  // namespace meta_invoke_if_detail

template <bool con>
struct invoke_if {
  template <class F, class... Args>
  using apply =
      typename meta_invoke_if_detail::meta_function_branch<con, F,
                                                           Args...>::type;
};

namespace meta_quote {

template <template <typename> class Template>
struct unary {
  template <typename Arg>
  using apply = Template<Arg>;
};

template <template <typename, typename> class Template>
struct binary {
  template <typename T, typename U>
  using apply = Template<T, U>;
};

template <template <typename, typename> class Template, typename Arg>
struct bind_binary {
  template <typename U>
  using apply = Template<Arg, U>;
};

}  // namespace meta_quote

namespace meta_fold_detail {

template <class T, template <class> class... Fs>
struct impl {};

template <class T, template <class> class F>
struct impl<T, F> {
  using type = F<T>;
};

template <class T, template <class> class First, template <class> class... Rest>
struct impl<T, First, Rest...> {
  using type = typename impl<First<T>, Rest...>::type;
};

}  // namespace meta_fold_detail

template <class T, template <class> class... Fs>
using meta_fold = typename meta_fold_detail::impl<T, Fs...>::type;

template <template <class> class... meta_templates>
struct meta_fold_list {
  template <class T>
  using apply = meta_fold<T, meta_templates...>;
};

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
}  // namespace initialize_details

using initialize_details::has_initializer;
using initialize_details::initialize;
using initialize_details::initialized;

/*A meta obj is a bind of a meta_function and an obj, each time it is invoked,
it update itself to a new type, use ::type to get the inner obj*/
template <class OBJ, class F /*Define how to Update an obj*/>
struct meta_object {
  using type = OBJ;
  template <class... Arg>
  using apply = meta_object<meta_invoke<F, OBJ, Arg...>, F>;

  template <class ANOTHER_OBJ>
  using meta_set = meta_object<ANOTHER_OBJ, F>;
};

template <class OBJ, class F>
  requires has_initializer<F> /*with F::initialize*/
struct meta_object<OBJ, F> {
  using type =
      OBJ;  // note : OBJ could still fail the Cond_Obj exam in initializer
  template <class... Arg>
  using apply =
      meta_object<typename initialize<F, OBJ, Arg...>::type, initialized<F>>;

  template <class ANOTHER_OBJ>
  using meta_set = meta_object<ANOTHER_OBJ, F>;
};

template <class F>
  requires has_initializer<F>
using meta_object_init = meta_object<meta_objects_details::meta_empty, F>;

template <class OBJ, class F, class Ret>
struct meta_ret_object {
  using ret = meta_invoke<Ret, OBJ>;
  using type = OBJ;
  template <class... Arg>
  using apply = meta_ret_object<meta_invoke<F, OBJ, Arg...>, F, Ret>;

  template <class ANOTHER_OBJ>
  using meta_set = meta_ret_object<ANOTHER_OBJ, F, Ret>;
};

template <class OBJ, class F, class Ret>
  requires has_initializer<F>
struct meta_ret_object<OBJ, F, Ret> {
  using initialized_type = typename initialize<F, OBJ>::type;
  using ret = meta_invoke<Ret, initialized_type>;
  using type = initialized_type;
  template <class... Arg>
  using apply = meta_ret_object<typename initialize<F, OBJ, Arg...>::type,
                                initialized<F>, Ret>;

  template <class ANOTHER_OBJ>
  using meta_set = meta_ret_object<ANOTHER_OBJ, F, Ret>;
};

template <class F, class Ret>
  requires has_initializer<F>
using meta_ret_init = meta_ret_object<meta_objects_details::meta_empty, F, Ret>;

namespace meta_states_details {
template <bool Changed, std::uint64_t Flags, std::size_t Index>
consteval std::uint64_t next_flags() {
  if constexpr (Changed) {
    static_assert(Index < std::numeric_limits<std::uint64_t>::digits);
    return Flags | (std::uint64_t{1} << Index);
  } else {
    return Flags;
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

  template <class... Args>
  using apply = meta_states_object<next_type<Args...>, F, Changed_Pred,
                                   new_flags<Args...>(), flag_index + 1>;
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

struct observe_stream {
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
          class Observer = observe_stream>
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
    static constexpr auto for_each(auto&& f, arg_types&&... args) {
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
          // 返回 void：调用后如果需要继续，则递归；否则自然结束。
          if constexpr (_continue_) {
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

    template <class first_arg_type, class... arg_types>
    static constexpr auto for_each_forward(auto&& f, first_arg_type&& first,
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
          if constexpr (_continue_) {
            std::invoke(f, stage_t{}, std::forward<first_arg_type>(first));
            if constexpr (sizeof...(arg_types)) {
              return track_apply_t::for_each_forward(
                  f, std::forward<arg_types>(args)...);
            }
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
  };
};

template <class Cond, class MO, class Generator, class Observer>
struct meta_looper_impl<false, Cond, MO, Generator, Observer> {
  static constexpr bool _continue_ = false;
  using type = typename MO::type;
};
}  // namespace meta_looper_detail

template <class C, class O, class G, class Observer = observe_stream,
          class... ARG_Tys>
using meta_looper_t = typename meta_invoke<
    meta_looper_detail::meta_looper_impl<true, C, O, G, Observer>,
    ARG_Tys...>::type;

template <class C, class O, class G = meta_empty_o,
          class Observer = observe_stream>
using meta_looper =
    meta_looper_detail::meta_looper_impl<true, C, O, G, Observer>;
}  // namespace meta_loop

namespace meta_ios {
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

//=== ChangedPred predicates for meta_stream_s_o ===
// If to is itself a meta_states_object, read its state directly — no
// preview/compare. Otherwise preview F and compare current vs next.
namespace observe_details {
template <class To, class Cache, class = void>
struct to_changed : std::false_type {};

template <class To, class Cache>
struct to_changed<To, Cache, std::void_t<typename To::changed_pred>> {
  static constexpr bool value = To::template changed<Cache>::value;
};
}  // namespace observe_details

template <class F>
struct observe_os_change {
  template <class ThisObj, class FromIs>
  struct apply {
    using cache_t = typename ThisObj::cache;
    static constexpr bool value = [] {
      if constexpr (observe_details::to_changed<typename ThisObj::to,
                                                cache_t>::value)
        return true;
      else {
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
  template <std::size_t Len>
  using ret_t = index_counter<Len - dec_index>;
};

template <std::size_t Len>
struct dec_index_f {
  template <class this_index, class...>
  using apply = typename this_index::dec_t;
};

template <std::size_t Len>
struct ret_index {
  template <class this_index, class...>
  using apply = typename this_index::template ret_t<Len>;
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
};

template <class F>
  requires(meta_objects::initialize_details::has_initializer<F>)
struct transform_iterator_f<F> {
  template <class this_obj, class from_ins>
  using initialize =
      meta_objects::initialize_details::initialize<F, this_obj, from_ins>;
  template <class this_obj, class from_ins>
  using apply = meta_invoke<F, this_obj, from_ins>;
};

template <class F, typename T>
using transform_iterator = meta_object<T, transform_iterator_f<F>>;
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
}  // namespace timer_condition_details
using meta_timer_cond_o =
    meta_object<void, timer_condition_details::timer_receiver>;
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
template <class OBJ, class F, class Changed_Pred, std::uint64_t byte_flag,
          std::size_t flag_index>
struct is_meta_object<
    meta_states_object<OBJ, F, Changed_Pred, byte_flag, flag_index>>
    : std::true_type {};

template <class T>
constexpr bool is_meta_object_v = is_meta_object<T>::value;

template <class T>
struct is_meta_object_ret : std::false_type {};

template <class OBJ, class F, class Ret>
struct is_meta_object_ret<meta_ret_object<OBJ, F, Ret>> : std::true_type {};
template <class T>
constexpr bool is_meta_object_ret_v = is_meta_object_ret<T>::value;

// the meta_istream_type must be a meta_ret_object
template <class T>
concept meta_istream_t = is_meta_object_ret_v<T>;

// the meta_ostream_type can be any type of meta_object
template <class T>
concept meta_ostream_t = is_meta_object_v<T>;

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
constexpr std::uint64_t OP_SKIP = 1ULL << 56;      // off: skip element
constexpr std::uint64_t OP_OS_CLEAR = 1ULL << 57;  // off: clear ostream to
                                                   // empty
constexpr std::uint64_t OP_IS_IDLE = 1ULL << 58;   // off: pop istream but
                                                   // discard
constexpr std::uint64_t OP_OS_IDLE = 1ULL
                                     << 59;  // on: force-call ostream function
                                             // even when pred is false
constexpr std::uint64_t OP_STACK = 1ULL << 60;  // on: push, off: pop
constexpr std::uint64_t OP_TIMER_DEC = 1ULL << 62;
constexpr std::uint64_t OP_BREAK = 1ULL << 63;           // off: break stream
constexpr std::uint64_t OP_DEFAULT = ~std::uint64_t{0};  // all on

// short names
constexpr std::uint64_t opSkip = OP_SKIP;
constexpr std::uint64_t opReset = OP_OS_CLEAR;
constexpr std::uint64_t opCallIs = OP_IS_IDLE;
constexpr std::uint64_t opCallOs = OP_OS_IDLE;
constexpr std::uint64_t opBreak = OP_BREAK;
}  // namespace stream_op_bits

// operator_code<bits...>: those bits are turned OFF (triggered)
template <std::uint64_t... Bits>
struct operator_code {
  static constexpr std::uint64_t value = ~(Bits | ...);
};

// make_base<List, OpCode>: wraps a type_list with a static opr_code
template <class List, std::uint64_t OpCode = stream_op_bits::OP_DEFAULT>
struct make_base {
  using type = List;
  static constexpr std::uint64_t opr_code = OpCode;
};

// states-style update: specialises on meta_states_object.
// Reads opr_code from to::type, applies skip/is_idle/break bits.
struct meta_stream_s_f {
 private:
  template <class Stream, class = void>
  struct update_impl {
    using type = meta_stream<
        meta_object_invoke<typename Stream::to, typename Stream::from>,
        meta_invoke<typename Stream::from>>;
  };

  // To is a meta_states_object
  template <class To, class From>
  struct update_impl<meta_stream<To, From>,
                     std::void_t<typename To::changed_pred>> {
    using cache_t = typename meta_stream<To, From>::cache;
    static constexpr bool pred = To::template changed<cache_t>::value;
    static constexpr std::uint64_t code = [] {
      if constexpr (requires { To::type::opr_code; })
        return To::type::opr_code;
      else
        return stream_op_bits::OP_DEFAULT;
    }();
    static constexpr bool skip = (code & stream_op_bits::OP_SKIP) && !pred;
    static constexpr bool is_idle =
        (code & stream_op_bits::OP_IS_IDLE) && !pred;
    static constexpr bool call_os =
        (code & stream_op_bits::OP_OS_IDLE) && !pred;

    static constexpr std::uint64_t cur_flags =
        To::flags | (pred ? (1ULL << To::size) : 0);

    // record step: flags updated, flag_index++
    using recorded =
        meta_states_object<typename To::type, typename To::function,
                           typename To::changed_pred, cur_flags, To::size + 1>;

    // skip or is_idle: from advances but ostream not touched
    using advancing = meta_stream<recorded, meta_invoke<From>>;

    // call_os: force-call function directly, replace OBJ, advance from
    using called_os =
        meta_stream<typename To::template meta_set<meta_invoke<
                        typename To::function, typename To::type, cache_t>>,
                    meta_invoke<From>>;

    // normal: ostream receives cache
    using normal = meta_stream<meta_object_invoke<To, From>, meta_invoke<From>>;

    using type =
        std::conditional_t<skip || is_idle, advancing,
                           std::conditional_t<call_os, called_os, normal>>;
  };

 public:
  template <class mo_stream, class...>
  using apply = typename update_impl<mo_stream>::type;
};

struct meta_always_false_c_o {
  template <class>
  struct apply : std::false_type {};
};

template <class BF>
struct meta_transfer_until_condition {
  template <class this_stream, class...>
  struct apply {
    static constexpr bool value = [] {
      if constexpr (is_end_stream<typename this_stream::from_t> ||
                    std::is_same_v<typename this_stream::cache,
                                   literal_types::end_of_list>)
        return false;

      using to_t = typename this_stream::to;
      if constexpr (
          requires { typename to_t::changed_pred; } &&
          requires { to_t::type::opr_code; }) {
        constexpr bool pred =
            to_t::template changed<typename this_stream::cache>::value;
        constexpr std::uint64_t code = to_t::type::opr_code;
        // break fires when break bit is on AND pred is false
        if ((code & stream_op_bits::OP_BREAK) && !pred) return false;
      }
      return !meta_invoke<BF, this_stream>::value;
    }();
  };
};

template <class BF>
using meta_transfer_until_condition_o =
    meta_object<void, meta_transfer_until_condition<BF>>;

template <class To, class From, class ChangedPred>
using meta_stream_s_o =
    meta_states_object<meta_stream<To, From>, meta_stream_s_f, ChangedPred>;

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
    io_stream_transform_details::io_stream_traits::meta_ostream_t<T>;

using meta_range_continue = io_stream_transform_details::meta_always_false_c_o;

template <meta_ostream_t To, meta_istream_t From, class BF,
          class Observer = observe_stream,
          class ChangedPred =
              io_stream_transform_details::default_stream_change_pred>
using meta_transfer_until_impl = meta_invoke<meta_looper<
    io_stream_transform_details::meta_transfer_until_condition_o<BF>,
    io_stream_transform_details::meta_stream_s_o<To, From, ChangedPred>,
    meta_empty_o, Observer>>;

template <meta_ostream_t To, meta_istream_t From,
          class BF = meta_range_continue, class Observer = observe_stream,
          class ChangedPred =
              io_stream_transform_details::default_stream_change_pred>
using meta_transfer_until =
    meta_transfer_until_impl<To, From, BF, Observer, ChangedPred>;

template <meta_ostream_t To, meta_istream_t From,
          class break_f = meta_range_continue, class Observer = observe_stream,
          class ChangedPred =
              io_stream_transform_details::default_stream_change_pred>
using transfer_until = typename meta_transfer_until<To, From, break_f, Observer,
                                                    ChangedPred>::type;

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

template <class type_list>
using meta_ostream =
    io_stream_transform_details::meta_basic_ostream_detail::meta_basic_ostream<
        type_list>;

// operation-code bits for meta_states_object opr_code
namespace stream_op_bits = io_stream_transform_details::stream_op_bits;
using io_stream_transform_details::make_base;
using io_stream_transform_details::operator_code;

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
                       accept_pred>;

// A states-based iterator ostream: OBJ IS the flags, pred defaults to
// always-changed.
using meta_states_iterator = meta_states_object<
    std::integral_constant<std::uint64_t, 0>,
    io_stream_transform_details::meta_states_iterator_detail::
        iterator_passthrough,
    io_stream_transform_details::meta_states_iterator_detail::always_changed>;
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

struct meta_stream_skip_signal {};

template <class T>
constexpr bool is_skip_signal = std::is_same_v<T, meta_stream_skip_signal>;

template <class meta_stream_type>
using wait_for_end =
    std::conditional_t<!length_equal<typename meta_stream_type::from::type, 0>,
                       meta_stream_skip_signal, meta_stream_type>;
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
    meta_istream_list<protocol_container<PS>...>>::to_t;

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
                has_no_protocol_v<protocols::stream_cache_t, PS...>) {
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
                has_no_protocol_v<protocols::stream_cache_t, PS...>) {
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
  template <class this_seek, class from_ins>
  using apply = seek_to<this_seek::advance_t::value, from_ins>;
  template <class this_seek, class from_ins>
  using initialize = seek_to<0, from_ins>;
};
}  // namespace meta_aligned_iterator_details
/// <summary>
/// for meta_aligned_iterator, it is a meta_ostream_t that can seek to an
/// aligned address for a specific type in a byte stream, and it can also
/// advance to the next aligned address for the next type.
/// </summary>
using meta_aligned_iterator =
    meta_object_init<meta_aligned_iterator_details::advance_f>;
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
struct advance_node {
  template <class this_pipe>
  struct advance_impl {
    using stream_invoke =
        transfer<1, typename this_pipe::to, typename this_pipe::from>;
    using from = typename stream_invoke::from;
    using to = typename stream_invoke::to;
  };
  template <class this_pipe>
  using apply = io_stream_transform_details::meta_stream<
      typename advance_impl<this_pipe>::to,
      typename advance_impl<this_pipe>::from>;
};

template <template <class> class... ps>
struct ret_from_node {
  template <class this_pipe>
  using apply = meta_fold<this_pipe, protocols::stream_to_t, ps...>;
};

template <meta_istream_t is, meta_ostream_t os, template <class> class... ps>
using stream_istream =
    meta_ret_object<transfer<1, os, is>, advance_node, ret_from_node<ps...>>;

template <class this_pipe>
using skip_node = io_stream_transform_details::meta_stream<
    typename this_pipe::to,
    meta_invoke<invoke_if<(exp_size<typename this_pipe::from::type> > 0)>,
                typename this_pipe::from>>;

using meta_nothing = meta_objects::meta_empty_o;
template <class meta_function_type, typename reset_f>
struct skip_advance_node {
  template <class this_pipe>
  struct advance_impl {
    using stream_invoke = meta_all_transfer<
        std::conditional_t<!std::is_same_v<reset_f, meta_nothing>,
                           typename this_pipe::to::template meta_set<
                               meta_invoke<reset_f, this_pipe>>,
                           typename this_pipe::to>,
        typename this_pipe::from, meta_function_type>;
    using from = typename skip_node<stream_invoke>::from;
    using to = typename skip_node<stream_invoke>::to;
  };
  template <class this_pipe>
  using apply = io_stream_transform_details::meta_stream<
      typename advance_impl<this_pipe>::to,
      typename advance_impl<this_pipe>::from>;
};

template <meta_istream_t is, meta_ostream_t os, class break_f, class reset_f,
          template <class> class... ps>
using skip_stream_istream =
    meta_ret_object<skip_node<meta_all_transfer<os, is, break_f>>,
                    skip_advance_node<break_f, reset_f>, ret_from_node<ps...>>;

template <std::size_t N, meta_istream_t is, meta_ostream_t os,
          template <class> class... ps>
struct transfer_pipe {
  template <meta_ostream_t another_os, template <class> class... other_ps>
  using all_to =
      transfer_pipe<exp_size<typename is::type>, stream_istream<is, os, ps...>,
                    another_os, other_ps...>;
  template <meta_ostream_t another_os, template <class> class... other_ps>
  using each_to =
      transfer_pipe<exp_size<typename is::type>,
                    stream_istream<is, os, protocols::forward_last, ps...>,
                    another_os, other_ps...>;

  template <std::size_t Nc, meta_ostream_t another_os,
            template <class> class... other_ps>
  using to =
      transfer_pipe<Nc, stream_istream<is, os, ps...>, another_os, other_ps...>;

  template <std::size_t Nc, meta_ostream_t another_os, class break_f,
            typename reset_f, template <class> class... other_ps>
  using skip_to =
      transfer_pipe<Nc, skip_stream_istream<is, os, break_f, reset_f, ps...>,
                    another_os, other_ps...>;
  using from = stream_istream<is, os, ps...>;
  using transfer = meta_ios::transfer<N, os, is>;
  template <class meta_function_type>
  using skip = meta_ios::meta_all_transfer<os, is, meta_function_type>;
};

}  // namespace meta_pipe_node_details

namespace pipe {
using meta_objects::meta_timer_object_details::meta_always_continue;
using meta_pipe_node_details::skip_stream_istream;
using meta_pipe_node_details::transfer_pipe;
template <meta_istream_t is>
struct transfer {
  template <meta_ostream_t another_os, template <class> class... other_ps>
  using all_to =
      transfer_pipe<exp_size<typename is::type>, is, another_os, other_ps...>;

  template <std::size_t Nc, meta_ostream_t another_os,
            template <class> class... other_ps>
  using to = transfer_pipe<Nc, is, another_os, other_ps...>;

  template <meta_ostream_t another_os, class break_f, class reset_f,
            template <class> class... other_ps>
  using skip_to = pipe::transfer<
      skip_stream_istream<is, another_os, break_f, reset_f, other_ps...>>;
};

}  // namespace pipe
}  // namespace meta_ios
