#include<type_traits>
#include<utility>
#include<array>
#include<functional>
#include<algorithm>

namespace exp_utilities
{
    namespace literal_types {
        struct no_exist_type {};

        template<class L>
        struct end_of_list {};

        template<class T>
        struct error {
            template<auto str>
            struct message {
                static constexpr bool value = false;
            };
        };
    }
    namespace exp_select_detail {
        template<int I, class T> struct select_type {
            using type = literal_types::no_exist_type;
        };

        template<int I, template<typename ...> typename TL, class First, class ... Rest>
        struct select_type<I, TL<First, Rest...>> {
            using type = typename select_type<I - 1, TL<Rest...>>::type;
        };

        template<template<typename ...> typename TL, class First, class ... Rest>
        struct select_type<0, TL<First, Rest...>> {
            using type = First;
        };

        template<int I, template<class...> class TL>
        struct select_type<I, TL<>> {
            using type = literal_types::no_exist_type;
            static_assert(false, "Index out of bounds for exp_select");
        };
    }

    template<std::size_t I, class TL>
    using exp_select = typename exp_select_detail::select_type<I, TL>::type;

    namespace exp_list_select_detail {
        template<std::size_t ...I>
        struct list_select_impl {
            template<template<typename ...>typename TL, class ...Tys>
            static constexpr auto apply_impl(TL<Tys...>) {
                return TL<exp_select<I, TL<Tys...>>...>{};
            }

            template<typename TL>
            using apply = decltype(apply_impl(std::declval<TL>()));

        };
    }

    template<std::size_t ...I>
    struct exp_list_select {
        template<typename TL>
        using apply = typename exp_list_select_detail::list_select_impl<I...>::template apply<TL>;
    };

    template<class index_sequence_t>
    struct to_exp_list_select_t_impl;

    template<std::size_t ...I>
    struct to_exp_list_select_t_impl<std::index_sequence<I...>> {
        using type = exp_list_select<I...>;
    };

    template<class index_sequence_t>
    using to_exp_list_select_t = typename to_exp_list_select_t_impl<index_sequence_t>::type;

    template<class ...Tys>
    struct exp_list;



    namespace exp_list_details {
        template<class TL> struct to_exp_list {};
        template<template<class ...> class TL, class ...Typs> struct to_exp_list<TL<Typs...>>
        {
            using type = exp_list<Typs...>;
        };

        template<typename ...Tys> struct my_list {};

        template<class TL> struct is_exp_list_based : std::false_type {};
        template<template<class ...> class TL, class ...TS>
        struct is_exp_list_based<TL<TS...>>
        {
            static constexpr bool value = std::is_base_of_v<exp_list<TS...>, TL<TS...>>;
        };

        template<class TL>
        using pop_last = std::conditional_t<(TL::length >= 1), typename to_exp_list_select_t<std::make_index_sequence<TL::length - 1>>::template apply<TL>, exp_list<>>;

    }

    template<class First, class ...Tys>
    struct exp_list<First, Tys...> {
        static constexpr std::size_t length = sizeof...(Tys) + 1;
        template<template<class...> class TL> using to = TL<First, Tys...>;
        template<template<class> class F> using for_each = exp_list<F<First>, F<Tys>...>;
        template<class T> using push_back = exp_list<First, Tys..., T>;
        template<class T> using push_front = exp_list<T, First, Tys...>;
        using pop_front = exp_list<Tys...>;
        using front = First;
        using back = exp_select<sizeof...(Tys), exp_list_details::my_list<First, Tys...>>;
        template<std::size_t I> requires (I < length)
            using at = exp_select<I, exp_list<First, Tys...>>;
    };

    template<>
    struct exp_list<> {
        static constexpr std::size_t length = 0;
        template<template<class...> class TL> using to = TL<>;
        template<template<class> class F> using for_each = exp_list<>;
        template<class T> using push_back = exp_list<T>;
        template<class T> using push_front = exp_list<T>;
        using pop_front = literal_types::no_exist_type;
        using pop_back = literal_types::no_exist_type;
        using front = literal_types::no_exist_type;
        using back = literal_types::no_exist_type;
    };

    template<class TL>
    using to_exp_list_t = typename exp_list_details::to_exp_list<TL>::type;

    template<class TL>  concept exp_list_based = exp_list_details::is_exp_list_based<TL>::value;
    namespace exp_size_details {
        template<class TL>
        concept has_length = requires { TL::length; };
        template<class TL>
        struct type_list_size
        {
            static constexpr std::size_t value = 0;
        };

        template<class TL> requires has_length<TL>
        struct type_list_size<TL>
        {
            static constexpr std::size_t value = TL::length;
        };

        template<template<class...> class TL, class ...Tys>
        struct type_list_size<TL<Tys...>>
        {
            static constexpr std::size_t value = sizeof...(Tys);
        };
    }
    template<class L>
    constexpr std::size_t exp_size = exp_size_details::type_list_size<L>::value;

    namespace max_index_details {
        template<class TL>
        struct max_index_type {
            static_assert((exp_size<TL> > 0), "Error: empty list");
            static constexpr std::size_t value = exp_size<TL> -1;
        };
    }

    template<class L>
    constexpr std::size_t max_index = max_index_details::max_index_type<L>::value;

    template<class L, std::size_t N>
    constexpr bool length_equal = (exp_size<L> == N);

    template<std::size_t N>
    struct length_is {
        template<class L>
        struct apply : std::bool_constant<length_equal<L, N>> {};
    };

    template<std::size_t N>
    struct length_is_not {
        template<class L>
        struct apply : std::bool_constant<!length_equal<L, N>> {};
    };

    template<std::size_t N>
    struct length_less {
        template<class L>
        struct apply : std::bool_constant < exp_size<L> < N> {};
    };

    template<std::size_t N>
    struct length_greater {
        template<class L>
        struct apply : std::bool_constant<(exp_size<L> > N)> {};
    };
    namespace exp_find_detail {
        template<class IDX, class T, class TL> struct find_impl;

        template<class IDX, class T, template<class...> class TL, class First, class... Ts>
        struct find_impl<IDX, T, TL<First, Ts...>> {
            using type = typename std::conditional_t<
                std::is_same<T, First>::value,
                std::integral_constant<std::size_t, IDX::value>,
                typename find_impl<
                std::integral_constant<std::size_t, IDX::value + 1>, T, TL<Ts...>
                >::type
            >;

        };

        template<class IDX, class T, template<class...> class TL>
        struct find_impl<IDX, T, TL<>> {
            using type = IDX; // idx overflow
        };
    }

    template<class T, class TL>
    using exp_find = typename exp_find_detail::find_impl<std::integral_constant<std::size_t, 0>, T, TL>::type;


    template<class T, class TL>
    using exp_try_find = std::integral_constant<bool, (max_index<TL> >= exp_find<T, TL>::value)>;

    namespace get_type_detail
    {
        template<class T, class U = std::void_t<>>
        struct get_type_impl : std::false_type
        {
            using type = literal_types::no_exist_type;
        };

        template<class T>
        struct get_type_impl<T, std::void_t<typename T::type>> : std::true_type
        {
            using type = typename T::type;
        };
    }
    template<class T>
    using get_type = typename get_type_detail::get_type_impl<T>::type;

    template<class T>
    constexpr bool has_type = get_type_detail::get_type_impl<T>::value;

