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

int main(){
    using l = exp_list<int, double ,char>;
    meta_transfer_until<
        meta_iterator, 
        meta_istream<l>, 
        protocols::only_stream_cache_unref<meta_quote::bind_binary<std::is_same, char>>
    >::for_each([](auto stream){
        std::cout << stream.target_type().name() << std::endl;
        protocol_call<protocols::stream_cache_t>([](auto cache){
            std::cout << "cache = " << typeid(cache).name() << std::endl;
        })(stream);
    });

    //print_list<rl>();


}