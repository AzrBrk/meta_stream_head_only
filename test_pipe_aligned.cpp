#include <iostream>
#include <type_traits>
#include <cstdint>
#include <cstddef>
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
  // buffer sized for the alignment layout (include an extra double probe)
  std::byte data[
      meta_all_transfer<meta_aligned_iterator,
      meta_istream_list<int, std::string, char, std::string,
      double>>::to_t::value]{};

  // construct in aligned order
  std::cout << "aligned offsets: ";
  meta_for<
      meta_aligned_iterator,
      meta_istream_list<int, std::string, char, std::string>
  >::for_each_forward(
      [&](auto aligned_stream, auto &&val) {
        std::cout << aligned_stream.value()
                  << (aligned_stream.left() ? ',' : '\n');
        aligned_stream.object().emplace(data, val);
      },
      10, std::string("hello"), 'k', std::string("world"));

  // overwrite the last std::string via the aligned iterator
  auto iter = transfer<3 + 1, meta_aligned_iterator,
                       meta_istream_list<int, std::string, char,
                                        std::string>>::to_t{};
  iter.get(data) = std::string("not world");

  // pipe: align -> (collect via transfer_until into r_ostream)
  using aligned_node =
      meta_pipe<meta_istream_list<int, std::string, char, std::string>>::
          all_to<meta_aligned_iterator>::transfer::from;
  using collected = transfer_until<r_ostream, aligned_node>::to::type;
  using final_is = meta_istream<collected>;

  // destroy in reverse construction order
  std::cout << "reverse destroy:" << std::endl;
  meta_for<meta_iterator, final_is>::for_each([&](auto aligned_stream) {
    std::cout << "  " << aligned_stream.object().address_of(data) << ' ';
    std::cout << aligned_stream.object().get(data) << " destroyed!"
              << std::endl;
    aligned_stream.object().destroy(data);
  });

  return 0;
}