    template<class T>
    concept has_value = requires{
        T::value;
    };
    template<class T, auto cmp>
    constexpr bool value_equal = false;

    template<class T, auto cmp> requires has_value<T>
    constexpr bool value_equal<T, cmp> = T::value == cmp;

    template<auto val>
    struct value_is {
        template<class T>
        struct apply : std::bool_constant<value_equal<T, val>> {};
        template<class F2>
        struct OR
        {
            template<class T>
            struct apply : std::bool_constant<value_equal<T, val> || F2::template apply<T>::value> {};
        };
    };

    template<auto val>
    struct value_is_not {
        template<class T>
        struct apply : std::bool_constant<!value_equal<T, val>> {};
        template<class F2>
        struct OR
        {
            template<class T>
            struct apply : std::bool_constant<!(value_equal<T, val> || F2::template apply<T>::value)> {};
        };
    };
    struct below_zero {
        static constexpr size_t value = 0;
    };

    template<std::size_t I>
    using Idx = std::integral_constant<std::size_t, I>;

    namespace exp_indices_details {
        template<class _Idx, size_t _I>
        struct Add_Idx {};

        template<std::size_t I> struct Add_Idx<below_zero, I>
        {
            using type = Idx<0>;
        };
        template<template<size_t> class idx, std::size_t I_in_Idx, std::size_t I>
        struct Add_Idx<idx<I_in_Idx>, I>
        {
            using type = idx<I_in_Idx + I>;
        };
        template<std::size_t I, size_t ADD>
        struct Add_Idx<Idx<I>, ADD> {
            using type = Idx<I + ADD>;
        };
    }


    template<class idx, size_t I>
    using add_idx_t = typename exp_indices_details::Add_Idx<idx, I>::type;

    template<class _Idx>
    using inc_idx_t = add_idx_t<_Idx, 1>;


    template<size_t ..._elements>
    struct meta_array : exp_list<Idx<_elements>...>
    {
        using cv_typelist = exp_list<Idx<_elements>...>;
        template<template<size_t...I> class integer_array_type>
        using to = integer_array_type<_elements...>;
        template<size_t I> using at = exp_select<I, cv_typelist>;
        template<size_t I> static constexpr size_t get() {
            return at<I>::value;
        }

        static constexpr size_t length = cv_typelist::length;
        static constexpr size_t sum = (0 + ... + _elements);
        static consteval std::array<std::size_t, length> array() {
            return { _elements... };
        }
    };

    template<class TL>
    struct to_meta_array {
        static_assert(false, "not all elements has value");
    };

    template<template<class...> class integer_wrapper, std::size_t ...elements>
    struct to_meta_array<integer_wrapper<Idx<elements>...>> {
        using type = meta_array<elements...>;
    };

    template<class TL>
    using to_meta_array_t = get_type<to_meta_array<TL>>;

    template<char c>
    struct exp_char {
        static constexpr char value = c;
        constexpr operator char() const { return value; }
    };

    namespace exp_str_detail {
        template<char ...str>
        struct exp_str_impl : exp_list<exp_char<str>...> {
            constexpr exp_str_impl() {}
            const char m_str[sizeof...(str)]{ str... };
            operator const char* () {
                return m_str;
            }
            static constexpr std::size_t length = sizeof...(str);
        };
        template<class TL> struct to_exp_char {};
        template<char ...str>
        struct to_exp_char<exp_list<exp_char<str>...>>
        {
            using type = exp_str_impl<str...>;
        };

        template<const char* p, class idx_ts>
        struct my_sptr_impl_conv;

        template<const char* p, std::size_t ...idx>
        struct my_sptr_impl_conv<p, std::index_sequence<idx...>>
        {
            using type = exp_str_detail::exp_str_impl<p[idx]...>;
        };

        template<std::size_t N, const char* p>
        struct my_sptr {
            static constexpr auto to_my_ch() {
                using cnt_arr = std::make_index_sequence<N>;
                using ret_t = typename my_sptr_impl_conv<p, cnt_arr>::type;
                return ret_t{};
            }
        };
        template<std::size_t N, const char* p>
        using meta_str = decltype(my_sptr<N, p>::to_my_ch());
    }

    template<class TL> using to_exp_char_t = get_type<exp_str_detail::to_exp_char<to_exp_list_t<TL>>>;

    template<std::size_t N>
    struct static_str {
        constexpr static_str(char const(&s)[N]) {
            std::ranges::copy(s, str);
        }
        char str[N];
    };

    template<static_str ss>
    constexpr auto operator ""_exp_str() {
        constexpr auto size = sizeof(ss.str);
        return exp_str_detail::meta_str<size, ss.str>{};
    }

    template<static_str ss>
    using str_to_list = typename exp_str_detail::meta_str<sizeof(ss.str), ss.str>::template to<exp_list>;
}

namespace meta_invoke_protocols {
    template<template<class> class F>
    consteval std::size_t meta_alias_argc() {
        return 1;
    }
    template<template<class, class> class F>
    consteval std::size_t meta_alias_argc() {
        return 2;
    }
    template<template<class, class, class> class F>
    consteval std::size_t meta_alias_argc() {
        return 3;
    }
    template<template<class, class, class, class> class F>
    consteval std::size_t meta_alias_argc() {
        return 4;
    }
    template<template<class, class, class, class, class> class F>
    consteval std::size_t meta_alias_argc() {
        return 5;
    }
    template<template<class, class, class, class, class, class> class F>
    consteval std::size_t meta_alias_argc() {
        return 6;
    }
    template<template<class, class, class, class, class, class, class> class F>
    consteval std::size_t meta_alias_argc() {
        return 7;
    }
    template<template<class, class, class, class, class, class, class, class> class F>
    consteval std::size_t meta_alias_argc() {
        return 8;
    }
    template<template<class, class, class, class, class, class, class, class, class> class F>
    consteval std::size_t meta_alias_argc() {
        return 9;
    }
    template<template<class, class, class, class, class, class, class, class, class, class> class F>
    consteval std::size_t meta_alias_argc() {
        return 10;
    }
    namespace is_meta_function_trait_detail {
        template<template<class ...> class apply_shape> struct meta_function_template_container {};

        template<class F, class En = void> struct is_meta_function_type : std::false_type {};

        template<class F> struct is_meta_function_type <F, std::void_t<meta_function_template_container<F::template apply>>> : std::true_type {};

    }

    template<class F> constexpr bool is_meta_function_v = is_meta_function_trait_detail::is_meta_function_type<F>::value;

    namespace meta_invoke_detail {
        template<class F, class ...L>
        struct impl {
            static_assert(is_meta_function_v<F>, "First parameter of meta_invoke must be a meta function");
        };
        template<class F, class ...L> requires is_meta_function_v<F>
        struct impl<F, L...> {
            using type = typename F::template apply<L...>;
        };

        template<class F, class TL>
        struct type_list_invoke;

        template<class F, template<class...> class L, class ...Ts>
        struct type_list_invoke<F, L<Ts...>> {
            using type = typename impl<F, Ts...>::type;
        };
    }


    template<class F, class ...L>
    using meta_invoke = typename meta_invoke_detail::impl<F, L...>::type;


    template<class F, class TL>
    using meta_list_invoke = typename meta_invoke_detail::type_list_invoke<F, TL>::type;

