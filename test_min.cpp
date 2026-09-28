#include <tuple>
#include <type_traits>
#include "meta_stream.hpp"
using namespace meta_ios;
using namespace exp_utilities;
using namespace meta_objects;

// Minimal one-shot reverse: rostream collects, wait_for_end skips partial
// collections, stream_to_t reads the full list, to_meta_array_t converts it.
using run_t = meta_pipe<index_sequence_istream<3>>::run_with<
    meta_rostream<>, protocols::wait_for_end, protocols::stream_to_t,
    to_meta_array_t>;
using arr = typename run_t::type::to::type;

static_assert(std::is_same_v<arr, meta_array<2, 1, 0>>,
              "should yield meta_array<2,1,0>");

int main() { return 0; }
