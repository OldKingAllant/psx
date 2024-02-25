#include <psxemu/include/psxemu/MemoryMapper.hpp>

#include <Windows.h>

#include <stdexcept>
#include <bit>
#include <algorithm>

namespace psx::memory {
	MemoryMapper::MemoryMapper(std::optional<u8*> base) :
		m_guest_base(nullptr), m_free_regions{}, m_mapped_regions{},
		m_memory_file{}, m_page_granularity{} {
		PVOID wanted_base_address = base.value_or(nullptr);

		DWORD size_high = (region_sizes::PSX_EFFECTIVE_MEMORY_SIZE >> 32) & UINT32_MAX;
		DWORD size_low = region_sizes::PSX_EFFECTIVE_MEMORY_SIZE & UINT32_MAX;

		m_memory_file = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
			size_high, size_low, NULL);

		if (m_memory_file == NULL) {
			throw std::runtime_error("Memory file creation failed");
		}

		PVOID base_address = VirtualAlloc2(GetCurrentProcess(),
			wanted_base_address, PSX_ADDRESS_SPACE_SIZE, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER,
			PAGE_NOACCESS, NULL, 0);

		if (base_address == nullptr) {
			throw std::runtime_error("Guest address space allocation failed");
		}

		MemoryRegion region{};

		region.guest_base = 0x0;
		region.extent = PSX_ADDRESS_SPACE_SIZE;

		m_guest_base = std::bit_cast<uint8_t*>( base_address );

		m_free_regions.push_back(region);

		SYSTEM_INFO info{};

		GetSystemInfo(&info);

		m_page_granularity = info.dwPageSize;
	}

	MemoryMapper::~MemoryMapper() {
		for (auto const& region : m_free_regions) {
			VirtualFree(m_guest_base + region.guest_base, 0, MEM_RELEASE);
		}

		for (auto ptr : m_mapped_regions) {
			UnmapViewOfFile(ptr);
		}

		if (m_memory_file)
			CloseHandle(m_memory_file);
	}

	bool MemoryMapper::ReserveRegion(u64 region_start, u64 region_extent) noexcept {
		auto region_ptr = std::find_if(m_free_regions.cbegin(), m_free_regions.cend(),
			[region_start, region_extent](MemoryRegion const& region) {
				return region.guest_base <= region_start &&
					region_start + region_extent <= (region.guest_base + region.extent);
			});

		if (region_start % m_page_granularity || region_extent % m_page_granularity) {
			return false;
		}

		if (region_ptr == m_free_regions.cend()) {
			return false;
		}

		MemoryRegion old_region = *region_ptr;

		m_free_regions.erase(region_ptr);

		u8* effective_host_address = m_guest_base + region_start;

		if (region_start == old_region.guest_base) {
			if (region_extent != old_region.extent) {
				u64 new_base = region_start + region_extent;
				u64 new_extent = old_region.extent - region_extent;

				m_free_regions.push_back(MemoryRegion{ .guest_base= new_base, .extent= new_extent });
			}
		}
		else if (region_start + region_extent == old_region.guest_base + old_region.extent) {
			u64 new_base = old_region.guest_base;
			u64 new_extent = region_start - old_region.guest_base;

			m_free_regions.push_back(MemoryRegion{ .guest_base = new_base, .extent = new_extent });
		}
		else {
			u64 base_left = old_region.guest_base;
			u64 extent_left = region_start - old_region.guest_base;

			u64 base_right = region_start + region_extent;
			u64 extent_right = (old_region.guest_base + old_region.extent) - base_right;

			m_free_regions.push_back(MemoryRegion{ .guest_base = base_left, .extent = extent_left });
			m_free_regions.push_back(MemoryRegion{ .guest_base = base_right, .extent = extent_right });
		}

		return VirtualFree(effective_host_address, region_extent, MEM_PRESERVE_PLACEHOLDER | MEM_RELEASE);
	}

	u8* MemoryMapper::MapRegion(u64 region_start, u64 region_extent, u64 memory_file_base, PageProtection protect) noexcept {
		if (region_start >= PSX_ADDRESS_SPACE_SIZE || region_start + region_extent >= PSX_ADDRESS_SPACE_SIZE) {
			return nullptr;
		}

		auto region_ptr = std::find_if(m_free_regions.cbegin(), m_free_regions.cend(),
			[region_start, region_extent](MemoryRegion const& region) {
				return region.guest_base <= region_start &&
					region_start + region_extent <= (region.guest_base + region.extent);
			});

		if (region_ptr != m_free_regions.cend()) {
			//Region has not been reserved!
			return nullptr;
		}

		if (memory_file_base >= region_sizes::PSX_EFFECTIVE_MEMORY_SIZE) {
			//Base is outside of the memory file
			return nullptr;
		}

		u8* effective_host_base = m_guest_base + region_start;

		auto present = std::find(m_mapped_regions.cbegin(), m_mapped_regions.cend(), 
			effective_host_base);

		if (present != m_mapped_regions.cend()) {
			return *present;
		}

		ULONG page_protect = 0;

		switch (protect)
		{
		case psx::memory::PageProtection::READ:
			page_protect = PAGE_READONLY;
			break;
		case psx::memory::PageProtection::WRITE:
			page_protect = PAGE_WRITECOPY;
			break;
		case psx::memory::PageProtection::READ_WRITE:
			page_protect = PAGE_READWRITE;
			break;
		default:
			break;
		}

		//Perform map
		u8* mapped_region = std::bit_cast<u8*>(
			MapViewOfFile3(m_memory_file, GetCurrentProcess(), effective_host_base, memory_file_base,
				region_extent, MEM_REPLACE_PLACEHOLDER, page_protect, NULL, 0)
		);

		if (mapped_region == nullptr) {
			return nullptr;
		}

		m_mapped_regions.push_back(mapped_region);

		return mapped_region;
	}

	bool MemoryMapper::UnmapRegion(u8* host_ptr) noexcept {
		auto present = std::find(m_mapped_regions.cbegin(), m_mapped_regions.cend(), host_ptr);

		if (present == m_mapped_regions.cend()) {
			return false;
		}

		m_mapped_regions.erase(present);

		return UnmapViewOfFile2(GetCurrentProcess(), host_ptr, MEM_PRESERVE_PLACEHOLDER);
	}
}