    namespace meta_invoke_if_detail {
        template<bool con, class F, class ...Args>
        struct meta_function_branch
        {
            using type = F;
        };
        template<class F, class ...Args>
        struct meta_function_branch<true, F, Args...>
        {
            using type = meta_invoke<F, Args...>;
        };
    } // namespace meta_invoke_if_detail

    template<bool con> struct invoke_if
    {
        template<class F, class ...Args> using apply = typename meta_invoke_if_detail::meta_function_branch<con, F, Args...>::type;
    };

    namespace meta_quote
    {

        template<template<typename> class Template>
        struct unary
        {
            template<typename Arg>
            using apply = Template<Arg>;
        };


        template<template<typename, typename> class Template>
        struct binary
        {
            template<typename T, typename U>
            using apply = Template<T, U>;
        };


        template<template<typename, typename> class Template, typename Arg>
        struct bind_binary
        {
            template<typename U>
            using apply = Template<Arg, U>;
        };

    } // namespace meta_quote

    namespace meta_fold_detail {

        template<class T, template<class> class ...Fs>
        struct impl {};

        template<class T, template<class> class F>
        struct impl<T, F> {
            using type = F<T>;
        };

        template<class T, template<class> class First, template<class> class ...Rest>
        struct impl<T, First, Rest...> {
            using type = typename impl<First<T>, Rest...>::type;
        };


    }

    template<class T, template<class>class ...Fs>
    using meta_fold = typename meta_fold_detail::impl<T, Fs...>::type;

    template<template<class> class ...meta_templates>
    struct meta_fold_list {
        template<class T>
        using apply = meta_fold<T, meta_templates...>;
    };
}

namespace meta_objects {

    using namespace meta_invoke_protocols;

    namespace meta_objects_details {
        struct meta_empty { static constexpr int value = 0; };
        struct meta_empty_fn { template<class T, class ...> using apply = T; };
    }

 namespace initialize_details{
     template<template<class ...> class apply_shape> struct meta_initializer_container {};

     template<class F, class En = void> struct is_initializer_type : std::false_type {};

     template<class F> 
     struct is_initializer_type<F, std::void_t<meta_initializer_container<F::template initialize>>> : std::true_type {};

     template<class F>
     constexpr bool has_initializer = is_initializer_type<F>::value;

     template<class F>
     struct initialized {
         template<class OBJ, class ...Arg>
         using apply = meta_invoke<F, OBJ, Arg...>;
     };

     template<class F, class ...Arg>
     struct initialize {
         using type = typename F::template initialize<Arg...>;
     };
 }

 using initialize_details::has_initializer;
 using initialize_details::initialized;
 using initialize_details::initialize;

 /*A meta obj is a bind of a meta_function and an obj, each time it is invoked, it update itself to a new type,
 use ::type to get the inner obj*/
 template<class OBJ, class F/*Define how to Update an obj*/>
 struct meta_object
 {
     using type = OBJ;
     template<class ...Arg>
     using apply = meta_object<meta_invoke<F, OBJ, Arg...>, F>;

     template<class ANOTHER_OBJ>
     using meta_set = meta_object<ANOTHER_OBJ, F>;
 };

 template<class OBJ, class F> requires has_initializer<F>/*with F::initialize*/
 struct meta_object<OBJ, F>
 {
     using type = OBJ; //note : OBJ could still fail the Cond_Obj exam in initializer
     template<class ...Arg>
     using apply = meta_object<typename initialize<F, OBJ, Arg...>::type, initialized<F>>;

     template<class ANOTHER_OBJ>
     using meta_set = meta_object<ANOTHER_OBJ, F>;
 };

 template<class F> requires has_initializer<F>
 using meta_object_init = meta_object<void, F>;
 
 template<class OBJ, class F, class Ret>
 struct meta_ret_object
 {
     using ret = meta_invoke<Ret, OBJ>;
     using type = OBJ;
     template<class ...Arg>
     using apply = meta_ret_object<meta_invoke<F, OBJ, Arg...>, F, Ret>;

     template<class ANOTHER_OBJ>
     using meta_set = meta_ret_object<ANOTHER_OBJ, F, Ret>;
 };

 template<class OBJ, class F, class Ret> requires has_initializer<F>
 struct meta_ret_object<OBJ, F, Ret>
 {
     using ret = meta_invoke<Ret, typename initialize<F, OBJ>::type>;
     using type = typename initialize<F, OBJ>::type;
     template<class ...Arg>
     using apply = meta_ret_object<meta_invoke<F, typename initialize<F, OBJ>::type, Arg...>, initialized<F>, Ret>;

     template<class ANOTHER_OBJ>
     using meta_set = meta_ret_object<ANOTHER_OBJ, F, Ret>;
 };

 template<class F, class Ret> requires has_initializer<F>
 using meta_ret_object_init = meta_ret_object<meta_objects_details::meta_empty, F, Ret>;


 namespace meta_timer_object_details {
     struct meta_break_signal :std::false_type {};

     template<class Fn, class DF> struct meta_break_if
     {
         template<class T> using apply = std::conditional_t<meta_invoke<Fn, T>::value, meta_break_signal, DF>;
     };
     struct meta_always_continue
     {
         template<class T> struct apply :std::false_type
         {};
     };
    template<class MTO, class ...Arg>
    struct stop_forward_next_if_break_f_is_true {
        using type = MTO;
    };
    template<template<std::size_t, class, class, class>class meta_timer_template, std::size_t times, class OBJ, class F, class break_f, class ...Arg> requires (!std::is_same_v<typename meta_timer_template<times, OBJ, F, break_f>::timer, meta_break_signal>)
    struct stop_forward_next_if_break_f_is_true<meta_timer_template<times, OBJ, F, break_f>, Arg...> {
        using type = meta_timer_template<times - 1, meta_invoke<F, OBJ, Arg...>, F, break_f>;
    };
 }


 //since modifying to timer is forbidden, there is no initializer for meta_timer_object
 template<std::size_t times, class OBJ, class F, class break_f =
     //when true, looper breaks
     meta_timer_object_details::meta_always_continue>
 struct meta_timer_object
 {
     using timer = meta_invoke<
            
         //meta_break_if<Pred, Default_if_false_t>, Arg>
         //if(Pred<Arg> == true) return meta_break_signal
         //else return Default_if_false_t
         meta_timer_object_details::meta_break_if<break_f, std::integral_constant<bool, (times > 0)>>,
         OBJ
     >;

     using type = OBJ;
     template<class ...Arg>
     using apply = typename meta_timer_object_details::stop_forward_next_if_break_f_is_true<meta_timer_object<times, OBJ, F, break_f>, Arg...>::type;

     template<class ANOTHER_OBJ>
     using meta_set = meta_timer_object<times, ANOTHER_OBJ, F, break_f>;

     template<size_t reset_time>
     using reset = meta_timer_object<reset_time, OBJ, F, break_f>;
 };
using meta_empty_o = meta_object<meta_objects_details::meta_empty, meta_objects_details::meta_empty_fn>;
    namespace meta_timer_object_details {
        template<class OBJ, size_t N, class break_f> struct To_Timer {};
        template<class obj, class F, size_t N, class break_f> struct To_Timer<meta_object<obj, F>, N, break_f>
        {
            using type = meta_timer_object<N, obj, F, break_f>;
        };

