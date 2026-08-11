#include"meta_stream.hpp"
#include<iostream>

//demonstrate of meta_stream::pipe
using namespace meta_ios;


template<class my_list>
void print_list() {
    static_assert(exp_size<my_list>, "empty list or non list type");
    std::cout << "counts = " << exp_size<my_list> << std::endl;
    std::cout << '<';
    meta_for<
        meta_iterator,
        meta_istream<my_list>
    >::for_each(
        [](auto t_stream) {
            std::cout << typeid(typename decltype(t_stream)::to::type).name();
            if constexpr (t_stream.length()) std::cout << ',';
        }
    );
    std::cout << '>' << std::endl;
}
//define replace algo
template<std::size_t I, class T>
struct replace_at {
    template<class this_list, class from_ins>
    using apply = std::conditional_t<
        exp_size<this_list> == I,
        typename this_list::template push_back<T>,
        typename this_list::template push_back<from_ins>
    >;
};
//define replace meta_os
template<std::size_t I, class T>
using replace_os = meta_object<exp_list<>, replace_at<I, T>>;

//meta println:
template<class text_list>
using zero_end_fix =typename meta_invoke<invoke_if<text_list::back::value != '\0'>, meta_ostream<text_list>, exp_char<'\0'>>::type;

template<exp_utilities::static_str str>
std::ostream& println(std::ostream& os, auto const& ...args) {
    if constexpr (
        meta_all_transfer<
        meta_filter_ostream<exp_list<>, protocols::only_arg<value_is<'#'>>>,
        meta_char_istream<str>
        >::to_t::length == 0
        ) {
        os << str;
        return os;
    }else
    {
        using text = meta_char_istream<str>;
        constexpr auto max_idx = exp_utilities::str_to_list<str>::length - 1,//last is '\0'
            transfer_times = exp_utilities::str_to_list<str>::template at<max_idx>::value == '#' ? sizeof...(args) : sizeof...(args) + 1;

        using on_skip_reset_to_empty = exp_list<>;
        using detect_hash_and_zero_in_cache = protocols::only_stream_cache_unref<value_is<'#'>::OR<value_is<'\0'>>>;

        using text_list_stream = pipe::transfer<text>
            ::template skip_to<meta_ostream<exp_list<>>,detect_hash_and_zero_in_cache,on_skip_reset_to_empty>
            ::template to<transfer_times, meta_ostream<exp_list<>>>;
        meta_for<
            meta_iterator,
            meta_istream<typename text_list_stream::transfer::to_t>
        >::for_each_forward(protocol_call<zero_end_fix, to_exp_char_t>([&os](auto str, auto const v) { os << str << v; }), args..., '\n');
        return os;
    }
}
template<std::size_t I, class indx>
struct tp_i_node {
    static constexpr auto next_count = I + exp_size<indx>;
    using index_map = typename meta_all_transfer<
        meta_forward_ostream<meta_count<I, exp_size<indx>>>,
        meta_istream<indx>
    >::to_t;
};
//support initialize template
struct make_node{
    template<class this_node,class indx>
    using apply = tp_i_node<this_node::next_count, indx>;
    template<class this_, class indx>
    using initialize = tp_i_node<0, indx>;
};

using tp_indx_ini_os = meta_object_init<make_node>;

template<class tp_i_n>
using get_map = typename tp_i_n::index_map;
template<class ...TP>
auto my_tuple_cat(TP const&... tps) {
    using cat_tuple_t = meta_all_transfer<
        meta_jostream<exp_list<>>,
        meta_istream_list<TP...>
    >::to_t::template to<std::tuple>;

    using index_map_t = typename pipe::transfer<
        meta_istream_list<meta_count<0, exp_size<TP>>...>
    >::template all_to<tp_indx_ini_os>
        ::template each_to<meta_transform_ostream<exp_list<>, protocols::only_arg<meta_quote::unary<get_map>>>>;

    cat_tuple_t cat_tp{};
    auto assign = [&cat_tp]<class inx>(inx, auto const& tp) {
        std::get<inx::at<0>::value>(cat_tp) = std::get<inx::at<1>::value>(tp);
    };
    meta_for<
        meta_iterator,
        meta_istream<typename index_map_t::transfer::to_t>
    >::for_each_forward(
        protocol_call([&assign]<class indx_map>(indx_map, auto const& tp) {
        meta_for<
            meta_iterator,
            meta_istream<indx_map>
        >::for_each(protocol_call(assign), tp);
    }), tps...
    );
    return cat_tp;
}
/*
* template<typename indx>
* using initial = initialize<make_node, tp_i_node<0, indx>>;
* initialize_object<tp_i_node<0, arg_1>, make_node>
*/


struct add_os_f{
    template<class this_obj, class T>
    using apply = typename meta_invoke<meta_ostream<this_obj>, T>::type;
    template<class this_, class T>
    using initialize = exp_list<T>;
};

using meta_n_ostream = meta_object_init<add_os_f>;

template<class L>
struct dec_is_f {
    template<class this_list>
    using apply = typename this_list::pop_front;

    template<class this_list>
    using initialize = to_exp_list_t<L>;
};

struct read_f {
    template<class this_list>
    using apply = typename this_list::front;
};

template<class L>
using meta_n_istream = meta_ret_object_init<dec_is_f<L>, read_f>;


int main() {
    auto tp = my_tuple_cat(std::tuple{ 1, 3.44 }, std::tuple{ 'k', .2f });
}