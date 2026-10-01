#include <array>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>
#include "meta_stream.hpp"
using namespace exp_utilities;

// Perfect forwarding (like std::get): a prvalue argument yields an rvalue
// reference to the caller's materialized temporary.
static_assert(std::is_same_v<decltype(args_at<0>(1, 2.5, 3)), int&&>);
static_assert(std::is_same_v<decltype(args_at<1>(1, 2.5, 3)), double&&>);
static_assert(std::is_same_v<
              decltype(args_at<2>(1, 2.5, std::string("x"))), std::string&&>);

// value category: lvalue -> lvalue reference, rvalue -> rvalue reference
static_assert(std::is_lvalue_reference_v<decltype(
                  args_at<0>(std::declval<int&>(), std::declval<int&>()))>);
static_assert(std::is_lvalue_reference_v<decltype(
                  args_at<1>(std::declval<int&>(), std::declval<int&>()))>);
static_assert(std::is_rvalue_reference_v<decltype(args_at<0>(1, 2))>);

// out-of-range index is rejected by the requires clause
template <std::size_t I, class... A>
concept can_args_at = requires(A... a) { args_at<I>(std::forward<A>(a)...); };
static_assert(can_args_at<0, int, int>);
static_assert(!can_args_at<2, int, int>);
static_assert(!can_args_at<5, int, int, int>);

// large packs work (the old change-history test capped at ~56 elements)
template <std::size_t N, std::size_t I, std::size_t... J>
std::size_t big_select(std::index_sequence<J...>) {
  std::array<std::size_t, N> a{};
  for (std::size_t i = 0; i < N; ++i) a[i] = i;
  return args_at<I>(a[J]...);
}

int main() {
  int a = 7;
  std::string s = "hi";

  // the lvalue result is writable through the returned reference
  args_at<0>(a, s) = 42;
  args_at<1>(a, s) = "done";
  std::cout << a << ' ' << s << ' ' << args_at<0>(1, 2, 3) << '\n';

  // arbitrary-size pack: middle and last elements
  std::size_t mid = big_select<1000, 500>(std::make_index_sequence<1000>{});
  std::size_t last = big_select<1000, 999>(std::make_index_sequence<1000>{});
  assert(mid == 500 && last == 999);
  std::cout << "big pack: " << mid << ' ' << last << '\n';
}
