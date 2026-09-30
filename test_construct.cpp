#include <type_traits>
#include <cstddef>
#include <string>
#include "meta_stream.hpp"
using namespace meta_ios;
using namespace exp_utilities;
using namespace meta_objects;

// meta_istream_t still discriminates purely by structure: the refactor of the
// object-level initializer must not change what counts as an istream.
static_assert(meta_istream_t<meta_istream_list<int, double>>, "list istream");
static_assert(meta_istream_t<meta_istream<exp_list<int, char>>>, "basic istream");
static_assert(meta_istream_t<meta_ristream<exp_list<int, double>>>,
              "reverse istream");
static_assert(!meta_istream_t<meta_object<
                  exp_list<int>, meta_objects_details::meta_empty_fn>>,
              "plain meta_object (non-ret) is not an istream");
static_assert(!meta_istream_t<int>, "plain type is not an istream");

// A reduction whose accumulator has no usable seed. The constructor
// (F::initialize) seeds it with the first input; F::apply folds the rest.
// alignof is never applied to the uninitialized tag.
struct alignas(32) big_align {
  std::string s;
  double d;
};
struct larger_alignment {
  template <class this_obj, class from_is>
  using initialize = from_is;  // constructor: seed with the first element
  template <class this_obj, class from_is>
  using apply = std::conditional_t<(alignof(this_obj) >= alignof(from_is)),
                                   this_obj, from_is>;
};

using max_obj = typename transfer_until<
    meta_object_construct<larger_alignment>,
    meta_istream_list<char, int, double, big_align>>::to;
static_assert(std::is_same_v<typename max_obj::type, big_align>,
              "must reduce to the max-alignment type");

int main() { return 0; }
