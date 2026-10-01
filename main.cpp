#include <iostream>
#include <type_traits>
#include <cstdint>
#include <tuple>
#include <string>
#include <chrono>
#include <iomanip>

#include "meta_stream.hpp"
using namespace meta_ios;
using namespace exp_utilities;
using namespace meta_objects;

template<class T>
struct same_states_iterator: states_base, stream_op<deactivate(opSkip)>{
  using type = void;
  template<class this_type, class from_is>
  using pred = std::is_same<T, from_is>;
  template<class this_type, class from_is>
  using on_changed = from_is;
};

int main() {
  // meta_transfer_until<To, From, BF = meta_range_continue,
  //                     Observer = ostream_observer>
  // The last argument picks what counts as a change: ostream_observer (default),
  // istream_observer, cache_observer, or stream_observer (any of to/from/cache).
  auto type_mask = meta_transfer_until<
    meta_make_states<same_states_iterator<int>>,
    meta_istream_list<int,double,char>,
    meta_range_continue, stream_observer
  >::for_each(
    [](auto stream){
      std::cout << stream.index()
        << ": " << stream.target_type().name()
        << ": " << ((stream.flags()&~(0xFF00000000000000)) >> stream.index())
        <<std::endl;
        return stream.flags()&~(0xFF00000000000000);
    }
  );
}