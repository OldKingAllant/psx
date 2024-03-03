#pragma once

#include <common/Defs.hpp>
#include <common/Macros.hpp>
#include <common/FixedSizeArenaAllocator.hpp>

#include <asmjit/asmjit.h>

#include "JitBlock.hpp"

namespace psx::cpu {
	/// <summary>
	/// Keeps track of the compiled blocks,
	/// providing APIs to retrieve blocks
	/// and invalidate blocks
	/// </summary>
	class CodeCache {
	public:
		/// <summary>
		/// 
		/// </summary>
		/// <param name="addrspace_block_size">Divide the address space in N blocks of this size</param>
		/// <param name="max_blocks">Maximum number of blocks in the cache</param>
		CodeCache(u64 addrspace_block_size, u64 max_blocks);

		/// <summary>
		/// Add a block to the code cache, 
		/// if the base guest address is not
		/// already in the cache
		/// </summary>
		/// <param name="block"></param>
		/// <returns>Whether the block was added successfully or not</returns>
		bool AddBlock(JitBlock const& block, asmjit::CodeHolder& holder);

		/// <summary>
		/// Invalidates all the blocks which contain/are contained in
		/// [guest_base, guest_base + size]
		/// </summary>
		/// <param name="guest_base">Base address of the area to invalidate</param>
		/// <param name="size">Extent of the area</param>
		/// <returns>Number of invalidated blocks</returns>
		u64 InvalidateBlocks(u64 guest_base, u64 size);

		/// <summary>
		/// Performs a search in the cache to find
		/// a block which starts at "guest_address".
		/// If "guest_address" is in the middle of a block,
		/// nullptr is returned anyway
		/// </summary>
		/// <param name="guest_address">Start of the block</param>
		/// <returns>Function pointer to jitted block</returns>
		
		JitBlock const* GetBlock(u64 guest_address) const;

		FORCE_INLINE u64 GetNumBlocks() const noexcept { return m_num_blocks; }
		FORCE_INLINE JitBlock** GetCachePtr() noexcept { return m_blocks; }
		FORCE_INLINE asmjit::JitRuntime const& GetRuntime() noexcept { return m_rt; }
		FORCE_INLINE u64 GetBlockSize() const noexcept { return m_addrspace_block_sz; }

		~CodeCache();

	private :
		FixedArenaAllocator<JitBlock> m_arena;
		JitBlock** m_blocks;
		asmjit::JitRuntime m_rt;
		u64 m_addrspace_blocks;
		u64 m_addrspace_block_sz;
		u64 m_num_blocks;
	};
}