#include <type_traits>
#include <iostream>
#include <string>
#include <utility>
#include "meta_stream.hpp"
using namespace exp_utilities;

// Perfect forwarding (like std::get): a prvalue argument yields an rvalue
// reference to the caller's materialized temporary.
static_assert(std::is_same_v<decltype(args_at<0>(1, 2.5, 3)), int&&>);
static_assert(std::is_same_v<decltype(args_at<1>(1, 2.5, 3)), double&&>);
static_assert(std::is_same_v<
              decltype(args_at<2>(1, 2.5, std::string("x"))), std::string&&>);

// underlying value types
static_assert(std::is_same_v<
              std::remove_cvref_t<decltype(args_at<0>(1, 2.5, 3))>, int>);
static_assert(std::is_same_v<
              std::remove_cvref_t<decltype(args_at<1>(1, 2.5, 3))>, double>);

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

int main() {
  int a = 7;
  std::string s = "hi";

  // the lvalue result is writable through the returned reference
  args_at<0>(a, s) = 42;
  args_at<1>(a, s) = "done";

  std::cout << a << ' ' << s << ' ' << args_at<0>(1, 2, 3) << '\n';
}
