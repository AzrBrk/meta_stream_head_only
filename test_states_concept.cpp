#include <type_traits>
#include <cstddef>
#include "meta_stream.hpp"
using namespace meta_ios;
using namespace exp_utilities;
using namespace meta_objects;

// Input element exposes ::tag, a member std::integral_constant does NOT have.
template <std::size_t N>
struct token {
  static constexpr std::size_t tag = N;
};

// Record a token's tag as the library index type Idx.
template <class X>
using token_as_index = Idx<X::tag>;

// A states type whose three member templates genuinely CANNOT be instantiated
// with std::integral_constant as the second argument: every body needs X::tag.
// They are nevertheless perfectly valid for the real token input below.
// Inherits stream_op so the wrapper can read an opr_code (as all states do).
struct token_collector : stream_op<0> {
  using type = exp_list<>;
  template <class T, class X>
  using apply = typename T::template push_back<token_as_index<X>>;
  template <class T, class X>
  using on_changed = typename T::template push_back<token_as_index<X>>;
  template <class T, class X>
  using pred = std::bool_constant<(X::tag >= 0)>;  // always true, needs ::tag
};

// The OLD meta_states predicate verbatim: it forced instantiating every member
// template with std::integral_constant<std::size_t, 0> as the second argument.
// A requires-expression safely reports false instead of hard-erroring.
template <class S>
constexpr bool old_meta_states_v = requires {
  typename S::type;
  typename S::template apply<typename S::type,
                             std::integral_constant<std::size_t, 0>>;
  typename S::template on_changed<typename S::type,
                                  std::integral_constant<std::size_t, 0>>;
  typename S::template pred<typename S::type,
                            std::integral_constant<std::size_t, 0>>;
};

static_assert(!old_meta_states_v<token_collector>,
              "old integral_constant-based check must reject this states");

// The NEW concept accepts it WITHOUT instantiating the member templates.
static_assert(meta_states<token_collector>,
              "meta_states must accept the states without forced instantiation");

// End-to-end: run the states over a list istream of tokens.
using result = typename transfer_until<
    meta_make_states<token_collector>,
    meta_istream_list<token<1>, token<2>, token<3>, token<4>>>::to;
using collected = typename result::type;

static_assert(exp_size<collected> == 4, "all four tokens must be collected");
static_assert(std::is_same_v<typename collected::at<0>, Idx<1>>, "token 1");
static_assert(std::is_same_v<typename collected::at<1>, Idx<2>>, "token 2");
static_assert(std::is_same_v<typename collected::at<2>, Idx<3>>, "token 3");
static_assert(std::is_same_v<typename collected::at<3>, Idx<4>>, "token 4");

int main() { return 0; }
