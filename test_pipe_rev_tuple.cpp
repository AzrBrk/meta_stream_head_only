#include <iostream>
#include <tuple>
#include <string>
#include <type_traits>

#include "meta_stream.hpp"
using namespace meta_ios;
using namespace exp_utilities;
using namespace meta_objects;

int main() {
  std::tuple t{1, 3.33, std::string("test")};

  int calls = 0;
  // rostream collects; wait_for_end skips partial collections; once full,
  // stream_to_t + to_meta_array_t yield meta_array. run<meta_iterator> forwards,
  // and for_each<stream_to_t> folds the stage to to_t before the callable, so
  // the callable directly receives meta_array<I...>.
  meta_pipe<index_sequence_istream<3>>
      ::all_to<meta_rostream<>, protocols::wait_for_end,
               protocols::stream_to_t, to_meta_array_t>
      ::run<meta_iterator>::for_each<protocols::stream_to_t>(
          [&]<std::size_t... I>(meta_array<I...>) {
            ++calls;
            ((std::cout << std::get<I>(t) << ' '), ...);
          });

  std::cout << "\ncallback fired " << calls << " time(s) (expected 1)\n";
  std::cout << "(expected reverse order: test 3.33 1)\n";
  return 0;
}