        template<class mo, class F> struct Break_If {};
        template<size_t N, class OBJ, class MO_F, class F> struct Break_If<meta_timer_object<N, OBJ, MO_F>, F>
        {
            using type = meta_timer_object<N, OBJ, MO_F, F>;
        };
    }

    template<class OBJ, size_t N, class break_f = meta_timer_object_details::meta_always_continue>
    using to_timer = typename meta_timer_object_details::To_Timer<OBJ, N, break_f>::type;

    //set a break condition for meta_timer_object
    template<class MTO, class BF>
    using break_if = exp_utilities::get_type<meta_timer_object_details::Break_If<MTO, BF>>;

    namespace meta_objects_invoke_details {
        template<class From_T, class To_T>
        struct meta_transfer_object_impl {
            using type = typename To_T::template meta_set<typename From_T::type>;
        };

        //transfer timer if invoke to a meta_timer_oject
        template<size_t times, class obj, class F, class To_T, class B>
        struct meta_transfer_object_impl<meta_timer_object<times, obj, F, B>, To_T> {
            using type = typename To_T::template meta_set<typename meta_timer_object<times, obj, F, B>::timer>;
        };

        //transfer returns if invoke to a meta_ret_object
        template<class Ret, class obj, class F, class To_T>
        struct meta_transfer_object_impl<meta_ret_object<obj, F, Ret>, To_T> {
            using type = typename To_T::template meta_set<typename meta_ret_object<obj, F, Ret>::ret>;
        };
    }

    template<class From_T, class To_T>
    using meta_transfer_object = typename meta_objects_invoke_details::meta_transfer_object_impl<From_T, To_T>::type;

    namespace meta_objects_invoke_details {
        //the meta_object is itself a meta_function
        //if two meta_objects invoked, invoke the first object with type in second object
        template<class OBJ1, class OBJ2>
        struct Meta_Object_Invoke { using type = meta_invoke<OBJ1, typename OBJ2::type>; };

        //if invoke with meta_ret_object, invoke the first object with returns
        template<class OBJ1, class Obj2, class F, class Ret>
        struct Meta_Object_Invoke<OBJ1, meta_ret_object<Obj2, F, Ret>>
        {
            using type = meta_invoke<OBJ1, typename meta_ret_object<Obj2, F, Ret>::ret>;
        };

    }
    template<class OBJ1, class OBJ2>
    using meta_object_invoke = typename meta_objects_invoke_details::Meta_Object_Invoke<OBJ1, OBJ2>::type;

    namespace invoke_object_if_details {
        template<bool, class MO1, class MO2> struct meta_o_branch {
            using type = MO1;
        };
        template<class MO1, class MO2> struct meta_o_branch<true, MO1, MO2> {
            using type = meta_object_invoke<MO1, MO2>;
        };
    }

    template<bool con> struct invoke_object_if
    {
        template<class MO1, class MO2> using apply = typename invoke_object_if_details::meta_o_branch<con, MO1, MO2>::type;
    };

}

namespace meta_loop {
    using namespace meta_objects;

    //Note: All template parameters are meta objects
    namespace meta_looper_detail {
        template<bool, class Condition, class OBJ, class Generator = meta_empty_o>
        struct meta_looper_impl
        {

            template<class ...Args> struct apply {

                //transfer current obj to  condition_obj to judge
                //transfer different context based on types of meta_object
                using _continue_t = typename meta_invoke<meta_transfer_object<OBJ, Condition>>::type;
                static const bool _continue_ = _continue_t::value;

                //invoke generator object if condition is true
                using generator_stage_o = meta_invoke<invoke_if<_continue_>, Generator, Args...>;

                //invoke Obj object if condition is true
                using result_stage_o = meta_invoke<invoke_object_if<_continue_>, OBJ, generator_stage_o>;

                //recursively loop for result
                using track_apply_t = meta_invoke<invoke_if<_continue_>, meta_looper_impl<
                    _continue_,
                    Condition,
                    result_stage_o,
                    generator_stage_o
                >, Args...>;
                using type = typename track_apply_t::type;

                template<class ...arg_types>
                static constexpr auto for_each(auto&& f, arg_types &&...args)->decltype(std::invoke(f, typename result_stage_o::type{}, std::forward<arg_types>(args)...)) {
                    if constexpr (_continue_) {
                        std::invoke(f, typename result_stage_o::type{}, std::forward<arg_types>(args)...);
                        return track_apply_t::for_each(f, std::forward<arg_types>(args)...);
                    }
                    else{
                        return std::invoke(f, typename result_stage_o::type{}, std::forward<arg_types>(args)...);
                    }
                }

                template<class first_arg_type, class ...arg_types>
                static constexpr auto for_each_forward(auto&& f, first_arg_type&& first, arg_types &&...args)->decltype(std::invoke(f, typename result_stage_o::type{}, std::forward<first_arg_type>(first))) {
                    if constexpr (_continue_) {
                        if constexpr (sizeof ...(arg_types)){
                            std::invoke(f, typename result_stage_o::type{}, std::forward<first_arg_type>(first));
                            return track_apply_t::for_each_forward(f, std::forward<arg_types>(args)...);
                        }
                        else{
                            return std::invoke(f, typename result_stage_o::type{}, std::forward<first_arg_type>(first));
                        }
                    }
                    else{
                        std::invoke(f, typename result_stage_o::type{}, std::forward<first_arg_type>(first));
                    }
                }
            };
        };


        template<class Cond, class MO, class Generator> struct meta_looper_impl<false, Cond, MO, Generator>
        {
            static constexpr bool _continue_ = false;
            using type = typename MO::type;
            template<class ...arg_types>
            static constexpr auto for_each(auto&& f, arg_types &&...args) {
                return std::invoke(f, typename MO::type{}, std::forward<arg_types>(args)...);
            }
            template<class first_arg_type, class ...arg_types>
            static constexpr auto for_each_forward(auto&& f, first_arg_type&& first, arg_types &&...args) {
                return std::invoke(f, typename MO::type{}, std::forward<first_arg_type>(first));
            }
        };
    }

    template<class C, class O, class G, class ...ARG_Tys>
    using meta_looper_t = typename meta_invoke<meta_looper_detail::meta_looper_impl<true, C, O, G>, ARG_Tys...>::type;

    template<class C, class O, class G>
    using meta_looper = meta_looper_detail::meta_looper_impl<true, C, O, G>;
}

namespace meta_ios {
    using namespace meta_invoke_protocols;
    using namespace meta_objects;
    namespace io_stream_transform_details {
        namespace transfer_protocols {
            namespace details {
                template<class meta_function_type>
                struct this_policy_type {
                    template<class this_type, class from_ins>
                    using apply = meta_invoke<meta_function_type, this_type>;
                };

                template<class meta_function_type>
                struct arg_policy_type {
                    template<class this_type, class from_ins>
                    using apply = meta_invoke<meta_function_type, from_ins>;
                };

                template<class meta_function_type>
                struct stream_to_policy_type {
                    template<class this_stream_t, class from_ins>
                    using apply = meta_invoke<meta_function_type, typename this_stream_t::to::type, from_ins>;
                };

                template<class meta_function_type>
                struct stream_to_arg_type {
                    template<class this_stream_t, class from_ins>
                    using apply = meta_invoke<meta_function_type, from_ins>;
                };

