#pragma once

#include <cstdint>
#include <concepts>

namespace psx {
	template <typename Ty>
	concept Stackable = requires {
		{ std::default_initializable<Ty> };
		{ std::copy_constructible<Ty> };
		{ std::copyable<Ty> };
	};

	/// <summary>
	/// A fixed-size stack allocated stack. 
	/// Incredibly simple, no error checking
	/// is present
	/// </summary>
	template <Stackable Contained, uint32_t Size>
	class Stack {
	public :
		constexpr Stack() :
			m_stacktop{}, m_thestack{} {}

		constexpr Contained const& top() const noexcept {
			return m_thestack[m_stacktop - 1];
		}

		[[nodiscard]] constexpr Contained pop() noexcept {
			return m_thestack[--m_stacktop];
		}

		constexpr void push(Contained elem) noexcept {
			m_thestack[m_stacktop++] = elem;
		}

		constexpr uint32_t stacktop() const noexcept {
			return m_stacktop;
		}

	private :
		uint32_t m_stacktop;
		Contained m_thestack[Size];
	};
}