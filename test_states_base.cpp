#include <type_traits>
#include <cstddef>
#include "meta_stream.hpp"
using namespace meta_ios;

// states_base itself is a valid (do-nothing) states type: it supplies all four
// required members, but no opr_code.
static_assert(meta_states<states_base>, "states_base satisfies meta_states");
static_assert(!meta_states_with_opcode<states_base>,
              "states_base carries no opr_code");

// A states class that overrides only type/pred/on_changed and leaves apply to
// the inherited default. The concept must see the inherited member templates.
struct only_pred : states_base {
  using type = Idx<0>;
  template <class T, class U>
  using pred = std::bool_constant<U::value == 2>;
  template <class T, class U>
  using on_changed = Idx<9>;
};
static_assert(meta_states<only_pred>, "inherited apply still satisfies concept");

// Combine states_base with stream_op via multiple inheritance to also get
// opr_code flags; no member-name clashes.
struct tagged : stream_op<opSkip>, states_base {
  using type = Idx<0>;
  template <class T, class U>
  using pred = std::bool_constant<true>;
  template <class T, class U>
  using on_changed = U;
};
static_assert(meta_states<tagged>, "tagged states");
static_assert(meta_states_with_opcode<tagged>, "tagged states has opr_code");

// Sanity: a type without the states members is still rejected, proving the
// members above really come from states_base.
struct not_states {};
static_assert(!meta_states<not_states>, "empty struct is not states");

// End-to-end: find the position of value Pass, relying on the inherited apply
// (opSkip|opCallIs means non-matching inputs never reach apply).
template <std::size_t Pass>
struct find_index : stream_op<opSkip | opCallIs>, states_base {
  using type = Idx<0>;
  template <class T, class U>
  using pred = std::bool_constant<U::value == Pass>;
  template <class T, class U>
  using on_changed = Idx<Pass>;
};
using found = typename transfer_until<
    meta_make_states<find_index<3>>, index_sequence_istream<6>>::to;
static_assert(std::is_same_v<typename found::type, Idx<3>>,
              "must stop holding the matched position");

// The inherited apply is genuinely invoked when the ostream function is forced
// on a non-change step (OP_OS_IDLE); OP_IS_IDLE advances the input each time.
// The inherited apply returns default_t, so the object becomes default_t.
struct force_default : stream_op<OP_OS_IDLE | OP_IS_IDLE>, states_base {
  using type = Idx<7>;
};
using forced = typename transfer_until<
    meta_make_states<force_default>, index_sequence_istream<3>>::to;
static_assert(std::is_same_v<typename forced::type, default_t>,
              "forced call to inherited apply yields default_t");

int main() { return 0; }
