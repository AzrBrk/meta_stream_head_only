#pragma once

#include <cstddef>
#include <cstdint>
#include <exception>
#include <istream>
#include <memory>
#include <new>
#include <ostream>
#include <type_traits>
#include <utility>

#include "meta_stream.hpp"

namespace type_safe_aligned {

using namespace meta_ios;
using namespace exp_utilities;
using namespace meta_objects;

struct larger_alignment_f {
  template <class this_type, class from_ins>
  using initialize = from_ins;

  template <class this_type, class from_ins>
  using apply = std::conditional_t<alignof(this_type) < alignof(from_ins),
                                   from_ins, this_type>;
};

template <class... Ty>
consteval std::size_t field_size() {
  static_assert((!std::is_reference_v<Ty> && ...),
                "can not calculate size for reference types");

  using larger_alignment_calculator = meta_object_construct<larger_alignment_f>;
  using largest_aligned_t = typename meta_all_transfer<
      larger_alignment_calculator, meta_istream_list<Ty...>>::to_t;
  using end_seek = typename meta_all_transfer<
      meta_aligned_iterator, meta_istream_list<Ty...>>::to;
  return meta_invoke<end_seek, largest_aligned_t>::type::value;
}

// State predicate used to build a bit mask showing which list positions
// contain a requested type. The mask supports repeated types in Ty....
template <class T>
struct same_states_iterator : states_base, stream_op<deactivate(opSkip)> {
  template <class this_type, class from_is>
  using pred = std::is_same<T, from_is>;
};

// Compile-time mask: bit i is set when Ty[i] is T.
template <class T, class... Ty>
constexpr std::uint64_t type_mask_series =
    transfer_until<meta_make_states<same_states_iterator<T>>,
                   meta_istream_list<Ty...>>::to::flags &
    ~(0xFF00000000000000);

template <class... Ty>
struct aligned_iterator_storage {
  enum class error { null_pointer, type_not_safe, index_exceed };

  class exception : public std::exception {
   public:
    explicit exception(error code) noexcept : code_(code) {}
    error code() const noexcept { return code_; }

    const char* what() const noexcept override {
      switch (code_) {
        case error::null_pointer:
          return "aligned_iterator_storage: null pointer";
        case error::type_not_safe:
          return "aligned_iterator_storage: type is not safe at this index";
        case error::index_exceed:
          return "aligned_iterator_storage: index exceeds type list";
      }
      return "aligned_iterator_storage: unknown error";
      // A non-owning view of one slot in an aligned byte buffer. The caller owns
      // the buffer and is responsible for destroying any objects constructed in it.
    }

        // Runtime failures for invalid addresses, types, and indices.
   private:
    error code_;
  };

  std::byte* ptr{nullptr};
  std::size_t m_type_index{0};

  // Returns whether T is permitted at this slot. T must occur in Ty....
  // Throws for a null pointer or an index outside the type list.
  template <class T>
  bool safe() const {
    if constexpr (!exp_try_find<T, exp_list<Ty...>>::value) {
      static_assert(exp_try_find<T, exp_list<Ty...>>::value,
                    "T must be one of aligned_iterator_storage's types");
    } else {
      if (m_type_index >= sizeof...(Ty)) {
        throw exception(error::index_exceed);
      }
      if (ptr == nullptr) {
        throw exception(error::null_pointer);
      }
      return (type_mask_series<T, Ty...> >> m_type_index) & 1;
    }
  }

  // Starts a T object's lifetime at this slot using placement new.
  // T must occur in Ty... and match the current slot's type.
  template <class T>
  bool emplace(T&& val) {
    using value_type = std::remove_cvref_t<T>;
    if constexpr (!exp_try_find<value_type, exp_list<Ty...>>::value) {
      static_assert(exp_try_find<value_type, exp_list<Ty...>>::value,
                    "T must be one of aligned_iterator_storage's types");
    } else {
      if (m_type_index >= sizeof...(Ty)) {
        throw exception(error::index_exceed);
      }
      if (ptr == nullptr) {
        throw exception(error::null_pointer);
      }
      if (!((type_mask_series<value_type, Ty...> >> m_type_index) & 1)) {
        throw exception(error::type_not_safe);
      }
      ::new (static_cast<void*>(ptr)) value_type(std::forward<T>(val));
      return true;
    }
  }

