#include <type_traits>

#include "meta_objects.hpp"

using namespace meta_objects;

struct stop_condition {
  template <class>
  struct apply : std::false_type {};
};

using loop = meta_loop::meta_looper<meta_object<void, stop_condition>,
                                    meta_empty_o>;
using result = meta_invoke<loop>;

static_assert(std::is_same_v<typename result::type,
                             meta_objects_details::meta_empty>);
