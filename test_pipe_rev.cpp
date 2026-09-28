#include <iostream>
#include <type_traits>
#include <cstddef>
#include <string>

#include "meta_stream.hpp"
using namespace meta_ios;
using namespace exp_utilities;
using namespace meta_objects;

using raw = meta_istream_list<int, std::string, char, std::string>;

// ===== ONE-SHOT pipe =====
// aligned stage, then rostream collects it; wait_for_end skips partial
// collections (not observed/forwarded) and stream_to_t reads the full reversed
// list once the container is full.
using run_t =
    meta_pipe<raw>::all_to<meta_aligned_iterator>::run_with<
        meta_rostream<>, protocols::wait_for_end, protocols::stream_to_t>;
// the driven to::type is the full reversed aligned list L
using L = typename run_t::type::to::type;

using a0 = meta_aligned_iterator_details::seek_to<0, int>;
using a8 = meta_aligned_iterator_details::seek_to<4, std::string>;
using a40 = meta_aligned_iterator_details::seek_to<40, char>;
using a48 = meta_aligned_iterator_details::seek_to<41, std::string>;

int main() {
  std::cout << "L == reversed exp_list<a48,a40,a8,a0>: "
            << std::is_same_v<L, exp_list<a48, a40, a8, a0>> << "\n";

  std::cout << "reverse-order offsets: ";
  meta_for<meta_iterator, meta_istream<L>>::for_each(
      [](auto s) { std::cout << s.value() << (s.left() ? ',' : '\n'); });
  return 0;
}
