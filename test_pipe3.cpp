#include <iostream>
#include <type_traits>
#include <cstdint>
#include <tuple>
#include <string>

#include "meta_stream.hpp"
using namespace meta_ios;
using namespace exp_utilities;
using namespace meta_objects;

// Collecting ostream: prepend each element, building a reversed list
struct r_ostream_f {
  template <class this_list, class from_istream_t>
  using apply = typename this_list::template push_front<from_istream_t>;
};
using r_ostream = meta_object<exp_list<>, r_ostream_f>;

int main() {
  // Stage 1: mapping pipe; .transfer::from is the self-terminating node stream
  using map_node =
      meta_pipe<meta_istream_list<int, double, char>>::
          all_to<meta_iterator>::transfer::from;

  // Stage 2: collect that node stream into r_ostream
  using collected = transfer_until<r_ostream, map_node>::to::type;
  static_assert(std::is_same_v<collected, exp_list<char, double, int>>,
                "r_ostream should collect into exp_list<char,double,int>");

  std::cout << "=== map -> collect, terminal list ===" << std::endl;
  using final_is = meta_istream<collected>;
  int count = 0;
  meta_transfer_until<meta_iterator, final_is>::for_each([&](auto s) {
    std::cout << s.target_type().name() << std::endl;
    ++count;
  });
  std::cout << "count: " << count << " (expect 3, order c,d,i)" << std::endl;

  return 0;
}