                template<class meta_function_type>
                struct stream_to_this_policy_type {
                    template<class this_stream_t, class from_ins>
                    using apply = meta_invoke<meta_function_type, typename this_stream_t::to::type>;
                };
                template<class meta_function_type> requires (meta_alias_argc<meta_function_type::template apply>() == 1)
                    struct stream_to_this_policy_type<meta_function_type> {
                    template<class this_stream_t>
                    using apply = meta_invoke<meta_function_type, typename this_stream_t::to::type>;
                };

                template<class meta_function_type>
                struct stream_cache_policy_type {
                    template<class this_stream_t, class from_ins>
                    using apply = meta_invoke<meta_function_type, typename this_stream_t::cache, from_ins>;
                };

                template<class meta_function_type>
                struct stream_from_only_policy_type {
                    template<class this_stream_t>
                    using apply = meta_invoke<meta_function_type, typename this_stream_t::from::type>;
                };

                template<class meta_function_type>
                struct stream_cache_this_policy_type {
                    template<class this_stream_t>
                    using apply = meta_invoke<meta_function_type, typename this_stream_t::cache>;
                };

            }
        }
    }
    namespace io_stream_transform_details {
        using exp_utilities::exp_list;
        using exp_utilities::to_exp_list_t;
        using meta_quote::unary;
        using meta_objects::meta_ret_object;
        using meta_quote::binary;
        using meta_objects::meta_object;
        using exp_utilities::get_type;


        namespace meta_basic_istream_detail {
            template<class this_list> using dec_f = typename this_list::pop_front;
            template<class this_list> using pop_f = typename this_list::front;

            template<class TL>
            using meta_basic_istream = meta_ret_object<to_exp_list_t<TL>, unary<dec_f>, unary<pop_f>>;
        }

        namespace meta_basic_ostream_detail {


            template<class none_list_type, class T>
            struct add_impl { using type = exp_utilities::literal_types::no_exist_type; };

            template<template<class...> class L, class T, class ...Tys>
            struct add_impl<L<Tys...>, T> {
                using type = L<Tys..., T>;
            };


            template<class this_list, class T>
            using add_f = typename add_impl<this_list, T>::type;

            template<class TL = exp_list<>>
            using meta_basic_ostream = meta_object<TL, binary<add_f>>;
        }

        namespace meta_transform_istream_detail {
            using meta_basic_istream_detail::dec_f;
            using meta_basic_istream_detail::pop_f;

            template<class meta_function_type, class this_list>
            using pop_transform_f = meta_invoke<meta_function_type, pop_f<this_list>>;

            template<class TL, class meta_function_type>
            using meta_basic_transform_istream = meta_ret_object<to_exp_list_t<TL>, unary<dec_f>, meta_quote::bind_binary<pop_transform_f, meta_function_type>>;
        }

        namespace meta_transform_ostream_detail {
            using meta_basic_ostream_detail::add_f;

            template<class meta_function_type>
            struct add_transform_f {
                template<class this_list, class from_ins>
                using apply = add_f<this_list, meta_invoke<meta_function_type, this_list, from_ins>>;
            };

            template<class TL, class meta_function_type>
            using meta_basic_transform_ostream = meta_object<TL, add_transform_f<meta_function_type>>;
        }

        namespace meta_join_ostream_detail {
            using meta_basic_ostream_detail::add_f;

            template<class T>
            struct add_to_this_list {
                template<class this_list>
                using apply = add_f<this_list, T>;
            };

            template<class this_list, class T>
            struct auto_join_f {
                using type = add_f<this_list, T>;
            };

            template<class this_list, template<class...>class L, class ...Tys>
            struct auto_join_f<this_list, L<Tys...>> {
                using type = meta_fold<this_list, add_to_this_list<Tys>::template apply...>;
            };
            struct auto_join {
                template<class this_list, class T>
                using apply = typename auto_join_f<this_list, T>::type;
            };

            template<class TL>
            using join_ostream = meta_object<TL, auto_join>;
        }

        namespace meta_filter_ostream_detail {
            using meta_basic_ostream_detail::add_f;

            template<class meta_function_type>
            struct add_filter_f {
                template<class this_list, class from_ins>
                using apply = std::conditional_t<
                    meta_invoke<meta_function_type, this_list, from_ins>::value,
                    add_f<this_list, from_ins>,
                    this_list
                >;
            };

            template<class TL, class filter>
            using filter_ostream = meta_object<TL, add_filter_f<filter>>;
        }

        namespace meta_index_istream_detail {
            using exp_utilities::Idx;
            using exp_utilities::inc_idx_t;
            using exp_utilities::to_meta_array_t;
            using exp_utilities::exp_select;
            template<std::size_t start> using my_counter = exp_list<Idx<start>>;

            template<class this_list>
            using make_inc_type = exp_list<inc_idx_t<exp_select<0, this_list>>>;

            template<class this_list>
            using ret_index = exp_select<0, this_list>;

            template<std::size_t start>
            using index_istream = meta_ret_object<my_counter<start>, meta_quote::unary<make_inc_type>, meta_quote::unary<ret_index>>;

        }

        namespace meta_replace_able_ostream_detail {
            using exp_utilities::literal_types::no_exist_type;
            template<class this_obj, class T>
            using replace_this = T;

            using replace_able_ostream = meta_object<no_exist_type, meta_quote::binary<replace_this>>;
        }
        namespace meta_transform_iterator_detail {
            using exp_utilities::literal_types::no_exist_type;
            template<class F>
            struct transform_iterator_f {
                template<class this_obj, class from_ins>
                    using apply = meta_invoke<F, this_obj, from_ins>;
            };

            template<class F> requires (meta_objects::initialize_details::has_initializer<F>)
            struct transform_iterator_f<F> {
                template<class this_obj, class from_ins> 
                using initialize = meta_objects::initialize_details::initialize<F, this_obj, from_ins>;
                template<class this_obj, class from_ins>
                using apply = meta_invoke<F, this_obj, from_ins>;
            };

            template<class F, typename T>
            using transform_iterator = meta_object<T, transform_iterator_f<F>>;
        }

        namespace meta_self_repeat_ostream_detail {
            template<class this_obj>
            using ret_self = typename this_obj::template at<0>;

            template<class this_obj>
            using do_nothing = this_obj;

            template<class T>
            using self_ostream = meta_ret_object<exp_list<T>, meta_quote::unary<do_nothing>, meta_quote::unary<ret_self>>;
        }
        namespace meta_forward_ostream_details {
            using meta_basic_ostream_detail::meta_basic_ostream;
            using exp_utilities::exp_select;
            using exp_utilities::exp_size;
            template<std::size_t L, class TL, class F>
            struct forward_ostream_f_impl {
                //get current index according to the length of this_list, and apply F to the current index and the corresponding type in TL
                template<class this_list, class Arg>
                struct apply {
                    static constexpr std::size_t current_index = to_exp_list_t<this_list>::length;
                    using result = meta_invoke<F, exp_select<current_index, TL>, Arg>;
                    using type = typename meta_invoke<meta_basic_ostream<this_list>, result>::type;
                };
                //some istream don't empty itself, it cause the stream to invoke the length 0 istream to cause en error, so when the length of this_list is greater than or equal to L, it means the stream is empty, just return this_list without applying F
                template<class this_list, class Arg> requires (to_exp_list_t<this_list>::length >= L)
                    struct apply<this_list, Arg> {
                    using type = this_list;
                };
            };
            template<class TL, class F>
            struct forward_ostream_f {
                template <class this_list, class Arg>
                using apply = typename forward_ostream_f_impl<exp_size<TL>, TL, F>::template apply<this_list, Arg>::type;
            };

