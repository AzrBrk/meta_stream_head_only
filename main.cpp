#include<iostream>
#include<cassert>
#include"meta_stream.hpp"


using namespace meta_ios;
using namespace exp_utilities;

template<class L>
void print_list(){
    std::cout << "count = " << exp_size<L> << std::endl;
    meta_for<meta_iterator,
        meta_istream<L>>
        ::for_each(
            [](auto type_stream){
                if constexpr(has_value<decltype(type_stream)>)
                    std::cout << type_stream.value();
                else
                    std::cout << type_stream.target_type().name();
                if constexpr(type_stream.left())
                {
                    std::cout << ',';
                }
                else{
                    std::cout << std::endl;
                }
            }
        );
}

namespace index{
    template<std::size_t dec_index>
    struct index_counter:std::integral_constant<std::size_t,  dec_index>, end_of_stream<(dec_index == 0)>
    {
        static constexpr std::size_t length = dec_index;
        using dec_t = index_counter<dec_index -1>;
        template<std::size_t Len>
        using ret_t = index_counter<Len - dec_index>;
    };
    template<std::size_t Len>
    struct dec_index_f {
        template<class this_index, class...>
        using apply = typename this_index::dec_t;
    };
    template<std::size_t Len>
    struct ret_index {
        template<class this_index, class...>
        using apply = typename this_index::template ret_t<Len>;
    };
    template<std::size_t Len>
    using dec_index_istream = meta_ret_object<index_counter<Len>, dec_index_f<Len>, ret_index<Len>>;
}

//asumming provided skip pred
template<std::size_t start, std::size_t end = start>
struct in_ranged_of{
    struct on_left{
        template<class inx_type>
        struct apply{
            static constexpr bool value = inx_type::value < start;
        };
    };
    template<class inx_type>
    struct apply{
        static constexpr bool value = inx_type::value >= start && inx_type::value <= end;
    };
    struct on_right{
        template<class inx_type>
        struct apply{
            static constexpr bool value = inx_type::value > end;
        };
    };
};

using meta_objects::meta_states_object;

struct increment_state {
    template<class ThisObj, class FromIs>
    using apply = std::integral_constant<int, ThisObj::value + 1>;
};

struct state_changed {
    template<class ThisObj, class FromIs>
    using apply = std::true_type;
};

using state0 = meta_states_object<
    std::integral_constant<int, 0>,
    increment_state,
    state_changed>;
using state1 = state0::apply<meta_empty_o>;
static_assert(state1::type::value == 1);
static_assert(state1::at<0>());
using replaced_state = state1::meta_set<std::integral_constant<int, 42>>;
static_assert(replaced_state::type::value == 42);
static_assert(replaced_state::flags == state1::flags);
static_assert(replaced_state::size == state1::size);

template<class List, class T>
struct append_state_value;

template<template<class...> class L, class... Ts, class T>
struct append_state_value<L<Ts...>, T> {
    using type = L<Ts..., T>;
};

struct append_state {
    template<class ThisObj, class FromIs>
    using apply = typename append_state_value<ThisObj, FromIs>::type;
};

template<std::size_t Start, std::size_t End>
struct state_in_range {
    template<class ThisObj, class FromIs>
    using apply = std::bool_constant<
        (FromIs::value >= Start) && (FromIs::value <= End)>;
};

template<class Initial, class Changed_Pred>
using state_filter_ostream = meta_states_object<
    Initial,
    append_state,
    Changed_Pred>;

template<class State, std::size_t... I>
void print_state_changes(std::index_sequence<I...>) {
    ((std::cout << (State::template at<I>() ? '1' : '0')
                << (I + 1 == sizeof...(I) ? '\n' : ',')), ...);
}

int main(){
    using my_index_sequence = index::dec_index_istream<10>;

    using filtered_stream = meta_transfer_until<
        state_filter_ostream<exp_list<>, state_in_range<3, 5>>,
        my_index_sequence,
        meta_range_continue,
        observe_ostream
    >;
    using filtered = typename filtered_stream::type::to;
    static_assert(filtered::size == 10);
    static_assert(!filtered::at<0>());
    static_assert(!filtered::at<1>());
    static_assert(!filtered::at<2>());
    static_assert(filtered::at<3>());
    static_assert(filtered::at<4>());
    static_assert(filtered::at<5>());
    static_assert(!filtered::at<6>());
    static_assert(!filtered::at<7>());
    static_assert(!filtered::at<8>());
    static_assert(!filtered::at<9>());

    using filtered_values = typename filtered::type;
    static_assert(exp_size<filtered_values> == 10);
    static_assert(exp_select<0, filtered_values>::value == 0);
    static_assert(exp_select<3, filtered_values>::value == 3);
    static_assert(exp_select<5, filtered_values>::value == 5);
    static_assert(exp_select<9, filtered_values>::value == 9);

    std::size_t observed_calls = 0;
    filtered_stream::for_each([&observed_calls](auto) {
        ++observed_calls;
    });
    assert(observed_calls == 3);
    std::cout << "observe_ostream calls: " << observed_calls << std::endl;

    std::size_t valued_calls = 0;
    const auto final_size = filtered_stream::for_each([&valued_calls](auto stream) {
        ++valued_calls;
        return static_cast<int>(exp_size<typename decltype(stream)::to::type>);
    });
    static_assert(std::is_same_v<decltype(final_size), const int>);
    assert(valued_calls == 3);
    assert(final_size == 0);
    std::cout << "non-void calls: " << valued_calls
              << ", final size: " << final_size << std::endl;

    std::cout << "state changes:" << std::endl;
    print_state_changes<filtered>(std::make_index_sequence<filtered::size>{});
    std::cout << "state output:" << std::endl;
    print_list<filtered_values>();

    using skip_left = protocols::only_stream_cache_unref<in_ranged_of<3,5>::on_left>;
    using skip_middle = protocols::only_stream_cache_unref<in_ranged_of<3,5>>;
    using skip_right = protocols::only_stream_cache_unref<in_ranged_of<3,5>::on_right>;

    using left_skipped = transfer_until_skip<
        meta_ostream<exp_list<>>,
        my_index_sequence,
        skip_left
    >::to_t;
    using middle_skipped = transfer_until_skip<
        meta_ostream<exp_list<>>,
        my_index_sequence,
        skip_middle
    >::to_t;
    using right_skipped = transfer_until_skip<
        meta_ostream<exp_list<>>,
        my_index_sequence,
        skip_right
    >::to_t;

    static_assert(exp_size<left_skipped> == 7);
    static_assert(exp_size<middle_skipped> == 7);
    static_assert(exp_size<right_skipped> == 6);

    std::cout << "skip left:" << std::endl;
    print_list<left_skipped>();
    std::cout << "skip middle:" << std::endl;
    print_list<middle_skipped>();
    std::cout << "skip right:" << std::endl;
    print_list<right_skipped>();
}