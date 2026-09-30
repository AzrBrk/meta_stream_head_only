#include <type_traits>
#include <iostream>
#include <string>
#include <utility>
#include "meta_stream.hpp"
using namespace exp_utilities;

// selected element's type follows the argument at position I
static_assert(std::is_same_v<decltype(args_at<0>(1, 2.5, 3)), int>);
static_assert(std::is_same_v<decltype(args_at<1>(1, 2.5, 3)), double>);
static_assert(std::is_same_v<
              decltype(args_at<2>(1, 2.5, std::string("x"))), std::string>);

// the result is always a value (even when the arguments are lvalues)
static_assert(!std::is_reference_v<decltype(args_at<0>(std::declval<int&>(),
                                                       std::declval<int&>()))>);

// out-of-range index is rejected by the requires clause
template <std::size_t I, class... A>
concept can_args_at = requires(A... a) { args_at<I>(std::forward<A>(a)...); };
static_assert(can_args_at<0, int, int>);
static_assert(!can_args_at<2, int, int>);
static_assert(!can_args_at<5, int, int, int>);

int main() {
  int a = 7;
  std::string s = "hi";

  // selecting from lvalues yields an independent value
  int picked = args_at<0>(a, s);
  std::cout << picked << ' ' << args_at<1>(1, 2.5, s) << ' '
            << args_at<0>(1, 2, 3) << '\n';
}