            template<class TL, class F>
            using meta_forward_ostream = meta_object<exp_list<>, forward_ostream_f<TL, F>>;
        }
        namespace timer_condition_details {
            struct timer_receiver {
                template<class timer, class...> struct apply :timer {};
            };
        }
        using meta_timer_cond_o = meta_object<void, timer_condition_details::timer_receiver>;
    }
    using meta_objects::meta_object;
    using meta_objects::meta_ret_object;
    using meta_objects::meta_timer_object;
    using meta_objects::meta_empty_o;
    using meta_objects::meta_timer_object_details::meta_always_continue;
    using meta_objects::invoke_object_if;
    using meta_loop::meta_looper;
    using meta_loop::meta_looper_t;
    using namespace exp_utilities;

    namespace io_stream_transform_details
    {
        namespace io_stream_traits {
            template<class T> struct is_meta_object : std::false_type {};

            template<class OBJ, class F>
            struct is_meta_object<meta_object<OBJ, F>> : std::true_type {};
            template<class OBJ, class F, class Ret>
            struct is_meta_object<meta_ret_object<OBJ, F, Ret>> : std::true_type {};

            template<class T> constexpr bool is_meta_object_v = is_meta_object<T>::value;

            template<class T> struct is_meta_object_ret : std::false_type {};

            template<class OBJ, class F, class Ret>
            struct is_meta_object_ret<meta_ret_object<OBJ, F, Ret>> : std::true_type {};
            template<class T> constexpr bool is_meta_object_ret_v = is_meta_object_ret<T>::value;

            //the meta_istream_type must be a meta_ret_object
            template<class T>
            concept meta_istream_t = is_meta_object_ret_v<T>;

            //the meta_ostream_type can be any type of meta_object
            template<class T>
            concept meta_ostream_t = is_meta_object_v<T>;

        }


        #include<typeinfo>
        template<io_stream_traits::meta_ostream_t To, io_stream_traits::meta_istream_t From> struct meta_stream
        {
            using from = From;
            using to = To;
            using from_t = typename from::type;
            using to_t = typename to::type;
            using cache = typename From::ret;

            template<class ...Arg>
            using invoke_to = meta_stream<meta_invoke<To, Arg...>, from>;

            constexpr std::type_info const& target_type()const {
                return typeid(to_t);
            }
            constexpr to_t object()const {
                return to_t{};
            }
            consteval auto value()const {
                if constexpr (has_value<to_t>) {
                    return to_t::value;

                }
                else {
                    return literal_types::no_exist_type{};
                }
            }
            consteval std::size_t left()const {
                return exp_size<from_t>;
            }
        };


      template<class meta_stream_t>
    struct meta_stream_update {
        using type = meta_stream_t;
    };

    template<class To, class From> requires (std::is_same_v<typename From::type, literal_types::end_of_list<typename From::type>>)
    struct meta_stream_update<meta_stream<To, From>> {
        using type = meta_stream<To, From>;
    };

    template<class To, class From> requires (!length_equal<typename From::type, 0>)
    struct meta_stream_update<meta_stream<To, From>> {
        using type = meta_stream<meta_object_invoke<To, From>, meta_invoke<From>>;
    };

    struct meta_stream_f
    {
        template<class mo_stream, class...>
        using apply = typename meta_stream_update<mo_stream>::type;
    };

    struct meta_stream_always_continue {
    template<class in_stream_t>
    struct apply {
        static constexpr bool value = std::is_same_v<typename in_stream_t::from::ret, literal_types::end_of_list<typename in_stream_t::from::type>>;
        };
     };
}

    //convert meta_stream into a timed meta_object
    template<std::size_t Transfer_Length,
        io_stream_transform_details::io_stream_traits::meta_ostream_t To,
        io_stream_transform_details::io_stream_traits::meta_istream_t From,
        class break_f = meta_always_continue>
    using meta_stream_o = meta_timer_object<
        Transfer_Length,
        io_stream_transform_details::meta_stream<To, From>,
        io_stream_transform_details::meta_stream_f,
        break_f
    >;

    //create a meta_timer_object that transfers all elements from From to To
    template<io_stream_transform_details::io_stream_traits::meta_ostream_t To,
        io_stream_transform_details::io_stream_traits::meta_istream_t From,
        class break_f = meta_always_continue>
    using meta_all_transfer_o = meta_stream_o<exp_size<typename From::type>, To, From, break_f>;


    template<io_stream_transform_details::io_stream_traits::meta_ostream_t To,
        io_stream_transform_details::io_stream_traits::meta_istream_t From,
        class break_f = meta_always_continue>
    using meta_all_transfer = meta_looper_t<
        io_stream_transform_details::meta_timer_cond_o,
        meta_all_transfer_o<To, From, break_f>,
        meta_empty_o
    >;

    template<std::size_t N,
        io_stream_transform_details::io_stream_traits::meta_ostream_t To,
        io_stream_transform_details::io_stream_traits::meta_istream_t From,
        class break_f = meta_always_continue
    >
    using transfer = meta_looper_t<
        io_stream_transform_details::meta_timer_cond_o,
        meta_stream_o<N, To, From, break_f>,
        meta_empty_o
    >;

    //warning: this may cause infinite loop if break condition was never met
    template<
        io_stream_transform_details::io_stream_traits::meta_ostream_t To,
        io_stream_transform_details::io_stream_traits::meta_istream_t From,
        class break_f
    >
    using transfer_until = meta_looper_t<
        io_stream_transform_details::meta_timer_cond_o,
        meta_stream_o<static_cast<std::size_t>(-1), To, From, break_f>,
        meta_empty_o
    >;

    //create a meta_looper 
    //in meta_stream, Generator is not needed, so we use meta_empty_o here
    template<class stream_t>
    using transfer_stream = meta_looper<
        io_stream_transform_details::meta_timer_cond_o,
        stream_t,
        meta_empty_o
    >;

    template<
        io_stream_transform_details::io_stream_traits::meta_ostream_t To,
        io_stream_transform_details::io_stream_traits::meta_istream_t From,
        class break_f = meta_always_continue
    >
    using meta_for = meta_invoke<transfer_stream<
        meta_all_transfer_o<To, From, break_f>
        >>;

    template<
        std::size_t times,
        io_stream_transform_details::io_stream_traits::meta_ostream_t To,
        io_stream_transform_details::io_stream_traits::meta_istream_t From,
        class break_f = meta_always_continue
    >
    using meta_while = meta_invoke<transfer_stream<
        meta_stream_o<times, To, From, break_f>
        >>;

    //common basic io streams
    template<class type_list>
    using meta_istream = io_stream_transform_details::meta_basic_istream_detail::meta_basic_istream<type_list>;

    template<class ...Tys>
    using meta_istream_list = meta_istream<exp_list<Tys...>>;

    template<class type_list>
    using meta_ostream = io_stream_transform_details::meta_basic_ostream_detail::meta_basic_ostream<type_list>;

