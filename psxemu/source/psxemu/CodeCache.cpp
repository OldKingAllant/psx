#include <psxemu/include/psxemu/CodeCache.hpp>

#include <stdexcept>

namespace psx::cpu {
	CodeCache::CodeCache(u64 addrspace_block_size, u64 max_blocks) :
		m_arena{max_blocks},
		m_blocks{nullptr},
		m_rt{},
		m_addrspace_blocks{},
		m_addrspace_block_sz{addrspace_block_size},
		m_num_blocks{}
	{
		if (memory::SEGMENT_SIZE % addrspace_block_size) {
			throw std::runtime_error("Invalid block size (SEGMENT_SIZE % BLOCK_SIZE != 0)");
		}

		m_addrspace_blocks = memory::SEGMENT_SIZE / addrspace_block_size;

		m_blocks = new JitBlock*[m_addrspace_blocks];

		std::memset(m_blocks, 0x0, sizeof(JitBlock*) * m_addrspace_blocks);
	}

	bool CodeCache::AddBlock(JitBlock const& _block, asmjit::CodeHolder& holder) {
		JitBlock block = _block;

		if (block.guest_base >= block.guest_end)
			return false;

		block.guest_base &= memory::SEGMENT_MASK;
		block.guest_end &= memory::SEGMENT_MASK;

		u64 base = block.guest_base;

		u64 list_index = base / m_addrspace_block_sz;
		JitBlock* list = m_blocks[list_index];

		JitBlock* new_block_ptr = m_arena.Alloc();

		if (new_block_ptr == nullptr)
			return false;

		*new_block_ptr = block;

		if (list == nullptr) {
			m_blocks[list_index] = new_block_ptr;
		}
		else {
			JitBlock* curr_node = list;

			while (curr_node->next != nullptr) {
				if (curr_node->guest_base == base)
					return false;

				curr_node = curr_node->next;
			}

			curr_node->next = new_block_ptr;
		}

		if (m_rt.add(&new_block_ptr->host_fn, &holder))
			return false;

		m_num_blocks++;

		return true;
	}

	JitBlock const* CodeCache::GetBlock(u64 guest_address) const {
		u64 base = guest_address & memory::SEGMENT_MASK;

		u64 list_index = base / m_addrspace_block_sz;
		JitBlock* list = m_blocks[list_index];

		if (list == nullptr)
			return nullptr;

		if (list->guest_base == guest_address) {
			return list;
		}

		JitBlock* curr_block = list;

		while (curr_block != nullptr && curr_block->guest_base != guest_address)
			curr_block = curr_block->next;

		return curr_block;
	}

	u64 CodeCache::InvalidateBlocks(u64 guest_base, u64 size) {
		u64 base = guest_base & memory::SEGMENT_MASK;

		u64 list_index = base / m_addrspace_block_sz;
		JitBlock* list = m_blocks[list_index];

		if (list == nullptr)
			return 0;

		//Exclusive [base, base+size)
		u64 block_end = base + size;
		u64 invalidated_blocks = 0;
		
		JitBlock* curr_block = list;
		JitBlock* prev_block = curr_block;

		while (curr_block != nullptr) {
			if ((curr_block->guest_base <= guest_base && curr_block->guest_end >= block_end) //Contains
				|| (block_end > curr_block->guest_base && block_end <= curr_block->guest_end) //Contained (partly or not)
				|| (guest_base >= curr_block->guest_base && guest_base <= curr_block->guest_end)) {
				JitBlock* block_to_free = curr_block;

				if (curr_block == m_blocks[list_index]) {
					m_blocks[list_index] = m_blocks[list_index]->next;
					prev_block = m_blocks[list_index];
				}
				else {
					prev_block->next = curr_block->next;
				}

				curr_block = curr_block->next;

				invalidated_blocks++;

				m_rt.release(block_to_free->host_fn);
				m_arena.Free(block_to_free);
			}
			else {
				prev_block = curr_block;
				curr_block = curr_block->next;
			}
		}

		m_num_blocks -= invalidated_blocks;

		return invalidated_blocks;
	}

	CodeCache::~CodeCache() {
		if (m_blocks) {
			//Deleting values referenced by the pointers
			//in the array is a bad idea, since
			//they are managed by the pool
			delete[] m_blocks;
			m_blocks = nullptr;
		}
	}
}