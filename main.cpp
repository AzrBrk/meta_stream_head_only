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

#include"meta_objects.hpp"
#include "meta_states.hpp"
#include "meta_iostream.hpp"

using namespace meta_states;
using namespace meta_objects;
namespace mios = meta_iostream;

template<std::size_t N>
struct wait_til_last : meta_states::states_base
{
  template<this_ref flags, meta_object_t gen>
  struct pre_gen{
    static constexpr u64 value = []{
      if constexpr (flags::template get<u16, position_shift> == 0ul)
      {
        return set_timer<flags, 0, G_NOT_SKIP, N>::value;
      }
      else{
        return flags::value;
      }
    }();
  };
  template<this_ref flags, meta_object_t gen>
  struct post_gen{
    static constexpr u64 value = []{
      return exp_utilities::length_equal<gen, 0> ? flags::template opDeact<G_CONTINUE>::value : flags::value;
    }();
  };
};



int main(){
  auto ls = run<make_condition<wait_til_last<3>>, mios::meta_ostream<>, mios::meta_istream_list<int, double, char, char>>
  ::for_each([](auto states){
    std::cout << typeid(states.object()).name() << std::endl;
    return states.object();
  });
}