  // Provides checked access to the current object as T&.
  template <class T>
  operator T&() const {
    if constexpr (!exp_try_find<T, exp_list<Ty...>>::value) {
      static_assert(exp_try_find<T, exp_list<Ty...>>::value,
                    "T must be one of aligned_iterator_storage's types");
    } else {
      if (m_type_index >= sizeof...(Ty)) {
        throw exception(error::index_exceed);
      }
      if (ptr == nullptr) {
        throw exception(error::null_pointer);
      }
      if (!((type_mask_series<T, Ty...> >> m_type_index) & 1)) {
        throw exception(error::type_not_safe);
      }
      return *reinterpret_cast<T*>(ptr);
    }
  }

  // Assigns to the current object after verifying T matches the slot type.
  template <class T>
  void operator=(T const& val) {
    if constexpr (!exp_try_find<T, exp_list<Ty...>>::value) {
      static_assert(exp_try_find<T, exp_list<Ty...>>::value,
                    "T must be one of aligned_iterator_storage's types");
    } else {
      if (m_type_index >= sizeof...(Ty)) {
        throw exception(error::index_exceed);
      }
      if (ptr == nullptr) {
        throw exception(error::null_pointer);
      }
      if (!((type_mask_series<T, Ty...> >> m_type_index) & 1)) {
        throw exception(error::type_not_safe);
      }
      *reinterpret_cast<T*>(ptr) = val;
    }
  }
};

// Writes the value at the current slot using its type from Ty....
template <class... Ty>
std::ostream& operator<<(std::ostream& os,
                         aligned_iterator_storage<Ty...> const& storage) {
  using storage_type = aligned_iterator_storage<Ty...>;
  if (storage.m_type_index >= sizeof...(Ty)) {
    throw typename storage_type::exception(storage_type::error::index_exceed);
  }
  meta_for<meta_iterator, meta_istream_list<Ty...>>::for_each(
  // Forward iterator over the aligned slots described by Ty.... The supplied
  // pointer is the start of the backing byte buffer; offsets are computed at
  // compile time, including any padding required between adjacent types.
      [&os, &storage]<class Stream>(Stream s) {
        using current_type = typename Stream::to_t;
        constexpr auto stream_index = sizeof...(Ty) - s.left() - 1;
        if (os && stream_index == storage.m_type_index &&
            storage.template safe<current_type>()) {
          os << static_cast<current_type&>(storage);
        }
      });
  return os;
}

// Reads into the current slot using its type from Ty....
template <class... Ty>
std::istream& operator>>(std::istream& is,
                         aligned_iterator_storage<Ty...>& storage) {
  using storage_type = aligned_iterator_storage<Ty...>;
  if (storage.m_type_index >= sizeof...(Ty)) {
    throw typename storage_type::exception(storage_type::error::index_exceed);
  }
  meta_for<meta_iterator, meta_istream_list<Ty...>>::for_each(
      [&is, &storage]<class Stream>(Stream s) {
        using current_type = typename Stream::to_t;
        constexpr auto stream_index = sizeof...(Ty) - s.left() - 1;
        if (is && stream_index == storage.m_type_index &&
            storage.template safe<current_type>()) {
          is >> static_cast<current_type&>(storage);
        }
      });
  return is;
}

template <class... Ty>
class type_safe_aligned_iterator {
  static_assert(sizeof...(Ty) > 0,
                "type_safe_aligned_iterator requires at least one type");

