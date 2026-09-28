#include <iostream>
#include <tuple>
#include <string>
#include <type_traits>

#include "meta_stream.hpp"
using namespace meta_ios;
using namespace exp_utilities;
using namespace meta_objects;

int main() {
  auto tp = std::make_tuple(1, 'k', 2.33);

  // wait_for_end makes intermediate collections skip (not observed/forwarded);
  // once full, stream_to_t reads the reversed list and to_meta_array_t converts.
  using run_t = meta_pipe<index_sequence_istream<3>>::run_with<
      meta_rostream<>, protocols::wait_for_end, protocols::stream_to_t,
      to_meta_array_t>;

  int calls = 0;
  run_t::for_each([&](auto stream) {
    ++calls;
    // the driven stream's to_t is the folded result: meta_array<2,1,0>
    using arr = typename decltype(stream)::to_t;
    [&]<std::size_t... I>(meta_array<I...>) {
      ((std::cout << std::get<I>(tp) << ' '), ...);
    }(arr{});
  });

  std::cout << "\ncallback fired " << calls << " time(s) (expected 1)\n";
  std::cout << "(expected reverse order: 2.33 k 1)\n";
  return 0;
}
