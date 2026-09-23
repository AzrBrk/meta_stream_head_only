#include<iostream>
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


int main(){
    using my_index_sequence = index::dec_index_istream<10>;

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