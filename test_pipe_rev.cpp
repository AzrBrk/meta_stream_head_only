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
// aligned -> rostream collecting the full reversed list (wait_for_end)
using ro_node =
    meta_pipe<raw>
        ::all_to<meta_aligned_iterator>
        ::all_to<meta_rostream<>, protocols::wait_for_end,
                 protocols::stream_to_t>::from;
// ro_node yields: skip x3, reversed list L, eol

// drive collection lazily, then read the final reversed list
// transfer_until::to = final ostream (meta_iterator state); its ::type is L
using driven = transfer_until<meta_iterator, ro_node>::to;
using L = driven::type;

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
