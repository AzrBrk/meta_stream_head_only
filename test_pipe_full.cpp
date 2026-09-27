#include <iostream>
#include <type_traits>
#include <cstddef>
#include <string>

#include "meta_stream.hpp"
using namespace meta_ios;
using namespace exp_utilities;
using namespace meta_objects;

using raw = meta_istream_list<int, std::string, char, std::string>;

// F: TL carries the index (Idx<k>); the arg is rostream's current reverse
// list. Read its front = the original-order element at position k.
struct get_front_f {
  template <class Idx_k, class CurrentRevList>
  using apply = typename CurrentRevList::front;
};

// ===== ONE-SHOT pipe: composed all at once, nothing flows yet =====
using pipe_t =
    meta_pipe<raw>
        ::all_to<meta_aligned_iterator>
        ::all_to<meta_rostream<>>
        ::all_to<meta_forward_ostream<meta_count<0, 4>, get_front_f>>;

// terminal node (unflowed); read its ret positions for verification
using node = pipe_t::from;
using S0 = node;
using S1 = meta_invoke<S0>;
using S2 = meta_invoke<S1>;
using S3 = meta_invoke<S2>;
using S4 = meta_invoke<S3>;

using a0 = meta_aligned_iterator_details::seek_to<0, int>;
using a8 = meta_aligned_iterator_details::seek_to<4, std::string>;
using a40 = meta_aligned_iterator_details::seek_to<40, char>;
using a48 = meta_aligned_iterator_details::seek_to<41, std::string>;

int main() {
  std::cout << "pipe composed in one chain\n";
  std::cout << "S3 ret == original-order exp_list<a0,a8,a40,a48>: "
            << std::is_same_v<typename S3::ret, exp_list<a0, a8, a40, a48>>
            << "\n";
  std::cout << "S4 ret eol: "
            << std::is_same_v<typename S4::ret, literal_types::end_of_list>
            << "\n";

  // drive the final original-order list at runtime
  std::cout << "original-order offsets: ";
  meta_for<meta_iterator, meta_istream<typename S3::ret>>::for_each(
      [](auto s) { std::cout << s.value() << (s.left() ? ',' : '\n'); });
  return 0;
}
