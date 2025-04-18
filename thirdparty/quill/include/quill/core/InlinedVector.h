/**
 * @page copyright
 * Copyright(c) 2020-present, Odysseas Georgoudis & quill contributors.
 * Distributed under the MIT License (http://opensource.org/licenses/MIT)
 */

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "../core/Attributes.h"
#include "../core/Common.h"
#include "../core/QuillError.h"

QUILL_BEGIN_NAMESPACE

namespace detail
{
template <typename T, size_t N>
class InlinedVector
{
public:
  using value_type = T;
  static_assert(std::is_trivially_copyable_v<value_type>, "value_type must be trivially copyable");

  InlinedVector() noexcept
  {
    for (size_t i = 0; i < _capacity; ++i)
    {
      _storage.inline_buffer[i] = value_type{};
    }
  }

  ~InlinedVector()
  {
    if (_capacity != N)
    {
      delete[] _storage.heap_buffer;
    }
  }

  /**
   * Deleted
   */
  InlinedVector(InlinedVector const& other) = delete;
  InlinedVector& operator=(InlinedVector const& other) = delete;

  /**
   * Push back a new element
   */
  QUILL_ATTRIBUTE_HOT value_type push_back(value_type value)
  {
    if (_size == _capacity)
    {
      size_t const new_capacity = _capacity * 2;
      auto* new_data = new value_type[new_capacity];

      if (QUILL_UNLIKELY(new_capacity <= _capacity))
      {
        QUILL_THROW(
          QuillError{"This unreachable code is here only to suppress gcc false positive warnings"});
      }

      if (_capacity == N)
      {
        // Entering here for the first time, then we copy the inline storage
        for (size_t i = 0; i < _size; ++i)
        {
          new_data[i] = _storage.inline_buffer[i];
        }
      }
      else
      {
        for (size_t i = 0; i < _size; ++i)
        {
          new_data[i] = _storage.heap_buffer[i];
        }
        delete[] _storage.heap_buffer;
      }

      _storage.heap_buffer = new_data;
      _capacity = new_capacity;
    }

    if (_capacity == N)
    {
      _storage.inline_buffer[_size] = value;
    }
    else
    {
      _storage.heap_buffer[_size] = value;
    }

    ++_size;

    return value;
  }

  /**
   * Access element
   */
  QUILL_NODISCARD QUILL_ATTRIBUTE_HOT value_type operator[](size_t index) const
  {
#ifdef __MINGW32__
  // Disable the array bounds warning for MinGW
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Warray-bounds"
#endif

    if (QUILL_UNLIKELY(index >= _size))
    {
      QUILL_THROW(QuillError{"index out of bounds"});
    }

    if (_capacity == N)
    {
      return _storage.inline_buffer[index];
    }
    else
    {
      return _storage.heap_buffer[index];
    }

#ifdef __MINGW32__
  // Re-enable the array bounds warning
  #pragma GCC diagnostic pop
#endif
  }

  /**
   * Assign value at index
   */
  QUILL_ATTRIBUTE_HOT void assign(size_t index, value_type value)
  {
    if (QUILL_UNLIKELY(index >= _size))
    {
      QUILL_THROW(QuillError{"index out of bounds"});
    }

    if (_capacity == N)
    {
      _storage.inline_buffer[index] = value;
    }
    else
    {
      _storage.heap_buffer[index] = value;
    }
  }

  QUILL_NODISCARD QUILL_ATTRIBUTE_HOT size_t size() const noexcept { return _size; }
  QUILL_NODISCARD size_t capacity() const noexcept { return _capacity; }
  QUILL_ATTRIBUTE_HOT void clear() noexcept { _size = 0; }

private:
  union Storage
  {
    value_type inline_buffer[N];
    value_type* heap_buffer;
  } _storage;

  size_t _size{0};
  size_t _capacity{N};
};

/**
 * A vector that stores sizes for specific operations using `uint32_t` to optimize space.
 * The capacity of 12 is chosen to fit within a full cache line for better performance.
 */
using SizeCacheVector = InlinedVector<uint32_t, 12>;
static_assert(sizeof(SizeCacheVector) <= detail::CACHE_LINE_SIZE,
              "SizeCacheVector should not exceed a cache line");
} // namespace detail

QUILL_END_NAMESPACE
