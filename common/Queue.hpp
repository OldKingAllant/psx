#pragma once

#include <algorithm>

namespace psx {
	template <typename Ty, size_t size>
	class Queue {
	public :
		constexpr Queue() :
			m_curr_size{}, 
			arr{} {}

		constexpr size_t len() const {
			return m_curr_size;
		}

		constexpr Ty const& peek() const {
			return arr[0];
		}

		constexpr Ty& peek() {
			return arr[0];
		}

		constexpr Ty& back() {
			return arr[m_curr_size - 1];
		}

		constexpr Ty const& back() const {
			return arr[m_curr_size - 1];
		}

		constexpr Ty deque() {
			Ty item = arr[0];
			std::shift_left(arr, arr + m_curr_size, 1);
			m_curr_size--;
			return item;
		}

		constexpr size_t deque_block(size_t count, Ty* dest) {
			count = std::min(m_curr_size, count);
			std::copy_n(arr, count, dest);
			std::shift_left(arr, arr + m_curr_size, count);
			m_curr_size -= count;
			return count;
		}

		constexpr void queue(Ty element) {
			arr[m_curr_size++] = element;
		}

		constexpr bool empty() const {
			return m_curr_size <= 0;
		}

		constexpr bool full() const {
			return m_curr_size == CAPACITY;
		}

		constexpr void clear() {
			m_curr_size = 0;
		}

		constexpr Ty* begin() {
			return &arr[0];
		}

		constexpr Ty* end() {
			return (arr + m_curr_size);
		}

		static constexpr size_t CAPACITY = size;

	private :
		size_t m_curr_size;
		Ty arr[size];
	};
}