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

#include "type_safe_aligned_iterator.hpp"

using namespace meta_ios;
using namespace exp_utilities;
using namespace meta_objects;
using namespace type_safe_aligned;

template<class MOT, class changed_pred, std::uint64_t flags, class ELse>
struct to_states_impl{
  using type = literal_types::no_exist_type;
};

template<class F, class Else>
struct states_function_wrapper{
  template<class this_obj, class from_is>
  using apply = meta_invoke<Else,  this_obj, from_is>;
  template<class this_obj, class from_is>
  using on_changed = meta_invoke<F, this_obj, from_is>;
};

template<class F, class OBJ, class change_pred, std::uint64_t flags, class Else>
struct to_states_impl<meta_object<OBJ, F>, change_pred, flags, Else>{
  using type = meta_states_object<OBJ, states_function_wrapper<F, Else>, change_pred, 0>;
};

template<class Mo, class change_pred, std::uint64_t flags, class Else = typename meta_objects::meta_empty_o::function>
using to_states = typename to_states_impl<Mo, change_pred, flags, Else>::type;

template<class T>
struct is_type{
  template<class U>
  using apply = std::is_same<T, U>;
};

int main(){
  using states_iter = to_states<meta_iterator, protocols::only_arg<is_type<int>>, (opSkip|opCallIs)>;
  using states_iter_callos_on = to_states<meta_iterator, protocols::only_arg<is_type<int>>, (opSkip|opCallIs|opCallOs)>;
  using states_iter_deactive_skip = to_states<meta_iterator, protocols::only_arg<is_type<int>>, (compose(deactivate(opSkip))|opCallIs|opCallOs)>;
  meta_ios::meta_transfer_until<states_iter, meta_istream_list<int, double, char, int, int>>
  ::for_each(
    [](auto stream){
      std::cout << stream.index() << ": " 
        << stream.target_type().name() << ":"
        << std::bitset<sizeof(std::uint64_t) * 8>(stream.flags())
        << std::endl;
    }
  );
  meta_ios::meta_transfer_until<states_iter_callos_on, meta_istream_list<int, double, char, int, int>>
  ::for_each(
    [](auto stream){
      std::cout << stream.index() << ": " 
        << stream.target_type().name() << ":"
        << std::bitset<sizeof(std::uint64_t) * 8>(stream.flags())
        << std::endl;
    }
  );
  meta_ios::meta_transfer_until<states_iter_deactive_skip, meta_istream_list<int, double, char, int, int>>
  ::for_each(
    [](auto stream){
      std::cout << stream.index() << ": " 
        << stream.target_type().name() << ":"
        << std::bitset<sizeof(std::uint64_t) * 8>(stream.flags())
        << std::endl;
    }
  );
}


