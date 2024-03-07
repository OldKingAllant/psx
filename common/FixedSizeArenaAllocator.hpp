#pragma once

#include "Defs.hpp"
#include "Macros.hpp"

#include <concepts>
#include <bit>

namespace psx {
	using psx::u64;
	using psx::u8;

	/// <summary>
	/// Simply a linked list node
	/// element
	/// </summary>
	/// <typeparam name="Ty">The type of the stored element</typeparam>
	template <std::default_initializable Ty>
	struct ArenaNode {
		Ty element;
		bool used;
		ArenaNode* next_free;

		constexpr ArenaNode(Ty elem) : element{elem}, 
			used{false},
			next_free{nullptr} {}

		constexpr ArenaNode() : element{},
			used{false},
			next_free{nullptr} {}
	};

	/// <summary>
	/// Tests the two necessary conditions
	/// that the arena allocator
	/// requires to work:
	/// <list type="bullet">
	/// <item>
	/// <description>1. That the element inside the node is
	///   aligned correctly</description>
	/// </item>
	/// <item>
	/// <description>2. That the element inside the node has
	///   the same address as the node itself</description>
	/// </item>
	/// </list>
	/// 
	/// Note that it would be cool to
	/// have this evaluated at compiletime.
	/// However, this is not possible with
	/// the current standard, since reinterpret_casts
	/// are not allowed in constexpr functions 
	/// (even though it doesn't make any sense)
	/// </summary>
	/// <typeparam name="Ty"></typeparam>
	/// <returns></returns>
	template <std::default_initializable Ty>
	void VerifyNode() {
		ArenaNode<Ty> dummy{};

		const Ty* ptr_to_element = &dummy.element;

		auto aliased_dummy = std::bit_cast<const Ty*>(&dummy);

		if(ptr_to_element != aliased_dummy) {
			throw "Pointer mismatch (element address must match node address)";
		}

		auto align = alignof(Ty);

		if ((decltype(align))ptr_to_element % align != 0) {
			throw "Invalid align (element inside node must have same align as normal element)";
		}
	}

	/// <summary>
	/// A simple arena allocator used to allocate
	/// fixed size objects. All the objects
	/// that can be contained in the pool
	/// are constructed upon allocation, 
	/// no deferred construction is possible
	/// </summary>
	/// <typeparam name="Ty">Type of the stored elements</typeparam>
	template <std::default_initializable Ty>
	class FixedArenaAllocator {
	private:
		ArenaNode<Ty>* m_pool;
		u64 m_size;

	public :
		/// <summary>
		/// Immediately allocates (ideally) sizeof(Ty) * pool_size
		/// bytes of memory
		/// </summary>
		/// <param name="pool_size">Number of elements inside the pool</param>
		FixedArenaAllocator(u64 pool_size) :
			m_pool{ new ArenaNode<Ty>[pool_size] },
			m_size{ pool_size } {
			VerifyNode<Ty>();

			for (u64 index = 0; index < pool_size - 1; index++) {
				m_pool[index].next_free = &m_pool[index + 1];
			}
		}

		~FixedArenaAllocator() {
			if (m_pool) {
				delete[] m_pool;
				m_pool = nullptr;
			}
		}

		/// <summary>
		/// Allocate one single element inside
		/// the pool
		/// </summary>
		/// <returns>A valid pointer if the allocation succeded</returns>
		Ty* Alloc() noexcept {
			if (m_pool->used == false) {
				m_pool->used = true;
				return &m_pool->element;
			}

			if (m_pool->next_free == nullptr)
				return nullptr;

			//This should be free
			auto old_free = m_pool->next_free;
			//This might be nullptr
			auto new_free = old_free->next_free;

			m_pool->next_free = new_free;

			old_free->used = true;
			old_free->next_free = nullptr;

			return &old_free->element;
		}

		/// <summary>
		/// Try freeing a single element.
		/// The list of free nodes is always 
		/// ordered (even though this is not necessary)
		/// </summary>
		/// <param name="ptr">The pointer to the base of the element</param>
		/// <returns>Whether the function was succesfull or not</returns>
		bool Free(Ty* ptr) {
			void* pool_end = m_pool + m_size;
			void* _ptr = std::bit_cast<void*>(ptr);

			if (_ptr < m_pool || _ptr >= pool_end) {
				return false;
			}

			auto node_base = std::bit_cast<ArenaNode<Ty>*>(ptr);

			if (!node_base->used)
				return false;

			if (node_base != m_pool) {
				auto curr_next_free = m_pool->next_free;

				node_base->next_free = curr_next_free;
				m_pool->next_free = node_base;
			}

			node_base->used = false;

			return true;
		}

		FORCE_INLINE ArenaNode<Ty> const* GetPool() const {
			return m_pool;
		}
	};
}