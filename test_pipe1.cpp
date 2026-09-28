#include <iostream>
#include <type_traits>
#include <cstdint>
#include <tuple>
#include <string>

#include "meta_stream.hpp"
using namespace meta_ios;
using namespace exp_utilities;
using namespace meta_objects;

int main() {
  // Build a passthrough pipe; .transfer::from is the self-terminating istream
  using P1 = meta_pipe<meta_istream_list<int, double, char>>::
      all_to<meta_iterator>::transfer::from;

  std::cout << "=== passthrough pipe, transfer::from driven directly ==="
            << std::endl;
  int count = 0;
  meta_transfer_until<meta_iterator, P1>::for_each([&](auto s) {
    std::cout << s.target_type().name() << std::endl;
    ++count;
  });
  std::cout << "call count: " << count << " (expect 3)" << std::endl;

  return 0;
}
