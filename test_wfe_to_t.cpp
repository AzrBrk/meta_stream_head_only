#include <iostream>
#include <tuple>
#include <string>
#include <type_traits>
#include "meta_stream.hpp"
using namespace meta_ios;
using namespace exp_utilities;
using namespace meta_objects;

// wait_for_end_to_t fuses wait_for_end + stream_to_t: it directly yields the
// final to_t once exhausted, so to_meta_array_t then produces meta_array.
using run_t = meta_pipe<index_sequence_istream<3>>
    ::all_to<meta_rostream<>, protocols::wait_for_end_to_t, to_meta_array_t>
    ::run<meta_iterator>;
using arr = typename run_t::type::to::type;
static_assert(std::is_same_v<arr, meta_array<2, 1, 0>>);

int main() {
  std::tuple t{1, 3.33, std::string("test")};
  int calls = 0;
  run_t::for_each<protocols::stream_to_t>(
      [&]<std::size_t... I>(meta_array<I...>) {
        ++calls;
        ((std::cout << std::get<I>(t) << ' '), ...);
      });
  std::cout << "\ncalls=" << calls << " (expected 1)\n";
  return 0;
}
