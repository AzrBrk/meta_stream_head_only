#include <type_traits>
#include <cstdint>
#include "meta_stream.hpp"
using namespace meta_ios;

// (1) no spec -> pure default
static_assert(stream_op<>::opr_code == OP_DEFAULT, "default");

// (2) enable forces a default-off bit on, leaves everything else default
static_assert(stream_op<enable(opCallOs)>::opr_code == (OP_DEFAULT | opCallOs),
              "enable opCallOs");

// (3) deactivate forces a default-on bit off, leaves everything else default
static_assert(stream_op<deactivate(opIsSkip)>::opr_code ==
                  (OP_DEFAULT & ~opIsSkip),
              "deactivate opIsSkip");

// (4) combined: one forced on, two forced off
static_assert(
    stream_op<enable(opCallOs) | deactivate(opIsSkip | opSkip)>::opr_code ==
        ((OP_DEFAULT | opCallOs) & ~(opIsSkip | opSkip)),
    "combined enable+deactivate");

// (5) deactivate all op bits -> high band fully clear
static_assert(
    stream_op<deactivate(0xFFULL << 56)>::opr_code == 0,
    "deactivate all");

int main() { return 0; }
