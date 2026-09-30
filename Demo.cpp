#include <iostream>
#include <tuple>

// meta_stream: a header-only template metaprogramming library that gives TMP
// runtime-like stream capabilities -- istream / ostream, meta-functions and
// meta-states -- so meta-algorithms are written directly inside (alias)
// function bodies instead of template specializations / partial specializations.
//
// Build and see it run live on Compiler Explorer:
//   https://gcc.godbolt.org/z/fKqE5a7n9
#include "meta_stream.hpp"

using namespace meta_ios;

// Placeholder result for states whose apply step must not move the state
// forward; the meaningful transition happens through pred / on_changed.
struct default_t {};

// --- selected states -----------------------------------------------------
// A meta-states class is a small state machine used as an ostream:
//   ::type        initial state object
//   ::pred        decides whether the current input changes the state
//   ::on_changed  produces the next state object when pred is true
//   ::apply       the default step when nothing changes (here a no-op)
//
// `selected` scans an index stream: when the incoming index equals Pass, it has
// found the target and pushes Indx out (on_changed). Its stream_op flags are
// opSkip|opCallIs: while pred is false every non-matching input is skipped and
// the istream advances, so at runtime only the matching branch survives.
template <std::size_t Pass, std::size_t Indx>
struct selected : stream_op<opSkip | opCallIs> {
  using type = Idx<0>;

  // not changed: leave the state where it is
  template <class this_idx, class from_ins>
  using apply = default_t;

  // changed only when the incoming index is the one we are looking for
  template <class this_idx, class from_ins>
  using pred = std::bool_constant<(from_ins::value == Pass)>;

  // changed: emit the target index
  template <class this_idx, class from_ins>
  using on_changed = Idx<Indx>;
};

// Break the loop as soon as the selected states reports a change on the last
// step, i.e. the target has been found.
struct break_on_change {
  template <class Stream>
  using apply = std::bool_constant<Stream::to::last_changed>;
};

// Wrap the user states struct into a runnable meta_states_object.
template <std::size_t P, std::size_t I>
using selected_states = meta_make_states<selected<P, I>>;

// --- dec_size states -----------------------------------------------------
// Convert a GLOBAL flat index across all tuples into a LOCAL index inside the
// tuple that actually contains it. opBreak is set: once pred becomes false the
// stream breaks, leaving the record positioned on the correct tuple.
//
// The state tracks how many elements of the current tuple have been consumed;
// pred stays true while the sought index still lies beyond this tuple's size,
// and on_changed subtracts that size to move on to the next tuple.
template <std::size_t I>
struct dec_size : stream_op<opBreak> {
  using type = Idx<I>;

  template <class this_idx, class from_ins>
  using apply = default_t;

  // true while the sought index is past the end of the current tuple
  template <class this_idx, class from_ins>
  using pred =
      std::bool_constant<(this_idx::value + 1 > exp_size<from_ins>)>;

  // step over this tuple: subtract its size from the running index
  template <class this_idx, class from_ins>
  using on_changed = Idx<this_idx::value - exp_size<from_ins>>;
};

// Select an element from a pack of tuples using a GLOBAL flat index.
template <std::size_t I, class... Tp>
  requires(I < (exp_size<std::remove_cvref_t<Tp>> + ...))
decltype(auto) from_tuples_v(Tp&&... tps) {
  // The high 8 bits of a states' flags hold the operation code (opr_code) that
  // drives the skip / break logic; the low bits record the change history.
  constexpr uint64_t code_mask = 0xFF00000000000000;

  // Phase 1: run dec_size over the list of tuples. The resulting ostream's
  // flags record which tuples were stepped through, and opBreak stops the
  // stream on the tuple that contains index I.
  using pass_recrd_o = typename transfer_until<
      meta_make_states<dec_size<I>>,
      meta_istream_list<std::remove_cvref_t<Tp>...>>::to;

  // Number of tuples fully passed: count the change bits outside the opcode.
  // Because opBreak is set, the bit for the current (matching) tuple is 0.
  constexpr std::size_t passed_count =
      std::popcount(pass_recrd_o::flags & ~(code_mask));

  // Local index within the tuple that holds the sought element.
  constexpr std::size_t index = pass_recrd_o::type::value;

  // Phase 2: with opSkip|opCallIs every non-matching branch is eliminated at
  // compile time, so the callable is instantiated only for the one correct
  // tuple and is invoked exactly once. The return type is therefore unique and
  // safe to return. selected_states pushes the correct local index through
  // on_changed, and break_on_change stops as soon as it is found.
  return meta_transfer_until<
      selected_states<passed_count, index>,
      index_sequence_istream<sizeof...(Tp)>,
      break_on_change>::for_each_forward(
      [](auto index_stream, auto&& tp) {
        return std::get<index_stream.value()>(tp);
      },
      std::forward<Tp>(tps)...);
}

// --- meta_pipe: lazy, composable stream pipelines -----------------------
//   meta_pipe<istream>::all_to<ostream>...   append a stage to the pipeline
//   ::run<iterator>                          choose how the pipe is driven
//   ::for_each / ::for_each<protocol>(f)     start flowing and visit stages
// The pipe is lazy: nothing flows until it is run. The istream / ostream slots
// are constrained by concepts, so only valid meta-streams (including a
// meta_transfer_until<to, from>) are accepted. Meta-states are valid ostreams;
// the raw states structs are not -- wrap them with meta_make_states first.

int main() {
  std::tuple t{1, 3.33, std::string("test")};
  std::tuple t1{std::string("my_str"), 5.74};

  // With no protocol, the callable receives the whole meta-stream stage. For
  // every global index 0..4, from_tuples_v selects the value from the tuple
  // that contains it and prints it, comma-separated.
  meta_pipe<index_sequence_istream<5>>::run<meta_iterator>::for_each(
      [&t, &t1](auto stream) {
        std::cout << from_tuples_v<stream.value()>(t, t1)
                  << (stream.left() ? ',' : '\n');
      });

  // NOTE: the aligned-iterator part below is a demonstration, not a general
  // guarantee: meta_aligned_iterator does not promise its layout fits every
  // C++ struct. Its purpose is to show how data can be managed inside a
  // heterogeneic vector -- use it as a navigator when building your own.
  struct S {
    int a;
    double b;
    std::string s;
  };

  std::cout << std::endl;
  S s{10, 3.13, std::string{"helloS"}};

  // Build a reversed ostream of aligned iterators over the member types
  // int, double, std::string. protocols::wait_for_end_to_t waits until the
  // istream is exhausted: a list inserted in reverse only yields the correct,
  // fully assembled type at the end. The wait idles the visit until it then
  // releases the stream and extracts its ostream, so the callable runs exactly
  // once. Visiting via protocols::stream_to_t hands it the aligned-iterator
  // list, and each iterator reads its member directly out of &s.
  meta_pipe<meta_istream_list<int, double, std::string>>::all_to<
      meta_aligned_iterator>::all_to<meta_rostream<>,
                                     protocols::wait_for_end_to_t>::run<
      meta_iterator>::for_each<protocols::stream_to_t>(
      [&s]<class... PTR>(exp_list<PTR...>) {
        ((std::cout << PTR{}.get(reinterpret_cast<std::byte*>(&s)) << ' '),
         ...);
      });
}