  using offset_array_type =
      typename meta_pipe<meta_istream_list<Ty...>>
          ::template all_to<meta_aligned_iterator>
          ::template run<meta_ostream<>>::type::to_t;
  static constexpr auto offsets = to_meta_array_t<offset_array_type>::array();
  // Total layout size is the position immediately after the final slot.
  static constexpr std::size_t total_size =
      meta_all_transfer<meta_aligned_iterator,
                        meta_istream_list<Ty...>>::to_t::value;

  aligned_iterator_storage<Ty...> storage_{};

  static constexpr std::size_t offset_at(std::size_t index) {
    return index == sizeof...(Ty) ? total_size : offsets[index];
  }

  // Advances the slot index and moves the pointer by the aligned offset delta.
  type_safe_aligned_iterator& advance() {
    using storage_type = aligned_iterator_storage<Ty...>;
    if (storage_.m_type_index >= sizeof...(Ty)) {
      throw typename storage_type::exception(storage_type::error::index_exceed);
    }
    if (storage_.ptr == nullptr) {
      throw typename storage_type::exception(storage_type::error::null_pointer);
    }
    const auto current_offset = offset_at(storage_.m_type_index);
    const auto next_offset = offset_at(storage_.m_type_index + 1);
    storage_.ptr += next_offset - current_offset;
    ++storage_.m_type_index;
    return *this;
  }

 public:
  // Constructs an iterator at slot type_index; sizeof...(Ty) denotes end().
  // A null base pointer is retained and diagnosed when a slot is accessed.
  explicit type_safe_aligned_iterator(std::byte* base_ptr,
                                      std::size_t type_index) {
    if (type_index > sizeof...(Ty)) {
      using storage_type = aligned_iterator_storage<Ty...>;
      throw typename storage_type::exception(storage_type::error::index_exceed);
    }
    storage_.ptr = base_ptr == nullptr ? nullptr : base_ptr + offset_at(type_index);
    storage_.m_type_index = type_index;
  }

  // Dereference exposes the checked storage proxy for the current slot.
  aligned_iterator_storage<Ty...>& operator*() { return storage_; }

  // Prefix increment advances to the next type in the aligned layout.
  type_safe_aligned_iterator& operator++() { return advance(); }

  bool operator==(type_safe_aligned_iterator const& other) const {
    return storage_.ptr == other.storage_.ptr &&
           storage_.m_type_index == other.storage_.m_type_index;
  }

  bool operator!=(type_safe_aligned_iterator const& other) const {
    return !(*this == other);
  }
};

template <class... Ty>
class type_safe_aligned_reverse_iterator {
  static_assert(sizeof...(Ty) > 0,
                "type_safe_aligned_reverse_iterator requires at least one type");

  using reverse_aligned_ptr_list =
      typename meta_pipe<meta_istream_list<Ty...>>
          ::template all_to<meta_aligned_iterator>
          ::template run<meta_rostream<>>::type::to::type;
  static constexpr auto offsets =
      to_meta_array_t<reverse_aligned_ptr_list>::array();

  aligned_iterator_storage<Ty...> storage_{};
  std::byte* base_ptr_{nullptr};
  std::size_t reverse_index_{0};

  void set_position(std::size_t reverse_index) {
    reverse_index_ = reverse_index;
    if (reverse_index_ == sizeof...(Ty)) {
      storage_.ptr = base_ptr_;
      storage_.m_type_index = sizeof...(Ty);
      return;
    }
    storage_.ptr = base_ptr_ == nullptr ? nullptr : base_ptr_ + offsets[reverse_index_];
    storage_.m_type_index = sizeof...(Ty) - reverse_index_ - 1;
  }

 public:
  explicit type_safe_aligned_reverse_iterator(std::byte* base_ptr,
                                               std::size_t reverse_index) :
      base_ptr_(base_ptr) {
    if (reverse_index > sizeof...(Ty)) {
      using storage_type = aligned_iterator_storage<Ty...>;
      throw typename storage_type::exception(storage_type::error::index_exceed);
    }
    set_position(reverse_index);
  }