    template<class type_list, class meta_function_type>
    using meta_transform_istream = io_stream_transform_details::meta_transform_istream_detail::meta_basic_transform_istream<type_list, meta_function_type>;

    template<class type_list, class meta_function_type>
    using meta_transform_ostream = io_stream_transform_details::meta_transform_ostream_detail::meta_basic_transform_ostream<type_list, meta_function_type>;

    template<class type_list, class meta_function_type>
    using meta_filter_ostream = io_stream_transform_details::meta_filter_ostream_detail::filter_ostream<type_list, meta_function_type>;
    //generate an index type for each element in the istream, starting from 'start'
    //note: this istream never ends
    template<std::size_t start>
    using meta_index_istream = io_stream_transform_details::meta_index_istream_detail::index_istream<start>;

    template<std::size_t start, std::size_t count>
    using meta_count = typename transfer<count, meta_ostream<exp_list<>>, meta_index_istream<start>>::to::type;

    template<std::size_t start, std::size_t count>
    using meta_count_istream = meta_istream<meta_count<start, count>>;

    using meta_iterator = io_stream_transform_details::meta_replace_able_ostream_detail::replace_able_ostream;

    //fold operation, but with stream protocol control, can be blocked
    template<class F, typename init = literal_types::no_exist_type>
    using meta_transform_iterator = io_stream_transform_details::meta_transform_iterator_detail::transform_iterator<F, init>;

    //generate an specific type T for each element in the istream
    //note: this istream never ends
    template<class T>
    using meta_repeat_istream = io_stream_transform_details::meta_self_repeat_ostream_detail::self_ostream<T>;

    //if from the istream reads a typelist, it joins the typelist into the typelist in ostream
    template<class TL>
    using meta_jostream = io_stream_transform_details::meta_join_ostream_detail::join_ostream<TL>;

    template<class T1, class T2>
    using default_combine = exp_list<T1, T2>;

    //in each iteration, F<this_list::at<I>, from_is>, F is default combine
    template<class TL, class F = meta_quote::binary<default_combine>>
    using meta_forward_ostream = io_stream_transform_details::meta_forward_ostream_details::meta_forward_ostream<TL, F>;

    //meta character istream, '0' terminated
    template<static_str str>
    using meta_char_istream = meta_istream<
        typename transfer<
        exp_size<str_to_list<str>> -1,
        meta_ostream<exp_list<>>,
        meta_istream<str_to_list<str>>
        >::to::type
    >;


    //transfer protocols
    namespace protocols {
        //quote meta unary function to handle this_type prefix only
        template<class meta_function_type>
        using only_this = io_stream_transform_details::transfer_protocols::details::this_policy_type<meta_function_type>;
        //quote meta unary function to handle ret from istream only
        template<class meta_function_type>
        using only_arg = io_stream_transform_details::transfer_protocols::details::arg_policy_type<meta_function_type>;
        //quote meta binary function to handle to::type from ostream and ret from istream
        template<class meta_function_type>
        using stream_to_unref = io_stream_transform_details::transfer_protocols::details::stream_to_policy_type<meta_function_type>;
        //quote meta unary function to handle ret from istream only
        template<class meta_function_type>
        using only_stream_to_unref = io_stream_transform_details::transfer_protocols::details::stream_to_this_policy_type<meta_function_type>;
        //quote meta unary function to handle ret from istream only
        template<class meta_function_type>
        using only_stream_from_unref = io_stream_transform_details::transfer_protocols::details::stream_from_only_policy_type<meta_function_type>;
        //quote meta binary function to handle cache from istream and ret from istream
        template<class meta_function_type>
        using stream_cache_unref = io_stream_transform_details::transfer_protocols::details::stream_cache_policy_type<meta_function_type>;
        //quote meta unary function to handle cache from istream only
        template<class meta_function_type>
        using only_stream_cache_unref = io_stream_transform_details::transfer_protocols::details::stream_cache_this_policy_type<meta_function_type>;

        template<class meta_stream_type>
        using stream_to_t = typename meta_stream_type::to::type;

        template<class meta_stream_type>
        using stream_from_t = typename meta_stream_type::from::type;

        template<class meta_stream_type>
        using stream_cache_t = typename meta_stream_type::cache;

        template<class type_list>
        using forward_last = exp_select<max_index<type_list>, type_list>;

        struct meta_stream_skip_signal {};

        template<class meta_stream_type>
        using wait_for_end = std::conditional_t<!length_equal<typename meta_stream_type::from::type, 0>, meta_stream_skip_signal, meta_stream_type>;
    }


    //helper to call a function with a protocol folded stream
    namespace protocol_auto_unref_details {
        //if has no protocol stream_to_t/stream_from_t/stream_cache_t, default adding protocol stream_to_t.
        //detect protocols
        template<template<class> class P>
        struct protocol_container {
             template<class T>
             using transform = P<T>;
        };

        template<template<class> class P1, template<class> class P2>
        struct template_equal {
            static constexpr bool value = std::is_same_v<protocol_container<P1>, protocol_container<P2>>;
        };

        template<template<class> class P>
        struct template_equal_with {
            template<class T>
            struct apply_impl :std::false_type {};
            template<template<class> class anotherP>
            struct apply_impl<protocol_container<anotherP>> :template_equal<P, anotherP> {};

            template<class T>
            using apply = apply_impl<T>;
        };

        template<template<class> class P, template<class> class ...PS>
        using has_protocol = meta_all_transfer<
            meta_filter_ostream<exp_list<>, protocols::only_arg<template_equal_with<P>>>,
            meta_istream_list<protocol_container<PS>...>
        >::to_t;

        template<template<class> class P, template<class> class ...PS>
        constexpr bool has_no_protocol_v = length_equal<has_protocol<P, PS...>, 0>;

        using protocols::meta_stream_skip_signal;

        struct fold_transition {
            template<class this_obj, class from_ins>
            struct impl {
                using type = typename from_ins::template transform<this_obj>;
            };
            template<class this_obj, class from_ins>
            using apply = typename impl<this_obj, from_ins>::type;
        };

        using break_if_result_is_skip_signal = protocols::only_stream_to_unref<meta_quote::bind_binary<std::is_same, meta_stream_skip_signal>>;

