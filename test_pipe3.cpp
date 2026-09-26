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
  // Full chain: map (passthrough) -> collect (r_ostream) -> terminal istream
  using entry = pipe::transfer<meta_istream_list<int, double, char>>;
  using after_map = entry::all_to<meta_iterator>;
  using after_collect = after_map::all_to<r_ostream>;
  using final_is = after_collect::template result_istream<>::type;

  std::cout << "=== chained map -> collect, terminal istream ===" << std::endl;
  int count = 0;
  meta_transfer_until<meta_iterator, final_is>::for_each([&](auto s) {
    std::cout << s.target_type().name() << std::endl;
    ++count;
  });
  std::cout << "count: " << count << " (expect 3, order c,d,i)" << std::endl;

  return 0;
}
