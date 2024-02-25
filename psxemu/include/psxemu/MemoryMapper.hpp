#pragma once

#include <common/Defs.hpp>
#include <common/Macros.hpp>

#include <Windows.h>

#include <list>
#include <vector>
#include <optional>

namespace psx::memory {
	constexpr u64 PSX_ADDRESS_SPACE_SIZE = (u64)4 * 1024 * 1024 * 1024;

	enum class PageProtection {
		READ,
		WRITE,
		READ_WRITE
	};

	/*
	Describes a guest memory region:
	- guest_base -> Address inside the guest address space
	- extent -> Size of the region
	*/
	struct MemoryRegion {
		u64 guest_base;
		u64 extent;
	};

	/*
	Structure used for mapping and mirroring
	guest memory regions inside the host
	address space
	*/
	class MemoryMapper {
	public :
		//Returns the a pointer to the base of the emulated address space
		FORCE_INLINE u8* GetGuestBase() const noexcept { return m_guest_base; }

		//Translate guest to host (simply GetGuestBase() + offset)
		FORCE_INLINE u8* GuestToHost(u64 offset) const noexcept { return m_guest_base + offset; }

		FORCE_INLINE auto GetFreeRegions() const noexcept -> std::list<MemoryRegion> const& { return m_free_regions; }

		//This constructor will immediately allocate 4 GB of virtual memory
		//If the allocation fails, an exception is thrown
		MemoryMapper(std::optional<u8*> base);

		~MemoryMapper();

		/// Splits the allocated region at "region_start" with the
		/// requested length. 
		/// 
		/// Once the function succeeds, it wont be possible to
		/// coalesce the regions back together.
		/// 
		/// The function fails under these conditions:
		/// - region_start and region_start + region_extent are not contained in a free region
		/// - No free regions are present
		/// - region_start and/or region_extent are not aligned to a page boundary
		bool ReserveRegion(u64 region_start, u64 region_extent) noexcept;

		/// <summary>
		/// Maps an already reserved region inside the guest 
		/// address space.
		/// The function fails if the region has not been reserved
		/// or the provided parameters do not follow required
		/// alignment.
		/// 
		/// If the region has already been mapped (the base address
		/// is inside the vector), the previously mapped address
		/// is returned
		/// </summary>
		/// <param name="region_start">Guest address start</param>
		/// <param name="region_extent">Region size</param>
		/// <param name="memory_file_base">Offset inside the memory file</param>
		/// <param name="protect">Page protections</param>
		/// <returns>The host pointer to the region (nullptr if failed)</returns>
		u8* MapRegion(u64 region_start, u64 region_extent, u64 memory_file_base, PageProtection 
		protect) noexcept;

		/// <summary>
		/// Unmaps an already mapped region. Fails if the region 
		/// is not mapped in the guest address space
		/// </summary>
		/// <param name="host_ptr">Host pointer to base</param>
		/// <returns>Whether or not the function succeded</returns>
		bool UnmapRegion(u8* host_ptr) noexcept;

		/// <summary>
		/// Get system page granularity (4KB or 4MB)
		/// </summary>
		/// <returns></returns>
		FORCE_INLINE u64 GetPageGranularity() const noexcept { return m_page_granularity; }

	private :
		u8* m_guest_base;
		std::list<MemoryRegion> m_free_regions;
		std::vector<u8*> m_mapped_regions;

		HANDLE m_memory_file;
		u64 m_page_granularity;
	};
}