#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <utility>
#include <vector>

#include "meta_stream.hpp"

using namespace meta_ios;
using namespace exp_utilities;
using namespace meta_objects;


using meta_objects_details::meta_uninitialized;
namespace select_types_o_detail{
  template<class idx_t, class T>
  struct select_o{
    using next_select_count = inc_idx_t<idx_t>;
    using type = T;
    using idx = idx_t;
  };
  struct inc_idx_f{
    template<class this_count, class ...Ty>
    struct apply_impl{
      using type = select_o<
                      typename this_count::next_select_count,
                      exp_select<this_count::idx::value, exp_list<Ty...>>
                   >;
    };
    template<class ...Ty>
    struct apply_impl<select_o<below_zero, meta_uninitialized>, Ty...>
    {
      using type = select_o<
                    inc_idx_t<below_zero>,
                    exp_select<0, exp_list<Ty...>>
                  >;

    };
    template<class this_count, class ...Ty>
    using apply = typename apply_impl<this_count, Ty...>::type;

  };
  struct ret_count_t{
    template<class this_count>
    using apply = typename this_count::type;
  };

}

using select_gen_o = meta_ret_object<
                      select_types_o_detail::select_o<below_zero, meta_uninitialized>,
                      select_types_o_detail::inc_idx_f,
                      select_types_o_detail::ret_count_t
                    >;

using meta_objects::to_timer;
using io_stream_transform_details::meta_timer_cond_o;

int main(){

  //select_gen_o
  using loop_t = meta_invoke<
    meta_looper<meta_timer_cond_o, to_timer<meta_iterator, 3>, select_gen_o>,
    int, double, char, long
  >;

  using T = loop_t::type;
  loop_t::for_each([](auto obj){

  });

}


