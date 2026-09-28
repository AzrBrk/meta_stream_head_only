#include <type_traits>
#include <cstddef>
#include "meta_stream.hpp"
using namespace meta_ios;
using namespace exp_utilities;
using namespace meta_objects;

// A pipe node that fills a rostream; wait_for_end makes it emit skip_signal
// while still filling and the final reversed list once full.
// ret sequence over index_sequence_istream<3>: skip, skip, reversed_list, eol
using node =
    meta_pipe<index_sequence_istream<3>>::all_to<
        meta_rostream<>, protocols::wait_for_end, protocols::stream_to_t>;

// Shared states body: push every received X into an exp_list; pred always true.
template <std::uint64_t Spec>
struct collector : stream_op<Spec> {
  using type = exp_list<>;
  template <class T, class X>
  using apply = typename T::template push_back<X>;
  template <class T, class X>
  using on_changed = typename T::template push_back<X>;
  template <class T, class X>
  using pred = std::true_type;
};

// (A) default: opIsSkip ON -> skip_signal consumed, only the final list collected
using c = typename transfer_until<
    meta_make_states<collector<0>>, typename node::from>::to;
static_assert(exp_size<typename c::type> == 1, "consumer sees only final list");

// (B) deactivate opIsSkip -> the states receives and collects the skip_signals
using w = typename transfer_until<
    meta_make_states<collector<deactivate(opIsSkip)>>,
    typename node::from>::to;
using got = typename w::type;
static_assert(exp_size<got> == 3, "wanter sees skip, skip, final list");
static_assert(std::is_same_v<typename got::at<0>, protocols::meta_stream_skip_signal>,
              "first collected is skip");
static_assert(std::is_same_v<typename got::at<1>, protocols::meta_stream_skip_signal>,
              "second collected is skip");

int main() { return 0; }