  aligned_iterator_storage<Ty...>& operator*() { return storage_; }

  void destroy() {
    using storage_type = aligned_iterator_storage<Ty...>;
    if (reverse_index_ >= sizeof...(Ty)) {
      throw typename storage_type::exception(storage_type::error::index_exceed);
    }
    if (storage_.ptr == nullptr) {
      throw typename storage_type::exception(storage_type::error::null_pointer);
    }
    meta_for<meta_iterator, meta_istream_list<Ty...>>::for_each(
        [this]<class Stream>(Stream stream) {
          using current_type = typename Stream::to_t;
          constexpr auto stream_index = sizeof...(Ty) - stream.left() - 1;
          if (stream_index == storage_.m_type_index) {
            std::destroy_at(reinterpret_cast<current_type*>(storage_.ptr));
          }
        });
  }

  type_safe_aligned_reverse_iterator& operator++() {
    using storage_type = aligned_iterator_storage<Ty...>;
    if (reverse_index_ >= sizeof...(Ty)) {
      throw typename storage_type::exception(storage_type::error::index_exceed);
    }
    if (storage_.ptr == nullptr) {
      throw typename storage_type::exception(storage_type::error::null_pointer);
    }
    set_position(reverse_index_ + 1);
    return *this;
  }

  bool operator==(type_safe_aligned_reverse_iterator const& other) const {
    return base_ptr_ == other.base_ptr_ && reverse_index_ == other.reverse_index_;
  }

  bool operator!=(type_safe_aligned_reverse_iterator const& other) const {
    return !(*this == other);
  }
};

// Non-owning range adapter so a typed byte buffer can be traversed with
// range-for: for (auto& slot : type_safe_aligned_range<Ty...>{buffer}).
template <class... Ty>
class type_safe_aligned_range {
  static_assert((!std::is_reference_v<Ty> && ...),
                "type_safe_aligned_range does not accept reference types; "
                "use std::reference_wrapper<T>");

 public:
  // base_ptr must address storage large enough for the complete Ty... layout.
  explicit type_safe_aligned_range(std::byte* base_ptr) : base_ptr_(base_ptr) {}

  // Returns the iterator for the first aligned slot.
  type_safe_aligned_iterator<Ty...> begin() const {
    return type_safe_aligned_iterator<Ty...>(base_ptr_, 0);
  }

  // Returns the one-past-last iterator required by range-for.
  type_safe_aligned_iterator<Ty...> end() const {
    return type_safe_aligned_iterator<Ty...>(base_ptr_, sizeof...(Ty));
  }

  type_safe_aligned_reverse_iterator<Ty...> rbegin() const {
    return type_safe_aligned_reverse_iterator<Ty...>(base_ptr_, 0);
  }

  type_safe_aligned_reverse_iterator<Ty...> rend() const {
    return type_safe_aligned_reverse_iterator<Ty...>(base_ptr_, sizeof...(Ty));
  }

  std::byte* data() const { return base_ptr_; }

 private:
  std::byte* base_ptr_;
};

template <class... Ty, class... Args>
void initialize(type_safe_aligned_range<Ty...> const& range,
                Args&&... args) {
  static_assert(sizeof...(Ty) == sizeof...(Args),
                "initialize requires one argument for each range element");
  auto* data = range.data();
  meta_for<meta_aligned_iterator, meta_istream_list<Ty...>>::for_each_forward(
      [&range, data]<class Stream, class Arg>(Stream ptr_stream, Arg&& value) {
        try {
          ptr_stream.object().emplace(data, std::forward<Arg>(value));
        } catch (...) {
          constexpr auto left = ptr_stream.left();
          auto destroy_iter = range.rbegin();
          for (std::size_t i = 0; i <= left; ++i) {
            ++destroy_iter;
          }
          for (; destroy_iter != range.rend(); ++destroy_iter) {
            destroy_iter.destroy();
          }
          throw;
        }
      },
      std::forward<Args>(args)...);
}

}  // namespace type_safe_aligned