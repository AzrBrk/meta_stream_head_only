#include<algorithm>
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
        >::for_each_forward(protocol_call<zero_end_fix, to_exp_char_t>([&os](auto clip_str, auto const v) { os << clip_str << v; }), args..., '\n');
        return os;
    }
}

//tuple cat
template<std::size_t I, class indx>
struct tp_i_node {
    static constexpr auto next_count = I + exp_size<indx>;
    using index_map = typename meta_all_transfer<
        meta_forward_ostream<meta_count<I, exp_size<indx>>>,
        meta_istream<indx>
    >::to_t;
};

struct make_node {
    template<class this_, class indx>
    using initialize = tp_i_node<0, indx>;
    template<class this_node, class indx>
    using apply = tp_i_node<this_node::next_count, indx>;
};

using tp_indx_os = meta_object_init<make_node>;

template<class tp_i_n>
using get_map = typename tp_i_n::index_map;
template<class ...TP>
auto my_tuple_cat(TP const&... tps) {
    using cat_tuple_t = meta_all_transfer<
        meta_jostream<exp_list<>>,
        meta_istream_list<TP...>
    >::to_t::template to<std::tuple>;

    using index_map_t = typename pipe::transfer<meta_istream_list<meta_count<0, exp_size<TP>>...>>
        ::template all_to<tp_indx_os>
        ::template each_to<meta_transform_ostream<exp_list<>, protocols::only_arg<meta_quote::unary<get_map>>>>;

    cat_tuple_t cat_tp{};
    auto assign = [&cat_tp]<class inx>(inx, auto const& tp) {
        println<"<#,#>">(std::cout, inx::template at<0>::value, inx::template at<1>::value);
        std::get<inx::template at<0>::value>(cat_tp) = std::get<inx::template at<1>::value>(tp);
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

template<class from_ins>
struct seek_to_v {
    template<class addr_type>
    struct apply {
        static constexpr bool value = !static_cast<bool>(addr_type::value % alignof(from_ins));
    };
};
template<std::size_t start, class from_ins>
struct seek_to {
    using seek_t =typename transfer_until<
        meta_iterator::template meta_set<std::integral_constant<std::size_t, start>>, 
        meta_index_istream<start>, 
        protocols::only_stream_to_unref<seek_to_v<from_ins>>
    >::to_t;
    using advance_t = std::integral_constant<std::size_t, seek_t::value + sizeof(from_ins)>;
    using type = from_ins;
};
struct advance_f {
    template<class this_seek, class from_ins>
    using apply = seek_to<this_seek::advance_t::value, from_ins>;
    template<class this_seek, class from_ins>
    using initialize = seek_to<0, from_ins>;
};

using align_off_os = meta_object_init<advance_f>;
template<class ...Arg>
void store(auto * base_ptr, Arg const& ...args){
    using data_type_list = exp_list<std::remove_cvref_t<Arg>...>;
    meta_for<
        align_off_os,
        meta_istream<data_type_list>
    >::for_each_forward(
        protocol_call([&base_ptr]<class POF>(POF, auto const& val){
            new(base_ptr + POF::seek_t::value) typename POF::type{val};
        }),args...
    );
}
struct S{int a; int b; std::string s;};
int main() {
    using align_t = double;
    using l = exp_list<int, int, std::string, align_t>;
    using final_reach = pipe::transfer<meta_istream<l>>
    ::all_to<align_off_os>
    ::all_to<meta_iterator>::transfer::to_t;
    std::byte data[final_reach::seek_t::value]{};
    store(data, 10, 12, std::string("hello"));
    auto *ps = reinterpret_cast<S*>(data);

    std::cout << ps->a <<','<<ps->b<<','<<ps->s;
}