        template<class in_stream_t, template<class> class ...PS>
        using fold_result = meta_all_transfer<
            meta_transform_iterator<fold_transition, in_stream_t>,
            meta_istream_list<protocol_container<PS>...>,
            break_if_result_is_skip_signal>::to_t;

    }
    template<template<class> class ...PS>
    auto protocol_call(auto&& f) {
        using protocol_auto_unref_details::has_no_protocol_v;
        using protocol_auto_unref_details::fold_result;
        if constexpr (
            has_no_protocol_v<protocols::stream_to_t, PS...> &&
            has_no_protocol_v<protocols::stream_from_t, PS...> &&
            has_no_protocol_v<protocols::stream_cache_t, PS...>)
        {
            return[&f]<class in_stream_t, class ...Args>(in_stream_t, Args&&...args) {
                using fold_result_t = fold_result<in_stream_t, protocols::stream_to_t, PS...>;
                if constexpr (!std::is_same_v<fold_result_t, protocols::meta_stream_skip_signal>)
                std::invoke(f, fold_result_t{}, std::forward<Args>(args)...);
            };
        }
        else
        {
            return[&f]<class in_stream_t, class ...Args>(in_stream_t, Args&&...args) {
                using fold_result_t = fold_result<in_stream_t, PS...>;
                if constexpr(!std::is_same_v<fold_result_t, protocols::meta_stream_skip_signal>)
                    std::invoke(f, fold_result_t{}, std::forward<Args>(args)...);
            };
        }
    }
 namespace meta_aligned_iterator_details {
     template<class from_ins>
     struct seek_to_v {
         template<class addr_type>
         struct apply {
             static constexpr bool value = !static_cast<bool>(addr_type::value % alignof(from_ins));
         };
     };
     template<std::size_t start, class from_ins>
     struct seek_to {
         using seek_t = typename transfer_until<
             meta_iterator::template meta_set<std::integral_constant<std::size_t, start>>,
             meta_index_istream<start>,
             protocols::only_stream_to_unref<seek_to_v<from_ins>>
         >::to_t;
         using advance_t = std::integral_constant<std::size_t, seek_t::value + sizeof(from_ins)>;
         using type = from_ins;
         static constexpr std::size_t value = seek_t::value;
         void emplace(std::byte* ptr, type const& val) {
             new(ptr + value) type{ val };
         }
         void destroy(std::byte* ptr) {
             std::destroy_at(reinterpret_cast<type*>(ptr + value));
         }
         type& get(std::byte* ptr) {
             return *reinterpret_cast<type*>(ptr + value);
         }
         type const& c_get(std::byte* ptr) {
             return *reinterpret_cast<type*>(ptr + value);
         }
     };
     struct advance_f {
         template<class this_seek, class from_ins>
         using apply = seek_to<this_seek::advance_t::value, from_ins>;
         template<class this_seek, class from_ins>
         using initialize = seek_to<0, from_ins>;
     };
 }
 /// <summary>
 /// for meta_aligned_iterator, it is a meta_ostream_t that can seek to an aligned address for a specific type in a byte stream, and it can also advance to the next aligned address for the next type.  
 /// </summary>
 using meta_aligned_iterator = meta_object_init<meta_aligned_iterator_details::advance_f>;
namespace meta_pipe_node_details {
    struct advance_node {
        template<class this_pipe>
        struct advance_impl {
            using stream_invoke = transfer<1, typename this_pipe::to, typename this_pipe::from>;
            using from = typename stream_invoke::from;
            using to = typename stream_invoke::to;
        };
        template<class this_pipe>
        using apply = io_stream_transform_details::meta_stream<typename advance_impl<this_pipe>::to, typename advance_impl<this_pipe>::from>;
    };

    template<template<class> class...ps>
    struct ret_from_node {
        template<class this_pipe>
        using apply = meta_fold<this_pipe, protocols::stream_to_t, ps...>;
    };

    template<
        io_stream_transform_details::io_stream_traits::meta_istream_t is,
        io_stream_transform_details::io_stream_traits::meta_ostream_t os,
        template<class> class...ps>
    using stream_istream = meta_ret_object<
        transfer<1, os, is>,
        advance_node,
        ret_from_node<ps...>
    >;

    template<class this_pipe>
    using skip_node = io_stream_transform_details::meta_stream<
        typename this_pipe::to,
        meta_invoke<invoke_if<(exp_size<typename this_pipe::from::type> > 0)>,typename this_pipe::from>
    >;

   
    template<class meta_function_type, typename reset_t>
    struct skip_advance_node {
        template<class this_pipe>
        struct advance_impl {
            using stream_invoke = meta_all_transfer<
                std::conditional_t<!std::is_same_v<reset_t, void>,
                typename this_pipe::to::template meta_set<reset_t>, 
                typename this_pipe::to
                >,
                typename this_pipe::from, meta_function_type>;
            using from = typename skip_node<stream_invoke>::from;
            using to = typename skip_node<stream_invoke>::to;
        };
        template<class this_pipe>
        using apply = io_stream_transform_details::meta_stream<typename advance_impl<this_pipe>::to, typename advance_impl<this_pipe>::from>;
    };

    template<
        io_stream_transform_details::io_stream_traits::meta_istream_t is,
        io_stream_transform_details::io_stream_traits::meta_ostream_t os,
        class break_f, class reset_t,
        template<class> class...ps>
    using skip_stream_istream = meta_ret_object<
        skip_node<meta_all_transfer<os, is, break_f>>,
        skip_advance_node<break_f, reset_t>,
        ret_from_node<ps...>
    >;

    template<
        std::size_t N,
        io_stream_transform_details::io_stream_traits::meta_istream_t is,
        io_stream_transform_details::io_stream_traits::meta_ostream_t os,
        template<class> class...ps/*protocols*/>
    struct transfer_pipe {
        template<io_stream_transform_details::io_stream_traits::meta_ostream_t another_os,
            template<class> class...other_ps>
        using all_to = transfer_pipe<
            exp_size<typename is::type>, stream_istream<is, os, ps...>, another_os, other_ps...
        >;
        template<io_stream_transform_details::io_stream_traits::meta_ostream_t another_os,
            template<class> class...other_ps>
        using each_to = transfer_pipe<
            exp_size<typename is::type>, stream_istream<is, os, ps...>, another_os, protocols::forward_last, other_ps...
        >;

        template<
            std::size_t Nc, 
            io_stream_transform_details::io_stream_traits::meta_ostream_t another_os,
            template<class> class...other_ps>
        using to = transfer_pipe<
            Nc, stream_istream<is, os, ps...>, another_os, other_ps...
        >;

        template<
            std::size_t Nc,
            io_stream_transform_details::io_stream_traits::meta_ostream_t another_os,
            class break_f, typename reset_t,
            template<class> class...other_ps>
        using skip_to = transfer_pipe<
            Nc, skip_stream_istream<is, os, break_f, reset_t, ps...>, another_os, other_ps...
        >;
        using from = stream_istream<is, os, ps...>;
        using transfer = meta_ios::transfer<N, os, is>;
        template<class meta_function_type>
        using skip = meta_ios::meta_all_transfer<os, is, meta_function_type>;
    };

}

    namespace pipe {
        using meta_pipe_node_details::transfer_pipe;
        using meta_pipe_node_details::skip_stream_istream;
        using meta_objects::meta_timer_object_details::meta_always_continue;
        using io_stream_transform_details::io_stream_traits::meta_istream_t;
        using io_stream_transform_details::io_stream_traits::meta_ostream_t;
        template<meta_istream_t is>
        struct transfer {
        template<meta_ostream_t another_os,
            template<class> class...other_ps>
        using all_to = transfer_pipe<
            exp_size<typename is::type>, is, another_os, other_ps...
        >;
        template<meta_ostream_t another_os,
            template<class> class...other_ps>
        using each_to = transfer_pipe<
            exp_size<typename is::type>, is, another_os, other_ps...,protocols::forward_last
        >;

        template<std::size_t Nc, meta_ostream_t another_os,
            template<class> class...other_ps>
        using to = transfer_pipe<
            Nc, is, another_os, other_ps...
        >;
        
        template<meta_ostream_t another_os, class break_f, class reset_t,
            template<class> class...other_ps>
        using skip_to = pipe::transfer<
            skip_stream_istream<is, another_os, break_f, reset_t, other_ps...>
        >;

    };
    
    }

}


